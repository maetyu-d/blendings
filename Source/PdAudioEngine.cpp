#include "PdAudioEngine.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cmath>
#include <cstdlib>

#if OTHERWARE_HAS_LIBPD
extern "C"
{
#include "z_libpd.h"
}
#endif

#ifndef OTHERWARE_PD_EXTRA_ROOT
#define OTHERWARE_PD_EXTRA_ROOT ""
#endif

namespace
{
constexpr const char* stdPathPrefix = "std:";

std::atomic<int> midiOutputSmokeMask { 0 };

void markMidiOutputSmokeBit (int bit) noexcept
{
    midiOutputSmokeMask.fetch_or (bit, std::memory_order_relaxed);
}

juce::File bundledPdExtraRoot()
{
    return juce::File (juce::String (OTHERWARE_PD_EXTRA_ROOT).unquoted());
}

bool parsePdFloatAtom (const juce::String& text, float& value)
{
    const auto trimmed = text.trim();

    if (trimmed.isEmpty())
        return false;

    const auto utf8 = trimmed.toStdString();
    char* end = nullptr;
    const auto parsed = std::strtof (utf8.c_str(), &end);

    if (end == utf8.c_str() || *end != '\0' || ! std::isfinite (parsed))
        return false;

    value = parsed;
    return true;
}

juce::String unescapePdAtom (const juce::String& atom)
{
    juce::String result;
    bool escaping = false;

    for (int i = 0; i < atom.length(); ++i)
    {
        const auto c = atom[i];

        if (escaping)
        {
            result << c;
            escaping = false;
            continue;
        }

        if (c == '\\')
        {
            escaping = true;
            continue;
        }

        result << c;
    }

    if (escaping)
        result << "\\";

    return result;
}

juce::String normalisedOutboundPdAtom (const juce::String& atom)
{
    return unescapePdAtom (atom.trim());
}

juce::StringArray pdAtoms (const juce::String& text)
{
    juce::StringArray atoms;
    juce::String current;
    bool escaping = false;

    for (int i = 0; i < text.length(); ++i)
    {
        const auto c = text[i];

        if (escaping)
        {
            current << "\\" << c;
            escaping = false;
            continue;
        }

        if (c == '\\')
        {
            escaping = true;
            continue;
        }

        if (juce::CharacterFunctions::isWhitespace (c))
        {
            if (current.isNotEmpty())
            {
                atoms.add (current);
                current.clear();
            }

            continue;
        }

        current << c;
    }

    if (escaping)
        current << "\\";

    if (current.isNotEmpty())
        atoms.add (current);

    return atoms;
}

juce::String stripPdFormatHints (const juce::String& text)
{
    bool escaping = false;

    for (int i = 0; i < text.length(); ++i)
    {
        const auto c = text[i];

        if (escaping)
        {
            escaping = false;
            continue;
        }

        if (c == '\\')
        {
            escaping = true;
            continue;
        }

        if (c == ',')
            return text.substring (0, i).trim();
    }

    return text.trim();
}
}

std::atomic<PdAudioEngine*> PdAudioEngine::activeMidiOutputEngine { nullptr };
std::atomic<PdAudioEngine*> PdAudioEngine::activeMessageOutputEngine { nullptr };

void PdAudioEngine::dispatchMessageOutput (const juce::String& receiver,
                                           const juce::String& selector,
                                           const juce::StringArray& atoms) const
{
    if (messageOutputCallback)
        messageOutputCallback (receiver, selector, atoms);
}

void PdAudioEngine::messageBangHook (const char* receiver)
{
    if (auto* engine = activeMessageOutputEngine.load (std::memory_order_acquire))
        engine->dispatchMessageOutput (juce::String::fromUTF8 (receiver != nullptr ? receiver : ""), "bang", {});
}

void PdAudioEngine::messageFloatHook (const char* receiver, float value)
{
    if (auto* engine = activeMessageOutputEngine.load (std::memory_order_acquire))
        engine->dispatchMessageOutput (juce::String::fromUTF8 (receiver != nullptr ? receiver : ""), "float", { juce::String (value) });
}

void PdAudioEngine::messageSymbolHook (const char* receiver, const char* symbol)
{
    if (auto* engine = activeMessageOutputEngine.load (std::memory_order_acquire))
        engine->dispatchMessageOutput (juce::String::fromUTF8 (receiver != nullptr ? receiver : ""), "symbol",
                                       { juce::String::fromUTF8 (symbol != nullptr ? symbol : "") });
}

#if OTHERWARE_HAS_LIBPD
namespace
{
juce::StringArray atomsFromPd (int argc, t_atom* argv)
{
    juce::StringArray atoms;
    for (int i = 0; i < argc; ++i)
    {
        if (libpd_is_float (argv + i))
            atoms.add (juce::String (libpd_get_float (argv + i)));
        else if (libpd_is_symbol (argv + i))
            atoms.add (juce::String::fromUTF8 (libpd_get_symbol (argv + i)));
    }
    return atoms;
}
}
#endif

void PdAudioEngine::messageListHook (const char* receiver, int argc, struct _atom* argv)
{
#if OTHERWARE_HAS_LIBPD
    if (auto* engine = activeMessageOutputEngine.load (std::memory_order_acquire))
        engine->dispatchMessageOutput (juce::String::fromUTF8 (receiver != nullptr ? receiver : ""), "list", atomsFromPd (argc, argv));
#else
    juce::ignoreUnused (receiver, argc, argv);
#endif
}

