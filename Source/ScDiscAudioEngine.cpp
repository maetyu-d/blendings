#include "ScDiscAudioEngine.h"

#include "GridInterpreter.h"
#include "ScSheetFormula.h"
#include "ScSheetMath.h"
#include "ScSheetScore.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>

namespace
{
const juce::StringArray soundPalette { "pluck", "bell", "fm", "grain", "string", "metal", "pad", "choir" };

int noteFor (const DiscAudioTrigger& trigger, int elementIndex) noexcept
{
    if (trigger.midiNote >= 0)
        return juce::jlimit (0, 127, trigger.midiNote);

    const auto base = 48 + trigger.depth * 7 + (trigger.branchIndex % 5) * 2;
    const int degrees[] { 0, 2, 4, 7, 9, 12, 14, 16 };
    return juce::jlimit (28, 96, base + degrees[static_cast<size_t> (elementIndex % 8)]);
}

float velocityFor (const DiscAudioTrigger& trigger, int elementIndex) noexcept
{
    const auto layerLift = juce::jlimit (0.0f, 0.18f, static_cast<float> (trigger.depth) * 0.035f);
    const auto accent = elementIndex == 0 ? 0.12f : 0.0f;
    return juce::jlimit (0.12f, 0.86f, 0.34f + layerLift + accent);
}

float firstPdSignalFrequency (const juce::String& patch, const juce::String& objectName, float fallback)
{
    auto lines = juce::StringArray::fromLines (patch);

    for (auto line : lines)
    {
        if (! line.contains (objectName))
            continue;

        line = line.upToFirstOccurrenceOf (";", false, false);
        auto tokens = juce::StringArray::fromTokens (line, " \t", {});

        for (int i = 0; i < tokens.size() - 1; ++i)
        {
            if (tokens[i] == objectName)
            {
                const auto frequency = tokens[i + 1].getFloatValue();
                if (frequency > 0.0f)
                    return juce::jlimit (12.0f, 16000.0f, frequency);
            }
        }
    }

    return fallback;
}

float firstPdMessageDurationMs (const juce::String& patch, float fallback)
{
    auto longest = 0.0f;
    auto lines = juce::StringArray::fromLines (patch);

    for (auto line : lines)
    {
        if (! line.startsWith ("#X msg"))
            continue;

        line = line.upToFirstOccurrenceOf (";", false, false)
                   .retainCharacters ("0123456789. -,\t");
        auto tokens = juce::StringArray::fromTokens (line, " ,\t", {});

        for (auto token : tokens)
        {
            const auto value = token.getFloatValue();
            if (value > longest)
                longest = value;
        }
    }

    return longest > 0.0f ? juce::jlimit (30.0f, 30000.0f, longest) : fallback;
}

struct ScSheetPreparedEvent
{
    int sample = 0;
    VoiceType voice = VoiceType::tone;
    int midi = 60;
    float durationSeconds = 0.2f;
    float amp = 0.2f;
    float pan = 0.0f;
    float cutoff = 0.7f;
    int groupIndex = -1;
};

struct ScSheetRenderRegion
{
    const ScoreSection* section = nullptr;
    const ScoreSection* transitionSpec = nullptr;
    int sectionIndex = 0;
    double startBeat = 0.0;
    double endBeat = 0.0;
    double transitionStartBeat = 0.0;
    double transitionEndBeat = 0.0;
    bool outgoing = false;
};

double quantizeToBeat (double beat)
{
    return std::round (beat * 4.0) / 4.0;
}

double transitionMix (const ScoreSection& spec, double absoluteBeat, double startBeat, double endBeat)
{
    if (spec.transition == TransitionType::cut || spec.transitionBars <= 0.0)
        return 1.0;

    const auto transitionBeats = juce::jmax (0.001, endBeat - startBeat);
    const auto x = juce::jlimit (0.0, 1.0, (absoluteBeat - startBeat) / transitionBeats);

    if (spec.transition == TransitionType::equalPower)
        return std::sin (x * juce::MathConstants<double>::halfPi);

    return x;
}

double transitionWeight (const ScSheetRenderRegion& region, double absoluteBeat, int rowIndex, int step)
{
    if (region.transitionSpec == nullptr)
        return 1.0;

    const auto incoming = transitionMix (*region.transitionSpec, absoluteBeat,
                                         region.transitionStartBeat, region.transitionEndBeat);
    if (region.transitionSpec->transition == TransitionType::probabilistic)
    {
        const auto probability = juce::jlimit (0.0, 1.0, incoming * region.transitionSpec->transitionProbability);
        const auto value = 0.5 + 0.5 * ScoreMath::deterministicNoise (region.sectionIndex + 701, rowIndex, step);
        const auto chooseIncoming = value < probability;
        return region.outgoing ? (chooseIncoming ? 0.0 : 1.0) : (chooseIncoming ? 1.0 : 0.0);
    }

    const auto shaped = incoming * region.transitionSpec->transitionProbability;
    return region.outgoing ? 1.0 - shaped : shaped;
}

int scaleDegreeToMidi (const ScoreRow& row, int step, int steps, int voiceIndex)
{
    static constexpr std::array<int, 5> minorPent { 0, 3, 5, 7, 10 };
    static constexpr std::array<int, 7> dorian { 0, 2, 3, 5, 7, 9, 10 };
    static constexpr std::array<int, 7> aeolian { 0, 2, 3, 5, 7, 8, 10 };
    static constexpr std::array<int, 6> whole { 0, 2, 4, 6, 8, 10 };

    const auto span = juce::jmax (1, (row.midiHi - row.midiLo) / 2);
    const auto progress = static_cast<int> (std::round (ScoreMath::contourPosition (row, step, steps) * span));

    const int degree = progress + voiceIndex * 2;
    int semitone = degree;
    auto pick = [degree] (const auto& scale)
    {
        return (degree / static_cast<int> (scale.size())) * 12
             + scale[static_cast<size_t> (degree % static_cast<int> (scale.size()))];
    };

    if (row.scale == ScaleType::minorPentatonic) semitone = pick (minorPent);
    else if (row.scale == ScaleType::dorian) semitone = pick (dorian);
    else if (row.scale == ScaleType::aeolian) semitone = pick (aeolian);
    else if (row.scale == ScaleType::whole) semitone = pick (whole);

    auto midi = row.root + semitone + row.transpose;
    while (midi < row.midiLo) midi += 12;
    while (midi > row.midiHi) midi -= 12;
    return juce::jlimit (0, 127, midi);
}

juce::String instrumentForVoice (VoiceType voice)
{
    switch (voice)
    {
        case VoiceType::bass:     return "bass";
        case VoiceType::noise:    return "noise";
        case VoiceType::kick:     return "kick";
        case VoiceType::sample:   return "grain";
        case VoiceType::external: return "pad";
        case VoiceType::tone:     break;
    }

    return "tone";
}

std::vector<ScSheetRenderRegion> renderRegionsFor (const ScoreDocument& score)
{
    std::vector<ScSheetRenderRegion> regions;
    std::vector<size_t> mainRegionIndices;

    if (score.getNumSections() == 0)
    {
        static const ScoreSection fallbackSection;
        const auto startBeat = quantizeToBeat (fallbackSection.startBar * 4.0);
        const auto endBeat = quantizeToBeat ((fallbackSection.startBar + fallbackSection.bars) * 4.0);
        regions.push_back ({ &fallbackSection, nullptr, 0, startBeat, endBeat, startBeat, endBeat, false });
        return regions;
    }

    for (int i = 0; i < score.getNumSections(); ++i)
    {
        const auto& section = score.getSection (i);
        const auto sectionStartBeat = quantizeToBeat (section.startBar * 4.0);
        const auto sectionEndBeat = juce::jmax (sectionStartBeat + 1.0,
                                                quantizeToBeat ((section.startBar + section.bars) * 4.0));
        mainRegionIndices.push_back (regions.size());
        regions.push_back ({ &section, nullptr, i, sectionStartBeat, sectionEndBeat,
                             sectionStartBeat, sectionStartBeat, false });

        if (i > 0 && section.transition != TransitionType::cut && section.transitionBars > 0.0)
        {
            const auto previousStartBeat = quantizeToBeat (score.getSection (i - 1).startBar * 4.0);
            const auto transitionBeats = juce::jmax (0.25,
                                                     quantizeToBeat (juce::jmin (section.transitionBars,
                                                                                 section.bars) * 4.0));
            const auto transitionStartBeat = juce::jmax (previousStartBeat, sectionStartBeat - transitionBeats);

            if (mainRegionIndices.size() >= 2)
            {
                auto& previousMain = regions[mainRegionIndices[static_cast<size_t> (i - 1)]];
                previousMain.endBeat = juce::jmax (previousMain.startBeat,
                                                   juce::jmin (previousMain.endBeat, transitionStartBeat));
            }

            regions.push_back ({ &section, &section, i, transitionStartBeat, sectionStartBeat,
                                 transitionStartBeat, sectionStartBeat, false });
            regions.push_back ({ &score.getSection (i - 1), &section, i - 1, transitionStartBeat, sectionStartBeat,
                                 transitionStartBeat, sectionStartBeat, true });
        }
    }

    return regions;
}

std::vector<ScSheetPreparedEvent> buildScSheetEvents (const ScoreDocument& score, double bpm, double sampleRate)
{
    std::vector<ScSheetPreparedEvent> events;
    const auto samplesPerBeat = sampleRate * 60.0 / juce::jmax (20.0, bpm);

    juce::StringArray meterGroups;
    for (const auto& row : score.getRows())
        if (! meterGroups.contains (row.group, true) && meterGroups.size() < 16)
            meterGroups.add (row.group);
    meterGroups.sort (true);

    bool hasSolo = false;
    for (const auto& row : score.getRows())
        hasSolo = hasSolo || row.solo;

    for (const auto& region : renderRegionsFor (score))
    {
        const auto& section = *region.section;

        for (int rowIndex = 0; rowIndex < static_cast<int> (score.getRows().size()); ++rowIndex)
        {
            auto row = score.getRows()[static_cast<size_t> (rowIndex)];
            if (row.mute || (hasSolo && ! row.solo) || ! section.includesGroup (row.group))
                continue;

            const auto groupIndex = meterGroups.indexOf (row.group);
            row.transpose += section.transpose;
            row.density = juce::jlimit (0.0, 1.0, row.density * section.densityScale);
            row.amp = static_cast<float> (juce::jlimit (0.0, 1.0, static_cast<double> (row.amp) * section.ampScale));

            const auto rowStartBeat = row.startBar * 4.0;
            const auto lengthBeats = row.bars * 4.0;
            const auto activeStartBeat = juce::jmax (rowStartBeat, region.startBeat);
            const auto activeEndBeat = juce::jmin (rowStartBeat + lengthBeats, region.endBeat);
            if (activeEndBeat <= activeStartBeat)
                continue;

            const auto steps = juce::jmax (1, static_cast<int> (std::floor (lengthBeats / juce::jmax (0.001, row.stepBeats))));

            for (int step = 0; step < steps; ++step)
            {
                const auto localBeat = step * row.stepBeats;
                const auto absoluteBeat = rowStartBeat + localBeat;
                if (absoluteBeat < activeStartBeat || absoluteBeat >= activeEndBeat)
                    continue;

                const auto transitionAmp = transitionWeight (region, absoluteBeat, rowIndex, step);
                if (transitionAmp <= 0.001)
                    continue;

                ScoreFormula::Variables vars {
                    { "step", static_cast<double> (step) },
                    { "steps", static_cast<double> (steps) },
                    { "beat", localBeat },
                    { "bar", absoluteBeat / 4.0 },
                    { "phase", row.phase },
                    { "row", static_cast<double> (rowIndex) },
                    { "section", static_cast<double> (region.sectionIndex) },
                    { "transition", transitionAmp },
                    { "density", row.density },
                    { "amp", row.amp },
                    { "root", static_cast<double> (row.root) },
                    { "transpose", static_cast<double> (row.transpose) },
                    { "macro", ScoreMath::macroLevel (row, step, steps) },
                    { "phrase", ScoreMath::phraseLevel (row, localBeat) },
                    { "accent", ScoreMath::accentLevel (row, step) }
                };

                auto calcRow = row;
                calcRow.root = juce::jlimit (0, 127, static_cast<int> (std::round (ScoreFormula::evaluate (row.rootExpr, vars, row.root))));
                calcRow.density = juce::jlimit (0.0, 1.0, ScoreFormula::evaluate (row.densityExpr, vars, row.density));
                calcRow.amp = static_cast<float> (juce::jlimit (0.0, 1.0, ScoreFormula::evaluate (row.ampExpr, vars, row.amp)));
                calcRow.transpose = static_cast<int> (std::round (ScoreFormula::evaluate (row.transposeExpr, vars, row.transpose)));

                if (! ScoreMath::shouldTriggerStep (calcRow, step, steps))
                    continue;

                const auto structureAmp = ScoreMath::macroLevel (calcRow, step, steps)
                                        * ScoreMath::phraseLevel (calcRow, localBeat)
                                        * ScoreMath::accentLevel (calcRow, step);
                if (structureAmp <= 0.001)
                    continue;

                for (int p = 0; p < calcRow.poly; ++p)
                {
                    const auto jitter = ScoreMath::deterministicNoise (rowIndex + calcRow.root, step, p) * calcRow.drift;
                    const auto pitchDrift = static_cast<int> (std::round (ScoreMath::deterministicNoise (rowIndex + 91, step, p)
                                                                          * calcRow.drift * 7.0));
                    const auto eventBeat = absoluteBeat + jitter * calcRow.stepBeats * 0.45;
                    ScSheetPreparedEvent event;
                    event.sample = juce::jmax (0, static_cast<int> (std::round (eventBeat * samplesPerBeat)));
                    event.voice = calcRow.voice;
                    event.midi = juce::jlimit (0, 127,
                                               scaleDegreeToMidi (calcRow, step, steps, p)
                                               + ScoreMath::phrasePitchOffset (calcRow, localBeat)
                                               + pitchDrift);
                    event.durationSeconds = static_cast<float> (calcRow.noteBeats * 60.0 / bpm
                                                               * juce::jlimit (0.2, 4.0, calcRow.curve));
                    event.amp = static_cast<float> (juce::jlimit (0.0, 2.0, structureAmp * transitionAmp))
                              * calcRow.amp / std::sqrt (static_cast<float> (juce::jmax (1, calcRow.poly)));
                    const auto spreadOffset = (static_cast<float> (p) - (static_cast<float> (calcRow.poly - 1) * 0.5f)) * calcRow.spread;
                    event.pan = juce::jlimit (-1.0f, 1.0f, calcRow.pan + spreadOffset);
                    event.cutoff = calcRow.cutoff;
                    event.groupIndex = groupIndex;
                    events.push_back (event);
                }
            }
        }
    }

    std::sort (events.begin(), events.end(),
               [] (const auto& a, const auto& b) { return a.sample < b.sample; });
    return events;
}
}