void PdAudioEngine::messageTypedHook (const char* receiver, const char* selector, int argc, struct _atom* argv)
{
#if OTHERWARE_HAS_LIBPD
    if (auto* engine = activeMessageOutputEngine.load (std::memory_order_acquire))
        engine->dispatchMessageOutput (juce::String::fromUTF8 (receiver != nullptr ? receiver : ""),
                                       juce::String::fromUTF8 (selector != nullptr ? selector : ""), atomsFromPd (argc, argv));
#else
    juce::ignoreUnused (receiver, selector, argc, argv);
#endif
}

void PdAudioEngine::dispatchMidiOutput (int port, const juce::MidiMessage& message) const
{
    if (midiOutputCallback)
        midiOutputCallback (port, message);
}

void PdAudioEngine::midiNoteOnHook (int channel, int pitch, int velocity)
{
    if (channel == 0 && pitch == 72 && velocity == 96) markMidiOutputSmokeBit (1 << 0);
    if (auto* engine = activeMidiOutputEngine.load (std::memory_order_acquire))
        engine->dispatchMidiOutput (channel >> 4,
                                    velocity > 0 ? juce::MidiMessage::noteOn ((channel & 0x0f) + 1, pitch, static_cast<juce::uint8> (velocity))
                                                 : juce::MidiMessage::noteOff ((channel & 0x0f) + 1, pitch));
}

void PdAudioEngine::midiControlChangeHook (int channel, int controller, int value)
{
    if (channel == 0 && controller == 7 && value == 101) markMidiOutputSmokeBit (1 << 1);
    if (auto* engine = activeMidiOutputEngine.load (std::memory_order_acquire))
        engine->dispatchMidiOutput (channel >> 4, juce::MidiMessage::controllerEvent ((channel & 0x0f) + 1, controller, value));
}

void PdAudioEngine::midiProgramChangeHook (int channel, int value)
{
    if (channel == 0 && value == 12) markMidiOutputSmokeBit (1 << 2);
    if (auto* engine = activeMidiOutputEngine.load (std::memory_order_acquire))
        engine->dispatchMidiOutput (channel >> 4, juce::MidiMessage::programChange ((channel & 0x0f) + 1, value));
}

void PdAudioEngine::midiPitchBendHook (int channel, int value)
{
    if (channel == 0 && value == 2048) markMidiOutputSmokeBit (1 << 3);
    if (auto* engine = activeMidiOutputEngine.load (std::memory_order_acquire))
        engine->dispatchMidiOutput (channel >> 4, juce::MidiMessage::pitchWheel ((channel & 0x0f) + 1, juce::jlimit (0, 16383, value + 8192)));
}

void PdAudioEngine::midiAftertouchHook (int channel, int value)
{
    if (channel == 0 && value == 74) markMidiOutputSmokeBit (1 << 4);
    if (auto* engine = activeMidiOutputEngine.load (std::memory_order_acquire))
        engine->dispatchMidiOutput (channel >> 4, juce::MidiMessage::channelPressureChange ((channel & 0x0f) + 1, value));
}

void PdAudioEngine::midiPolyAftertouchHook (int channel, int pitch, int value)
{
    if (channel == 0 && pitch == 65 && value == 88) markMidiOutputSmokeBit (1 << 5);
    if (auto* engine = activeMidiOutputEngine.load (std::memory_order_acquire))
        engine->dispatchMidiOutput (channel >> 4, juce::MidiMessage::aftertouchChange ((channel & 0x0f) + 1, pitch, value));
}

void PdAudioEngine::midiByteHook (int port, int byte)
{
    if (port == 0 && byte == 0x90) markMidiOutputSmokeBit (1 << 6);
    if (auto* engine = activeMidiOutputEngine.load (std::memory_order_acquire))
    {
        const auto rawByte = static_cast<juce::uint8> (byte & 0xff);
        engine->dispatchMidiOutput (port, juce::MidiMessage (&rawByte, 1, 0.0));
    }
}

PdAudioEngine::~PdAudioEngine()
{
    release();
}