void ScDiscAudioEngine::prepare (double sampleRate, int maximumBlockSize, int outputChannels, int inputChannels)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    currentOutputChannels = juce::jmax (2, outputChannels);
    renderedSamples.store (0);

    {
        const juce::ScopedLock lock (scheduledEventLock);
        scheduledEvents.clear();
    }

    ready = embeddedSc.prepare (currentSampleRate, juce::jmax (64, maximumBlockSize), currentOutputChannels);
    pdAudio.prepare (currentSampleRate, juce::jmax (64, maximumBlockSize), currentOutputChannels, inputChannels);

    if (ready)
    {
        embeddedSc.setMasterLevel (0.78f);
        embeddedSc.setTransport (currentBpm, tick, true);
    }
}

void ScDiscAudioEngine::release() noexcept
{
    pdAudio.release();
    embeddedSc.release();
    ready = false;
    loadedScPrograms.clear();
    loadedPdPrograms.clear();
    failedScPrograms.clear();
    failedPdPrograms.clear();
    {
        const juce::ScopedLock lock (scheduledEventLock);
        scheduledEvents.clear();
    }
    renderedSamples.store (0);
    nextProgramId = 1;
}

void ScDiscAudioEngine::render (juce::AudioBuffer<float>& output, const juce::AudioBuffer<float>* input)
{
    output.clear();

    if (ready)
    {
        enqueueDueScheduledEvents();
        embeddedSc.render (output);
        pdAudio.renderAndAdd (output, pdAudioInputMuted.load() ? nullptr : input);
        renderedSamples.fetch_add (output.getNumSamples());
    }
}

void ScDiscAudioEngine::trigger (const DiscAudioTrigger& triggerToPlay)
{
    triggerMany ({ triggerToPlay });
}

void ScDiscAudioEngine::triggerMidiNote (int midiNote, float velocity)
{
    if (! ready)
        return;
    gridcollider::EventFields fields;
    fields.timestampSeconds = static_cast<double> (tick) * 60.0 / currentBpm;
    fields.tick = tick++;
    fields.instrumentName = "tone";
    fields.pitch = juce::jlimit (0, 127, midiNote);
    fields.velocity = juce::jlimit (0.0f, 1.0f, velocity);
    fields.durationTicks = 1;
    fields.parameters["velocity"] = juce::String (fields.velocity, 3);
    fields.parameters["tempo"] = juce::String (currentBpm, 3);
    embeddedSc.setTransport (currentBpm, tick, true);
    embeddedSc.enqueue ({ gridcollider::InternalEvent { gridcollider::NoteEvent { fields } } });
}

void ScDiscAudioEngine::triggerMany (const std::vector<DiscAudioTrigger>& triggers)
{
    if (! ready || triggers.empty())
        return;

    std::vector<gridcollider::InternalEvent> events;
    bool scheduledSequence = false;

    for (const auto& triggerToPlay : triggers)
    {
        if (triggerToPlay.hasOrcaGridScore())
        {
            scheduledSequence = scheduleOrcaGrid (triggerToPlay) || scheduledSequence;
            continue;
        }

        if (triggerToPlay.hasScSheetScore())
        {
            scheduledSequence = scheduleScSheet (triggerToPlay) || scheduledSequence;
            continue;
        }

        if (triggerToPlay.hasScCode())
        {
            if (synthForProgram (triggerToPlay.scCode).isNotEmpty())
                events.push_back (makeScProgramEvent (triggerToPlay));

            continue;
        }

        if (triggerToPlay.hasPdPatch())
        {
            if (pdAudio.triggerPatch (triggerToPlay.pdPatch, triggerToPlay.pdDurationSeconds, triggerToPlay.pdSearchPath,
                                      static_cast<float> (triggerToPlay.midiNote)))
                scheduledSequence = true;
            else if (synthForPdPatch (triggerToPlay.pdPatch).isNotEmpty())
                events.push_back (makePdPatchEvent (triggerToPlay));

            continue;
        }

        const auto count = triggerToPlay.nestedWorldPulse
                               ? juce::jmax (1, triggerToPlay.soundElementCount)
                               : triggerToPlay.soundElementCount;

        for (int i = 0; i < count; ++i)
            events.push_back (makeNoteEvent (triggerToPlay, i));
    }

    if (events.empty() && ! scheduledSequence)
        return;

    if (! events.empty())
    {
        embeddedSc.setTransport (currentBpm, tick, true);
        embeddedSc.enqueue (events);
    }

    ++tick;
}