bool PdAudioEngine::prepare (double sampleRate, int maxBlockSize, int outputChannels, int inputChannels)
{
    const juce::ScopedLock scopedLock (lock);

#if OTHERWARE_HAS_LIBPD
    setCurrentInstanceIfNeeded();

    for (auto& subscription : messageSubscriptions)
        if (subscription.second != nullptr)
            libpd_unbind (subscription.second);
    messageSubscriptions.clear();

    for (auto& entry : loadedPatches)
        closePatchLocked (entry.second);
#endif

    for (const auto& entry : loadedPatches)
        entry.second.file.deleteFile();

    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    currentOutputChannels = juce::jmax (2, outputChannels);
    currentInputChannels = juce::jmax (0, inputChannels);
    maximumBlockSize = juce::jmax (64, maxBlockSize);
    loadedPatches.clear();
    pdOutput.clear();
    pdInput.clear();
    pendingPdOutput.clear();
    pendingPdFrames = 0;
    lastError.clear();
    lastWarning.clear();
    renderedSamples = 0;

#if OTHERWARE_HAS_LIBPD
    libpd_init();
    pdInstance = libpd_main_instance();
    setCurrentInstanceIfNeeded();
    addBundledPdSearchPaths();
    activeMidiOutputEngine.store (this, std::memory_order_release);
    activeMessageOutputEngine.store (this, std::memory_order_release);
    libpd_set_banghook (messageBangHook);
    libpd_set_floathook (messageFloatHook);
    libpd_set_symbolhook (messageSymbolHook);
    libpd_set_listhook (messageListHook);
    libpd_set_messagehook (messageTypedHook);
    libpd_set_noteonhook (midiNoteOnHook);
    libpd_set_controlchangehook (midiControlChangeHook);
    libpd_set_programchangehook (midiProgramChangeHook);
    libpd_set_pitchbendhook (midiPitchBendHook);
    libpd_set_aftertouchhook (midiAftertouchHook);
    libpd_set_polyaftertouchhook (midiPolyAftertouchHook);
    libpd_set_midibytehook (midiByteHook);

    if (libpd_init_audio (currentInputChannels, currentOutputChannels, static_cast<int> (std::round (currentSampleRate))) != 0)
    {
        ready = false;
        lastError = "libpd audio init failed";
        return false;
    }

    libpd_start_message (1);
    libpd_add_float (1.0f);
    libpd_finish_message ("pd", "dsp");

    const auto blockSize = juce::jmax (1, libpd_blocksize());
    const auto ticks = juce::jmax (1, (maximumBlockSize + blockSize - 1) / blockSize);
    pdOutput.resize (static_cast<size_t> (ticks * blockSize * currentOutputChannels), 0.0f);

    ready = true;
    lastError.clear();
    lastWarning.clear();
    return true;
#else
    pdInstance = nullptr;
    ready = false;
    lastError = "libpd source not available";
    return false;
#endif
}

void PdAudioEngine::release() noexcept
{
    const juce::ScopedLock scopedLock (lock);

#if OTHERWARE_HAS_LIBPD
    setCurrentInstanceIfNeeded();

    if (pdInstance != nullptr)
    {
        libpd_start_message (1);
        libpd_add_float (0.0f);
        libpd_finish_message ("pd", "dsp");
    }

    for (auto& entry : loadedPatches)
        closePatchLocked (entry.second);

    if (activeMidiOutputEngine.load (std::memory_order_acquire) == this)
    {
        activeMidiOutputEngine.store (nullptr, std::memory_order_release);
        libpd_set_noteonhook (nullptr);
        libpd_set_controlchangehook (nullptr);
        libpd_set_programchangehook (nullptr);
        libpd_set_pitchbendhook (nullptr);
        libpd_set_aftertouchhook (nullptr);
        libpd_set_polyaftertouchhook (nullptr);
        libpd_set_midibytehook (nullptr);
    }
    for (auto& subscription : messageSubscriptions)
        if (subscription.second != nullptr)
            libpd_unbind (subscription.second);
    messageSubscriptions.clear();
    if (activeMessageOutputEngine.load (std::memory_order_acquire) == this)
    {
        activeMessageOutputEngine.store (nullptr, std::memory_order_release);
        libpd_set_banghook (nullptr);
        libpd_set_floathook (nullptr);
        libpd_set_symbolhook (nullptr);
        libpd_set_listhook (nullptr);
        libpd_set_messagehook (nullptr);
    }
#endif

    for (const auto& entry : loadedPatches)
        entry.second.file.deleteFile();

    loadedPatches.clear();
    pdOutput.clear();
    pdInput.clear();
    pendingPdOutput.clear();
    pendingPdFrames = 0;
    ready = false;
    pdInstance = nullptr;
    lastWarning.clear();
    nextPatchId = 1;
    renderedSamples = 0;
}

void PdAudioEngine::stopAllPatches()
{
    const juce::ScopedLock scopedLock (lock);

#if OTHERWARE_HAS_LIBPD
    setCurrentInstanceIfNeeded();

    for (auto& entry : loadedPatches)
        closePatchLocked (entry.second);
#endif

    for (const auto& entry : loadedPatches)
        entry.second.file.deleteFile();

    loadedPatches.clear();
    pdOutput.clear();
    pdInput.clear();
    pendingPdOutput.clear();
    pendingPdFrames = 0;
    lastWarning.clear();
}