bool ScDiscAudioEngine::triggerPdGui (const juce::String& patch,
                                      const juce::String& receiver,
                                      float value,
                                      bool bangOnly,
                                      float durationSeconds,
                                      const juce::String& searchPath)
{
    if (! ready)
        return false;

    return pdAudio.sendGuiValue (patch, receiver, value, bangOnly, durationSeconds, searchPath);
}

bool ScDiscAudioEngine::triggerPdMessage (const juce::String& patch,
                                          const juce::String& receiver,
                                          const juce::String& selector,
                                          const juce::StringArray& atoms,
                                          float durationSeconds,
                                          const juce::String& searchPath)
{
    if (! ready)
        return false;

    return pdAudio.sendMessage (patch, receiver, selector, atoms, durationSeconds, searchPath);
}

bool ScDiscAudioEngine::sendPdMidiSmokeEvents()
{
    if (! ready)
        return false;

    return pdAudio.sendMidiSmokeEvents();
}

bool ScDiscAudioEngine::sendPdMidiMessage (const juce::MidiMessage& message, int port)
{
    return pdAudio.sendMidiMessage (message, port);
}

void ScDiscAudioEngine::setPdMessageOutputCallback (std::function<void(const juce::String&, const juce::String&, const juce::StringArray&)> callback)
{
    pdAudio.setMessageOutputCallback (std::move (callback));
}