void PdAudioEngine::renderAndAdd (juce::AudioBuffer<float>& output, const juce::AudioBuffer<float>* input)
{
#if OTHERWARE_HAS_LIBPD
    const juce::ScopedLock scopedLock (lock);

    if (! ready || loadedPatches.empty() || output.getNumSamples() <= 0 || output.getNumChannels() <= 0)
        return;

    setCurrentInstanceIfNeeded();
    unloadExpiredPatchesLocked();

    if (loadedPatches.empty())
        return;

    const auto samplesToCopy = output.getNumSamples();
    const auto blockSize = juce::jmax (1, libpd_blocksize());
    const auto channels = currentOutputChannels;

    while (pendingPdFrames < samplesToCopy)
    {
        const auto framesNeeded = samplesToCopy - pendingPdFrames;
        const auto ticks = juce::jmax (1, (framesNeeded + blockSize - 1) / blockSize);
        const auto renderedFrames = ticks * blockSize;
        const auto requiredSamples = static_cast<size_t> (renderedFrames * channels);
        const auto requiredInputSamples = static_cast<size_t> (renderedFrames * currentInputChannels);

        if (pdOutput.size() < requiredSamples)
            pdOutput.resize (requiredSamples, 0.0f);

        std::fill (pdOutput.begin(), pdOutput.begin() + static_cast<std::ptrdiff_t> (requiredSamples), 0.0f);

        const float* inputData = nullptr;
        if (currentInputChannels > 0)
        {
            if (pdInput.size() < requiredInputSamples)
                pdInput.resize (requiredInputSamples, 0.0f);

            std::fill (pdInput.begin(), pdInput.begin() + static_cast<std::ptrdiff_t> (requiredInputSamples), 0.0f);
            const auto inputOffset = juce::jmin (pendingPdFrames, input != nullptr ? input->getNumSamples() : 0);
            const auto inputFrames = input != nullptr ? juce::jmin (renderedFrames, input->getNumSamples() - inputOffset) : 0;
            const auto inputChannelsToCopy = input != nullptr ? juce::jmin (currentInputChannels, input->getNumChannels()) : 0;

            for (int frame = 0; frame < inputFrames; ++frame)
                for (int channel = 0; channel < inputChannelsToCopy; ++channel)
                    pdInput[static_cast<size_t> (frame * currentInputChannels + channel)] = input->getSample (channel, inputOffset + frame);

            inputData = pdInput.data();
        }

        if (libpd_process_float (ticks, inputData, pdOutput.data()) != 0)
        {
            lastError = "libpd render failed";
            return;
        }

        pendingPdOutput.insert (pendingPdOutput.end(),
                                pdOutput.begin(),
                                pdOutput.begin() + static_cast<std::ptrdiff_t> (requiredSamples));
        pendingPdFrames += renderedFrames;
    }

    const auto channelsToCopy = juce::jmin (output.getNumChannels(), channels);

    for (int channel = 0; channel < channelsToCopy; ++channel)
    {
        auto* destination = output.getWritePointer (channel);

        for (int sample = 0; sample < samplesToCopy; ++sample)
            destination[sample] += pendingPdOutput[static_cast<size_t> (sample * channels + channel)];
    }

    const auto consumedSamples = static_cast<size_t> (samplesToCopy * channels);
    pendingPdOutput.erase (pendingPdOutput.begin(),
                           pendingPdOutput.begin() + static_cast<std::ptrdiff_t> (consumedSamples));
    pendingPdFrames -= samplesToCopy;
    renderedSamples += samplesToCopy;
    unloadExpiredPatchesLocked();
#else
    juce::ignoreUnused (output);
#endif
}

bool PdAudioEngine::triggerPatch (const juce::String& patch, float durationSeconds, const juce::String& searchPath, float triggerValue)
{
    const auto trimmed = patch.trim();
    const auto trimmedSearchPath = searchPath.trim();

    if (trimmed.isEmpty())
        return false;

    const juce::ScopedLock scopedLock (lock);

    if (! ready)
        return false;

    setCurrentInstanceIfNeeded();

    auto* loadedPatch = findOrLoadPatchLocked (trimmed, trimmedSearchPath, durationSeconds);

    if (loadedPatch == nullptr)
        return false;

#if OTHERWARE_HAS_LIBPD
    // Some valid Pd patches are loadbang-driven and do not expose the synthetic
    // trigger receiver. Loading them successfully is enough; bang when possible.
    if (triggerValue >= 0.0f)
        juce::ignoreUnused (libpd_float (loadedPatch->receiver.toRawUTF8(), triggerValue));
    else
        juce::ignoreUnused (libpd_bang (loadedPatch->receiver.toRawUTF8()));
    return true;
#else
    juce::ignoreUnused (durationSeconds, triggerValue);
    return false;
#endif
}

bool PdAudioEngine::sendGuiValue (const juce::String& patch,
                                  const juce::String& receiver,
                                  float value,
                                  bool bangOnly,
                                  float durationSeconds,
                                  const juce::String& searchPath)
{
    const auto trimmed = patch.trim();
    const auto target = receiver.trim();
    const auto trimmedSearchPath = searchPath.trim();

    if (trimmed.isEmpty() || target.isEmpty())
        return false;

    const juce::ScopedLock scopedLock (lock);

    if (! ready)
        return false;

    setCurrentInstanceIfNeeded();

    if (findOrLoadPatchLocked (trimmed, trimmedSearchPath, durationSeconds) == nullptr)
        return false;

#if OTHERWARE_HAS_LIBPD
    if (bangOnly)
        return libpd_bang (target.toRawUTF8()) == 0;

    return libpd_float (target.toRawUTF8(), value) == 0;
#else
    juce::ignoreUnused (value, bangOnly, durationSeconds);
    return false;
#endif
}