void ScDiscAudioEngine::setPdMessageSubscriptions (const juce::StringArray& receivers)
{
    pdAudio.setMessageSubscriptions (receivers);
}

int ScDiscAudioEngine::getPdPatchDollarZero (const juce::String& patch, const juce::String& searchPath)
{
    return pdAudio.getPatchDollarZero (patch, searchPath);
}

std::vector<PdAudioEngine::ArraySnapshot> ScDiscAudioEngine::readPdArrays (const juce::String& patch,
                                                                           const juce::StringArray& names,
                                                                           const juce::String& searchPath,
                                                                           int maximumSamples)
{
    return pdAudio.readArrays (patch, names, searchPath, maximumSamples);
}

void ScDiscAudioEngine::setPdMidiOutputCallback (std::function<void(int, const juce::MidiMessage&)> callback)
{
    pdAudio.setMidiOutputCallback (std::move (callback));
}

bool ScDiscAudioEngine::runPdMidiOutputSmoke()
{
    if (! ready)
        return false;

    return pdAudio.runMidiOutputSmoke();
}

void ScDiscAudioEngine::stopPreview()
{
    {
        const juce::ScopedLock lock (scheduledEventLock);
        scheduledEvents.clear();
    }

    pdAudio.stopAllPatches();
    embeddedSc.stopAll();
}

bool ScDiscAudioEngine::isReady() const noexcept
{
    return ready && embeddedSc.isReady();
}

juce::String ScDiscAudioEngine::getStatusText() const
{
    if (! ready)
        return "SC unavailable: " + embeddedSc.getLastError();

    return embeddedSc.getStatusText() + "  /  " + pdAudio.getStatusText();
}

bool ScDiscAudioEngine::scheduleScSheet (const DiscAudioTrigger& triggerToPlay)
{
    auto score = ScoreDocument::fromValueTree (triggerToPlay.scSheet);

    if (score.getNumRows() == 0)
        return false;

    auto preparedEvents = buildScSheetEvents (score, currentBpm, currentSampleRate);

    if (preparedEvents.empty())
        return false;

    constexpr size_t maxScheduledSheetEvents = 4096;
    if (preparedEvents.size() > maxScheduledSheetEvents)
        preparedEvents.resize (maxScheduledSheetEvents);

    const auto sequenceStartSample = renderedSamples.load();
    const auto baseTick = tick;
    const auto samplesPerBeat = currentSampleRate * 60.0 / juce::jmax (20.0, currentBpm);
    std::vector<ScheduledEvent> newEvents;
    newEvents.reserve (preparedEvents.size());

    for (const auto& sheetEvent : preparedEvents)
    {
        gridcollider::EventFields fields;
        fields.timestampSeconds = static_cast<double> (sheetEvent.sample) / currentSampleRate;
        fields.tick = baseTick + static_cast<std::uint64_t> (juce::jmax (0.0, std::round (static_cast<double> (sheetEvent.sample) / samplesPerBeat)));
        fields.sourceCell = { juce::jlimit (0, 63, 10 + triggerToPlay.branchIndex * 4 + juce::jmax (0, sheetEvent.groupIndex) * 3),
                              juce::jlimit (0, 63, 30 + triggerToPlay.depth * 7) };
        fields.instrumentName = instrumentForVoice (sheetEvent.voice);
        fields.pitch = sheetEvent.midi;
        fields.velocity = juce::jlimit (0.0f, 1.0f, sheetEvent.amp * 1.35f);
        fields.durationTicks = 1;
        fields.parameters["sustain"] = juce::String (juce::jlimit (0.025f, 24.0f, sheetEvent.durationSeconds), 4);
        fields.parameters["pan"] = juce::String (juce::jlimit (-1.0f, 1.0f, sheetEvent.pan), 4);
        fields.parameters["fold"] = juce::String (juce::jlimit (0.0f, 1.0f, sheetEvent.cutoff), 4);
        fields.parameters["otherFold"] = juce::String (juce::jlimit (0.0f, 1.0f, sheetEvent.amp), 4);
        fields.parameters["sides"] = juce::String (juce::jlimit (3, 8, 4 + triggerToPlay.depth));
        fields.parameters["otherSides"] = juce::String (juce::jlimit (3, 8, 3 + juce::jmax (0, sheetEvent.groupIndex)));
        fields.parameters["tip"] = juce::String (juce::jlimit (0, 7, sheetEvent.midi % 8));
        fields.parameters["velocity"] = juce::String (fields.velocity, 3);
        fields.parameters["tempo"] = juce::String (currentBpm, 3);

        newEvents.push_back ({ sequenceStartSample + sheetEvent.sample,
                               gridcollider::InternalEvent { gridcollider::NoteEvent { fields } } });
    }

    const juce::ScopedLock lock (scheduledEventLock);
    scheduledEvents.insert (scheduledEvents.end(),
                            std::make_move_iterator (newEvents.begin()),
                            std::make_move_iterator (newEvents.end()));
    return true;
}