bool PdAudioEngine::sendMessage (const juce::String& patch,
                                 const juce::String& receiver,
                                 const juce::String& selector,
                                 const juce::StringArray& atoms,
                                 float durationSeconds,
                                 const juce::String& searchPath)
{
    const auto trimmed = patch.trim();
    const auto target = receiver.trim();
    const auto trimmedSelector = selector.trim();
    const auto trimmedSearchPath = searchPath.trim();

    if (trimmed.isEmpty() || target.isEmpty())
        return false;

    const juce::ScopedLock scopedLock (lock);

    if (! ready)
        return false;

    setCurrentInstanceIfNeeded();

    if (findOrLoadPatchLocked (trimmed, trimmedSearchPath, durationSeconds) == nullptr)
        return false;

#if OTHERWARE_HAS_LIBPD
    if (trimmedSelector == "bang" && atoms.isEmpty())
        return libpd_bang (target.toRawUTF8()) == 0;

    if ((trimmedSelector == "float" || trimmedSelector.isEmpty()) && atoms.size() == 1)
    {
        float floatValue = 0.0f;

        if (parsePdFloatAtom (atoms[0], floatValue))
            return libpd_float (target.toRawUTF8(), floatValue) == 0;
    }

    if (trimmedSelector == "symbol" && atoms.size() == 1)
    {
        const auto atom = normalisedOutboundPdAtom (atoms[0]);

        if (atom.isNotEmpty())
            return libpd_symbol (target.toRawUTF8(), atom.toRawUTF8()) == 0;
    }

    libpd_start_message (atoms.size());

    for (const auto& rawAtom : atoms)
    {
        const auto atom = normalisedOutboundPdAtom (rawAtom);
        float floatValue = 0.0f;

        if (parsePdFloatAtom (atom, floatValue))
            libpd_add_float (floatValue);
        else
            libpd_add_symbol (atom.toRawUTF8());
    }

    return libpd_finish_message (target.toRawUTF8(),
                                 trimmedSelector.isNotEmpty() ? trimmedSelector.toRawUTF8() : "list") == 0;
#else
    juce::ignoreUnused (atoms, durationSeconds);
    return false;
#endif
}

bool PdAudioEngine::sendMidiSmokeEvents()
{
    const juce::ScopedLock scopedLock (lock);

    if (! ready)
        return false;

#if OTHERWARE_HAS_LIBPD
    setCurrentInstanceIfNeeded();

    return libpd_noteon (0, 72, 96) == 0
        && libpd_controlchange (0, 7, 101) == 0
        && libpd_programchange (0, 12) == 0
        && libpd_pitchbend (0, 2048) == 0
        && libpd_aftertouch (0, 74) == 0
        && libpd_polyaftertouch (0, 65, 88) == 0
        && libpd_sysrealtime (0, 0xf8) == 0
        && libpd_sysex (0, 0x7d) == 0;
#else
    return false;
#endif
}

bool PdAudioEngine::sendMidiMessage (const juce::MidiMessage& message, int port)
{
    const juce::ScopedLock scopedLock (lock);

#if OTHERWARE_HAS_LIBPD
    if (! ready)
        return false;

    setCurrentInstanceIfNeeded();
    const auto channel = (juce::jlimit (0, 0x0fff, port) << 4) | juce::jlimit (0, 15, message.getChannel() - 1);

    if (message.isNoteOnOrOff())
        return libpd_noteon (channel, message.getNoteNumber(), message.isNoteOn() ? message.getVelocity() : 0) == 0;

    if (message.isController())
        return libpd_controlchange (channel, message.getControllerNumber(), message.getControllerValue()) == 0;

    if (message.isProgramChange())
        return libpd_programchange (channel, message.getProgramChangeNumber()) == 0;

    if (message.isPitchWheel())
        return libpd_pitchbend (channel, message.getPitchWheelValue() - 8192) == 0;

    if (message.isAftertouch())
        return libpd_polyaftertouch (channel, message.getNoteNumber(), message.getAfterTouchValue()) == 0;

    if (message.isChannelPressure())
        return libpd_aftertouch (channel, message.getChannelPressureValue()) == 0;

    if (message.isSysEx())
    {
        auto ok = true;
        const auto* data = message.getSysExData();
        for (int i = 0; i < message.getSysExDataSize(); ++i)
            ok = libpd_sysex (port, data[i]) == 0 && ok;
        return ok;
    }

    const auto* raw = message.getRawData();
    if (raw != nullptr && message.getRawDataSize() == 1 && raw[0] >= 0xf8)
        return libpd_sysrealtime (port, raw[0]) == 0;

    return false;
#else
    juce::ignoreUnused (message);
    return false;
#endif
}

bool PdAudioEngine::runMidiOutputSmoke()
{
    const juce::ScopedLock scopedLock (lock);

    if (! ready)
        return false;

#if OTHERWARE_HAS_LIBPD
    setCurrentInstanceIfNeeded();
    midiOutputSmokeMask.store (0, std::memory_order_relaxed);

    LoadedPatch loadedPatch;
    const juce::String patch =
        "#N canvas 120 120 760 520 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X obj 48 82 t b b b b b b b b b b;\n"
        "#X msg 48 122 96;\n"
        "#X msg 104 122 72;\n"
        "#X obj 104 162 noteout 1;\n"
        "#X msg 204 122 101;\n"
        "#X obj 204 162 ctlout 7 1;\n"
        "#X msg 334 122 13;\n"
        "#X obj 334 162 pgmout 1;\n"
        "#X msg 452 122 2048;\n"
        "#X obj 452 162 bendout 1;\n"
        "#X msg 590 122 74;\n"
        "#X obj 590 162 touchout 1;\n"
        "#X msg 48 262 65;\n"
        "#X msg 104 262 88;\n"
        "#X obj 104 302 polytouchout 1;\n"
        "#X msg 284 262 144;\n"
        "#X obj 284 302 midiout 1;\n"
        "#X msg 420 262 72;\n"
        "#X obj 420 302 noteout 17;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 9 2 0;\n"
        "#X connect 1 8 3 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 1 7 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 1 6 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 1 5 9 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 1 4 11 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 1 3 13 0;\n"
        "#X connect 1 2 14 0;\n"
        "#X connect 13 0 15 1;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 1 1 16 0;\n"
        "#X connect 16 0 17 0;\n"
        "#X connect 1 0 18 0;\n"
        "#X connect 2 0 19 1;\n"
        "#X connect 18 0 19 0;\n";

    const auto loaded = loadPatchLocked (patch, {}, loadedPatch);
    closePatchLocked (loadedPatch);

    const auto mask = midiOutputSmokeMask.load (std::memory_order_relaxed);

    if (! loaded || mask != ((1 << 7) - 1))
        lastWarning = "MIDI output smoke mask " + juce::String (mask);

    return loaded && mask == ((1 << 7) - 1);
#else
    return false;
#endif
}