bool ScDiscAudioEngine::scheduleOrcaGrid (const DiscAudioTrigger& triggerToPlay)
{
    auto snapshot = triggerToPlay.orcaGrid;

    if (snapshot.width <= 0 || snapshot.height <= 0)
        return false;

    if (snapshot.cells.size() != static_cast<std::size_t> (snapshot.width * snapshot.height))
        snapshot.cells.assign (static_cast<std::size_t> (snapshot.width * snapshot.height), gridcollider::GridModel::emptyGlyph);

    gridcollider::GridInterpreter interpreter;
    constexpr int framesToEvaluate = 64;
    constexpr int ticksPerBeat = 4;
    const auto samplesPerFrame = currentSampleRate * 60.0 / (currentBpm * static_cast<double> (ticksPerBeat));
    const auto sequenceStartSample = renderedSamples.load();
    const auto baseTick = tick;
    std::vector<ScheduledEvent> newEvents;

    for (int frame = 0; frame < framesToEvaluate; ++frame)
    {
        auto evaluation = interpreter.evaluate (snapshot, static_cast<std::uint64_t> (frame));

        if (evaluation.grid.width > 0 && evaluation.grid.height > 0)
            snapshot = std::move (evaluation.grid);

        const auto dueSample = sequenceStartSample + static_cast<std::int64_t> (std::round (static_cast<double> (frame) * samplesPerFrame));

        for (auto event : evaluation.events)
        {
            std::visit ([this, baseTick, frame, dueSample] (auto& typed)
            {
                typed.fields.timestampSeconds = static_cast<double> (dueSample) / currentSampleRate;
                typed.fields.tick = baseTick + static_cast<std::uint64_t> (frame);
            }, event);

            if (std::holds_alternative<gridcollider::NoteEvent> (event)
                || std::holds_alternative<gridcollider::ControlEvent> (event)
                || std::holds_alternative<gridcollider::TriggerEvent> (event))
                newEvents.push_back ({ dueSample, std::move (event) });
        }
    }

    if (newEvents.empty())
        return false;

    constexpr size_t maxScheduledOrcaEvents = 4096;
    if (newEvents.size() > maxScheduledOrcaEvents)
        newEvents.resize (maxScheduledOrcaEvents);

    const juce::ScopedLock lock (scheduledEventLock);
    scheduledEvents.insert (scheduledEvents.end(),
                            std::make_move_iterator (newEvents.begin()),
                            std::make_move_iterator (newEvents.end()));
    return true;
}

void ScDiscAudioEngine::enqueueDueScheduledEvents()
{
    std::vector<gridcollider::InternalEvent> dueEvents;
    const auto now = renderedSamples.load();

    {
        const juce::ScopedLock lock (scheduledEventLock);
        auto write = scheduledEvents.begin();

        for (auto read = scheduledEvents.begin(); read != scheduledEvents.end(); ++read)
        {
            if (read->dueSample <= now)
                dueEvents.push_back (std::move (read->event));
            else
            {
                if (write != read)
                    *write = std::move (*read);

                ++write;
            }
        }

        scheduledEvents.erase (write, scheduledEvents.end());
    }

    if (! dueEvents.empty())
    {
        embeddedSc.setTransport (currentBpm, tick, true);
        embeddedSc.enqueue (dueEvents);
    }
}

std::uint64_t ScDiscAudioEngine::ticksForSeconds (float seconds) const noexcept
{
    if (seconds < 0.0f)
        seconds = 9999.0f;

    const auto beats = static_cast<double> (juce::jlimit (0.25f, 9999.0f, seconds)) * currentBpm / 60.0;
    return static_cast<std::uint64_t> (juce::jmax (1.0, std::ceil (beats)));
}

juce::String ScDiscAudioEngine::synthForProgram (const juce::String& program)
{
    const auto trimmed = program.trim();

    if (trimmed.isEmpty())
        return {};

    if (const auto existing = loadedScPrograms.find (trimmed); existing != loadedScPrograms.end())
        return existing->second;

    if (failedScPrograms.count (trimmed) > 0)
        return {};

    return compileProgram (trimmed);
}

juce::String ScDiscAudioEngine::compileProgram (const juce::String& program)
{
    const auto synthName = "otherware_disc_" + juce::String (nextProgramId++);
    const auto source = wrappedProgramSource (synthName, program);

    if (! embeddedSc.loadSynthDef (synthName, source))
    {
        failedScPrograms.insert (program);
        return {};
    }

    juce::AudioBuffer<float> warmup (currentOutputChannels, 128);
    for (int i = 0; i < 4; ++i)
        embeddedSc.render (warmup);

    loadedScPrograms[program] = synthName;
    return synthName;
}

juce::String ScDiscAudioEngine::synthForPdPatch (const juce::String& patch)
{
    const auto trimmed = patch.trim();

    if (trimmed.isEmpty())
        return {};

    if (const auto existing = loadedPdPrograms.find (trimmed); existing != loadedPdPrograms.end())
        return existing->second;

    if (failedPdPrograms.count (trimmed) > 0)
        return {};

    return compilePdPatch (trimmed);
}

juce::String ScDiscAudioEngine::compilePdPatch (const juce::String& patch)
{
    const auto synthName = "otherware_pd_" + juce::String (nextPdProgramId++);
    const auto source = wrappedPdPatchSource (synthName, patch);

    if (! embeddedSc.loadSynthDef (synthName, source))
    {
        failedPdPrograms.insert (patch);
        return {};
    }

    juce::AudioBuffer<float> warmup (currentOutputChannels, 128);
    for (int i = 0; i < 4; ++i)
        embeddedSc.render (warmup);

    loadedPdPrograms[patch] = synthName;
    return synthName;
}

juce::String ScDiscAudioEngine::wrappedProgramSource (const juce::String& synthName, const juce::String& program)
{
    const auto trimmed = program.trim();

    if (trimmed.containsIgnoreCase ("SynthDef("))
        return trimmed.replace ("__name__", synthName, true);

    return "SynthDef(\\"
        + synthName
        + ", { |out = 0, pitch = 60, amp = 0.24, sustain = 4.0, pan = 0, fold = 0, otherFold = 0, sides = 4, otherSides = 4, tip = 0, velocity = 0.5, tempo = 120|\n"
        + trimmed
        + "\n})";
}

juce::String ScDiscAudioEngine::wrappedPdPatchSource (const juce::String& synthName, const juce::String& patch)
{
    const auto frequency = firstPdSignalFrequency (patch, "osc~", firstPdSignalFrequency (patch, "phasor~", 220.0f));
    const auto durationSeconds = firstPdMessageDurationMs (patch, 420.0f) / 1000.0f;
    const auto usesNoise = patch.contains ("noise~");
    const auto usesSaw = patch.contains ("phasor~");

    juce::String source;
    source << "SynthDef(\\"
           << synthName
           << ", { |out = 0, pitch = 60, amp = 0.22, sustain = "
           << juce::String (durationSeconds, 3)
           << ", pan = 0, velocity = 0.5, tempo = 120|\n"
           << "    var env, sig, freq;\n"
           << "    freq = " << juce::String (frequency, 4) << ";\n"
           << "    env = EnvGen.kr(Env.perc(0.003, sustain.max(0.03), curve: -4), doneAction: 2);\n";

    if (usesNoise)
        source << "    sig = BPF.ar(WhiteNoise.ar, freq.clip(80, 9000), 0.22);\n";
    else if (usesSaw)
        source << "    sig = LFSaw.ar(freq).softclip;\n";
    else
        source << "    sig = SinOsc.ar(freq);\n";

    source << "    Out.ar(out, Pan2.ar((sig * env * amp * velocity.linlin(0, 1, 0.55, 1.25)).tanh, pan));\n"
           << "})";
    return source;
}