void PdAudioEngine::setMidiOutputCallback (std::function<void(int, const juce::MidiMessage&)> callback)
{
    midiOutputCallback = std::move (callback);
}

void PdAudioEngine::setMessageOutputCallback (std::function<void(const juce::String&, const juce::String&, const juce::StringArray&)> callback)
{
    const juce::ScopedLock scopedLock (lock);
    messageOutputCallback = std::move (callback);
}

void PdAudioEngine::setMessageSubscriptions (const juce::StringArray& receivers)
{
    const juce::ScopedLock scopedLock (lock);
#if OTHERWARE_HAS_LIBPD
    setCurrentInstanceIfNeeded();
    auto uniqueReceivers = receivers;
    uniqueReceivers.removeEmptyStrings();
    uniqueReceivers.removeDuplicates (false);
    uniqueReceivers.sort (false);
    juce::StringArray existingReceivers;
    for (const auto& subscription : messageSubscriptions)
        existingReceivers.add (subscription.first);
    existingReceivers.sort (false);
    if (ready && uniqueReceivers == existingReceivers)
        return;
    for (auto& subscription : messageSubscriptions)
        if (subscription.second != nullptr)
            libpd_unbind (subscription.second);
    messageSubscriptions.clear();
    if (! ready)
        return;
    for (const auto& receiver : uniqueReceivers)
        if (auto* binding = libpd_bind (receiver.toRawUTF8()))
            messageSubscriptions.emplace (receiver, binding);
#else
    juce::ignoreUnused (receivers);
#endif
}

int PdAudioEngine::getPatchDollarZero (const juce::String& patch, const juce::String& searchPath)
{
    const juce::ScopedLock scopedLock (lock);
#if OTHERWARE_HAS_LIBPD
    const auto patchKey = patch.trim() + "\n@path=" + searchPath.trim();
    const auto existing = loadedPatches.find (patchKey);
    return existing != loadedPatches.end() ? existing->second.dollarZero : 0;
#else
    juce::ignoreUnused (patch, searchPath);
    return 0;
#endif
}

std::vector<PdAudioEngine::ArraySnapshot> PdAudioEngine::readArrays (const juce::String& patch,
                                                                     const juce::StringArray& names,
                                                                     const juce::String& searchPath,
                                                                     int maximumSamples)
{
    const juce::ScopedLock scopedLock (lock);
    std::vector<ArraySnapshot> snapshots;
#if OTHERWARE_HAS_LIBPD
    if (! ready || maximumSamples <= 0)
        return snapshots;

    const auto patchKey = patch.trim() + "\n@path=" + searchPath.trim();
    if (loadedPatches.find (patchKey) == loadedPatches.end())
        return snapshots;

    setCurrentInstanceIfNeeded();
    auto uniqueNames = names;
    uniqueNames.removeEmptyStrings();
    uniqueNames.removeDuplicates (false);

    for (const auto& name : uniqueNames)
    {
        const auto totalSize = libpd_arraysize (name.toRawUTF8());
        if (totalSize < 0)
            continue;

        ArraySnapshot snapshot;
        snapshot.name = name;
        snapshot.totalSize = totalSize;
        snapshot.values.resize (static_cast<std::size_t> (juce::jmin (totalSize, maximumSamples)));

        if (! snapshot.values.empty()
            && libpd_read_array (snapshot.values.data(), name.toRawUTF8(), 0, static_cast<int> (snapshot.values.size())) != 0)
            continue;

        snapshots.push_back (std::move (snapshot));
    }
#else
    juce::ignoreUnused (patch, names, searchPath, maximumSamples);
#endif
    return snapshots;
}

juce::String PdAudioEngine::getStatusText() const
{
#if OTHERWARE_HAS_LIBPD
    if (! ready)
        return "libpd offline: " + lastError;

    auto status = "libpd ready  /  " + juce::String (loadedPatches.size()) + " patches";

    if (lastWarning.isNotEmpty())
        status << "  /  " << lastWarning;

    return status;
#else
    return lastError.isNotEmpty() ? lastError : "libpd unavailable";
#endif
}

PdAudioEngine::LoadedPatch* PdAudioEngine::findOrLoadPatchLocked (const juce::String& patch,
                                                                  const juce::String& searchPath,
                                                                  float durationSeconds)
{
#if OTHERWARE_HAS_LIBPD
    const auto trimmed = patch.trim();
    const auto trimmedSearchPath = searchPath.trim();
    const auto patchKey = trimmed + "\n@path=" + trimmedSearchPath;
    auto existing = loadedPatches.find (patchKey);

    if (existing == loadedPatches.end())
    {
        LoadedPatch loadedPatch;

        if (! loadPatchLocked (trimmed, trimmedSearchPath, loadedPatch))
            return nullptr;

        existing = loadedPatches.emplace (patchKey, std::move (loadedPatch)).first;
    }

    if (durationSeconds >= 0.0f)
    {
        const auto durationSamples = static_cast<std::int64_t> (std::ceil (static_cast<double> (durationSeconds) * currentSampleRate));
        existing->second.stopSample = renderedSamples + juce::jmax<std::int64_t> (1, durationSamples);
    }
    else
    {
        existing->second.stopSample = -1;
    }

    return &existing->second;
#else
    juce::ignoreUnused (patch, searchPath, durationSeconds);
    return nullptr;
#endif
}

bool PdAudioEngine::loadPatchLocked (const juce::String& patch, const juce::String& searchPath, LoadedPatch& loadedPatch)
{
#if OTHERWARE_HAS_LIBPD
    setCurrentInstanceIfNeeded();

    loadedPatch.receiver = makePatchReceiver();
    loadedPatch.file = makePatchFile (loadedPatch.receiver);
    const auto searchDirectory = juce::File (searchPath);

    if (! loadedPatch.file.replaceWithText (namespaceTriggerReceiver (patch, loadedPatch.receiver)))
    {
        lastError = "Could not write Pd patch";
        return false;
    }

    addBundledPdSearchPaths();
    juce::StringArray unsupportedLibraries;

    for (const auto& libraryName : declaredLibraries (patch))
        if (! isBundledLibrary (libraryName))
            unsupportedLibraries.addIfNotAlreadyThere (libraryName);

    lastWarning = unsupportedLibraries.isEmpty()
                    ? juce::String {}
                    : "unsupported Pd libs: " + unsupportedLibraries.joinIntoString (", ");

    const auto declarationBaseDirectory = searchDirectory.isDirectory()
                                            ? searchDirectory
                                            : loadedPatch.file.getParentDirectory();

    if (searchDirectory.isDirectory())
        addSearchPathIfDirectory (searchDirectory);

    for (const auto& declaredPath : declaredSearchPaths (patch))
        addSearchPathIfDirectory (resolveDeclaredPath (declaredPath, declarationBaseDirectory));

    addSearchPathIfDirectory (loadedPatch.file.getParentDirectory());

    loadedPatch.handle = libpd_openfile (loadedPatch.file.getFileName().toRawUTF8(),
                                         loadedPatch.file.getParentDirectory().getFullPathName().toRawUTF8());

    if (loadedPatch.handle == nullptr)
    {
        lastError = "Could not open Pd patch";
        loadedPatch.file.deleteFile();
        return false;
    }

    loadedPatch.dollarZero = libpd_getdollarzero (loadedPatch.handle);

    lastError.clear();
    return true;
#else
    juce::ignoreUnused (patch, searchPath, loadedPatch);
    lastError = "libpd source not available";
    return false;
#endif
}

void PdAudioEngine::unloadExpiredPatchesLocked()
{
#if OTHERWARE_HAS_LIBPD
    auto it = loadedPatches.begin();
    bool expiredAnyPatch = false;

    while (it != loadedPatches.end())
    {
        if (it->second.stopSample >= 0 && renderedSamples >= it->second.stopSample)
        {
            closePatchLocked (it->second);
            it = loadedPatches.erase (it);
            expiredAnyPatch = true;
        }
        else
        {
            ++it;
        }
    }

    if (expiredAnyPatch)
    {
        pendingPdOutput.clear();
        pendingPdFrames = 0;
    }
#endif
}

void PdAudioEngine::closePatchLocked (LoadedPatch& loadedPatch)
{
#if OTHERWARE_HAS_LIBPD
    setCurrentInstanceIfNeeded();

    if (loadedPatch.handle != nullptr)
    {
        libpd_closefile (loadedPatch.handle);
        loadedPatch.handle = nullptr;
        loadedPatch.dollarZero = 0;
    }
#endif

    loadedPatch.file.deleteFile();
    loadedPatch.stopSample = -1;
}

void PdAudioEngine::setCurrentInstanceIfNeeded() const noexcept
{
#if OTHERWARE_HAS_LIBPD
    if (pdInstance != nullptr)
        libpd_set_instance (static_cast<t_pdinstance*> (pdInstance));
#endif
}

juce::String PdAudioEngine::makePatchReceiver()
{
    return "otherware_pd_trigger_" + juce::String (nextPatchId++);
}

juce::File PdAudioEngine::makePatchFile (const juce::String& receiver) const
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("otherware-libpd-patches");
    dir.createDirectory();
    return dir.getChildFile (receiver + ".pd");
}

void PdAudioEngine::addSearchPathIfDirectory (const juce::File& directory)
{
#if OTHERWARE_HAS_LIBPD
    if (directory.isDirectory())
        libpd_add_to_search_path (directory.getFullPathName().toRawUTF8());
#else
    juce::ignoreUnused (directory);
#endif
}