gridcollider::InternalEvent ScDiscAudioEngine::makeScProgramEvent (const DiscAudioTrigger& triggerToPlay)
{
    gridcollider::EventFields fields;
    fields.timestampSeconds = static_cast<double> (tick) * 60.0 / currentBpm;
    fields.tick = tick;
    fields.sourceCell = { juce::jlimit (0, 63, 18 + triggerToPlay.branchIndex * 3),
                          juce::jlimit (0, 63, 12 + triggerToPlay.depth * 8) };
    fields.instrumentName = synthForProgram (triggerToPlay.scCode);
    fields.pitch = triggerToPlay.midiNote >= 0 ? noteFor (triggerToPlay, 0)
                                               : noteFor (triggerToPlay, 0) + 12;
    fields.velocity = 0.68f * triggerToPlay.gain;
    fields.durationTicks = ticksForSeconds (triggerToPlay.scDurationSeconds);
    fields.parameters["fold"] = "0.880";
    fields.parameters["otherFold"] = "0.420";
    fields.parameters["sides"] = juce::String (juce::jlimit (3, 8, 5 + triggerToPlay.depth));
    fields.parameters["otherSides"] = "6";
    fields.parameters["tip"] = "0";
    fields.parameters["velocity"] = juce::String (fields.velocity, 3);
    fields.parameters["tempo"] = juce::String (currentBpm, 3);
    fields.parameters["pan"] = juce::String (triggerToPlay.pan, 3);

    if (triggerToPlay.scDurationSeconds < 0.0f)
        fields.parameters["sustain"] = "-1.0";

    return gridcollider::InternalEvent { gridcollider::NoteEvent { fields } };
}

gridcollider::InternalEvent ScDiscAudioEngine::makePdPatchEvent (const DiscAudioTrigger& triggerToPlay)
{
    gridcollider::EventFields fields;
    fields.timestampSeconds = static_cast<double> (tick) * 60.0 / currentBpm;
    fields.tick = tick;
    fields.sourceCell = { juce::jlimit (0, 63, 20 + triggerToPlay.branchIndex * 3),
                          juce::jlimit (0, 63, 16 + triggerToPlay.depth * 8) };
    fields.instrumentName = synthForPdPatch (triggerToPlay.pdPatch);
    fields.pitch = triggerToPlay.midiNote >= 0 ? noteFor (triggerToPlay, 1)
                                               : noteFor (triggerToPlay, 1) + 7;
    fields.velocity = 0.74f * triggerToPlay.gain;
    fields.durationTicks = ticksForSeconds (triggerToPlay.pdDurationSeconds);
    fields.parameters["velocity"] = juce::String (fields.velocity, 3);
    fields.parameters["tempo"] = juce::String (currentBpm, 3);
    fields.parameters["sustain"] = triggerToPlay.pdDurationSeconds < 0.0f
                                       ? "0.42"
                                       : juce::String (triggerToPlay.pdDurationSeconds, 3);
    fields.parameters["pan"] = juce::String (juce::jlimit (-1.0f, 1.0f, triggerToPlay.pan), 3);
    return gridcollider::InternalEvent { gridcollider::NoteEvent { fields } };
}

gridcollider::InternalEvent ScDiscAudioEngine::makeNoteEvent (const DiscAudioTrigger& triggerToPlay, int elementIndex)
{
    gridcollider::EventFields fields;
    fields.timestampSeconds = static_cast<double> (tick) * 60.0 / currentBpm;
    fields.tick = tick + static_cast<std::uint64_t> (elementIndex);
    fields.sourceCell = { juce::jlimit (0, 63, 8 + triggerToPlay.branchIndex * 5 + elementIndex),
                          juce::jlimit (0, 63, 8 + triggerToPlay.depth * 9) };
    fields.instrumentName = triggerToPlay.nestedWorldPulse
                                ? "pad"
                                : soundPalette[(triggerCounter + elementIndex + triggerToPlay.depth) % soundPalette.size()];
    fields.pitch = noteFor (triggerToPlay, elementIndex);
    fields.velocity = velocityFor (triggerToPlay, elementIndex) * triggerToPlay.gain;
    fields.durationTicks = triggerToPlay.nestedWorldPulse ? 4 : 1;
    fields.parameters["fold"] = triggerToPlay.nestedWorldPulse ? "1.000" : "0.620";
    fields.parameters["otherFold"] = juce::String (juce::jlimit (0.0f, 1.0f, static_cast<float> (triggerToPlay.soundElementCount) / 12.0f), 3);
    fields.parameters["sides"] = juce::String (juce::jlimit (3, 8, 3 + triggerToPlay.soundElementCount % 6));
    fields.parameters["otherSides"] = juce::String (juce::jlimit (3, 8, 4 + triggerToPlay.depth));
    fields.parameters["tip"] = juce::String (juce::jlimit (0, 7, elementIndex));
    fields.parameters["velocity"] = juce::String (fields.velocity, 3);
    fields.parameters["tempo"] = juce::String (currentBpm, 3);
    fields.parameters["pan"] = juce::String (triggerToPlay.pan, 3);

    ++triggerCounter;
    return gridcollider::InternalEvent { gridcollider::NoteEvent { fields } };
}