void PdAudioEngine::addBundledPdSearchPaths()
{
#if OTHERWARE_HAS_LIBPD
    const auto extraRoot = bundledPdExtraRoot();

    if (! extraRoot.isDirectory())
        return;

    addSearchPathIfDirectory (extraRoot);

    for (const auto& child : extraRoot.findChildFiles (juce::File::findDirectories, false))
        addSearchPathIfDirectory (child);
#endif
}

juce::String PdAudioEngine::namespaceTriggerReceiver (const juce::String& patch, const juce::String& receiver)
{
    juce::StringArray lines;
    lines.addLines (patch);

    for (auto& line : lines)
    {
        if (! line.trimStart().startsWith ("#X obj "))
            continue;

        const auto body = stripPdFormatHints (line.upToFirstOccurrenceOf (";", false, false).trim());
        auto tokens = pdAtoms (body);

        if (tokens.size() >= 6
            && (tokens[4] == "r" || tokens[4] == "receive")
            && (tokens[5] == "trigger" || tokens[5] == "trigger,"))
        {
            tokens.set (5, tokens[5].endsWithChar (',') ? receiver + "," : receiver);
            line = tokens.joinIntoString (" ") + ";";
        }
    }

    auto namespaced = lines.joinIntoString ("\n");

    if (! namespaced.endsWithChar ('\n'))
        namespaced << "\n";

    return namespaced;
}

juce::StringArray PdAudioEngine::declaredSearchPaths (const juce::String& patch)
{
    juce::StringArray paths;
    juce::StringArray lines;
    lines.addLines (patch);

    for (const auto& rawLine : lines)
    {
        const auto line = rawLine.trim();

        if (! line.startsWith ("#X declare ") && ! line.startsWith ("#X obj "))
            continue;

        const auto body = stripPdFormatHints (line.upToFirstOccurrenceOf (";", false, false).trim());
        const auto tokens = pdAtoms (body);
        const auto firstDeclareToken = line.startsWith ("#X declare ") ? 2 : 4;

        if (line.startsWith ("#X obj ") && (tokens.size() <= 4 || tokens[4] != "declare"))
            continue;

        for (int i = firstDeclareToken; i < tokens.size(); ++i)
        {
            const auto token = tokens[i];

            if ((token == "-path" || token == "-stdpath") && i + 1 < tokens.size())
            {
                const auto path = unescapePdAtom (tokens[++i]).unquoted().trim();

                if (path.isNotEmpty())
                    paths.addIfNotAlreadyThere (token == "-stdpath" ? juce::String (stdPathPrefix) + path : path);
            }
        }
    }

    return paths;
}

juce::StringArray PdAudioEngine::declaredLibraries (const juce::String& patch)
{
    juce::StringArray libraries;
    juce::StringArray lines;
    lines.addLines (patch);

    for (const auto& rawLine : lines)
    {
        const auto line = rawLine.trim();

        if (! line.startsWith ("#X declare ") && ! line.startsWith ("#X obj "))
            continue;

        const auto body = line.upToFirstOccurrenceOf (";", false, false).trim();
        const auto tokens = pdAtoms (body);
        const auto firstDeclareToken = line.startsWith ("#X declare ") ? 2 : 4;

        if (line.startsWith ("#X obj ") && (tokens.size() <= 4 || tokens[4] != "declare"))
            continue;

        for (int i = firstDeclareToken; i < tokens.size(); ++i)
        {
            const auto token = tokens[i];

            if ((token == "-lib" || token == "-stdlib") && i + 1 < tokens.size())
            {
                const auto libraryName = unescapePdAtom (tokens[++i]).unquoted().trim();

                if (libraryName.isNotEmpty())
                    libraries.addIfNotAlreadyThere (libraryName);
            }
        }
    }

    return libraries;
}

bool PdAudioEngine::isBundledLibrary (const juce::String& libraryName)
{
    const auto name = libraryName.trim();
    static const char* const bundledLibraries[] = {
        "bob~", "bonk~", "choice", "fiddle", "fiddle~", "loop~", "lrshift~",
        "pd~", "pique", "sigmund~", "stdout", "rev1~", "rev2~", "rev3~",
        "hilbert~", "complex-mod~", "output~"
    };

    for (const auto* bundled : bundledLibraries)
        if (name == bundled)
            return true;

    return false;
}

juce::File PdAudioEngine::resolveDeclaredPath (const juce::String& path, const juce::File& baseDirectory)
{
    if (path.startsWith (stdPathPrefix))
    {
        const auto standardPath = path.fromFirstOccurrenceOf (stdPathPrefix, false, false);

        if (juce::File::isAbsolutePath (standardPath))
            return juce::File (standardPath);

        const auto extraRoot = bundledPdExtraRoot();
        auto resolved = extraRoot.getChildFile (standardPath);

        if (resolved.isDirectory())
            return resolved;

        return extraRoot.getParentDirectory().getChildFile (standardPath);
    }

    const auto declared = juce::File (path);

    if (juce::File::isAbsolutePath (path))
        return declared;

    if (path.startsWith ("./"))
        return baseDirectory.getChildFile (path.fromFirstOccurrenceOf ("./", false, false));

    return baseDirectory.getChildFile (path);
}
