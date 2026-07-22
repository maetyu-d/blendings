#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <map>
#include <atomic>
#include <functional>
#include <vector>

class PdAudioEngine
{
public:
    struct ArraySnapshot
    {
        juce::String name;
        int totalSize = 0;
        std::vector<float> values;
    };

    PdAudioEngine() = default;
    ~PdAudioEngine();

    bool prepare (double sampleRate, int maximumBlockSize, int outputChannels, int inputChannels = 0);
    void release() noexcept;
    void renderAndAdd (juce::AudioBuffer<float>& output, const juce::AudioBuffer<float>* input = nullptr);
    bool preparePatch (const juce::String& patch, const juce::String& searchPath = {});
    bool triggerPatch (const juce::String& patch, float durationSeconds, const juce::String& searchPath = {}, float triggerValue = -1.0f);
    bool sendGuiValue (const juce::String& patch, const juce::String& receiver, float value, bool bangOnly, float durationSeconds, const juce::String& searchPath = {});
    bool sendMessage (const juce::String& patch, const juce::String& receiver, const juce::String& selector, const juce::StringArray& atoms, float durationSeconds, const juce::String& searchPath = {});
    bool sendMidiMessage (const juce::MidiMessage& message, int port = 0);
    void setMidiOutputCallback (std::function<void(int, const juce::MidiMessage&)> callback);
    void setMessageOutputCallback (std::function<void(const juce::String&, const juce::String&, const juce::StringArray&)> callback);
    void setMessageSubscriptions (const juce::StringArray& receivers);
    [[nodiscard]] int getPatchDollarZero (const juce::String& patch, const juce::String& searchPath = {});
    [[nodiscard]] std::vector<ArraySnapshot> readArrays (const juce::String& patch,
                                                         const juce::StringArray& names,
                                                         const juce::String& searchPath = {},
                                                         int maximumSamples = 65536);
    bool sendMidiSmokeEvents();
    bool runMidiOutputSmoke();
    void stopAllPatches();

    [[nodiscard]] bool isReady() const noexcept { return ready; }
    [[nodiscard]] int getDspBlockSize() const noexcept { return dspBlockSize; }
    [[nodiscard]] juce::String getStatusText() const;

private:
    struct LoadedPatch
    {
        juce::String receiver;
        juce::File file;
        void* handle = nullptr;
        int dollarZero = 0;
        std::int64_t stopSample = -1;
    };

    juce::CriticalSection lock;
    double currentSampleRate = 44100.0;
    int currentOutputChannels = 2;
    int currentInputChannels = 0;
    int maximumBlockSize = 512;
    int dspBlockSize = 64;
    int nextPatchId = 1;
    std::int64_t renderedSamples = 0;
    std::int64_t lastAudibleSample = 0;
    void* pdInstance = nullptr;
    bool ready = false;
    juce::String lastError;
    juce::String lastWarning;
    std::map<juce::String, LoadedPatch> loadedPatches;
    std::vector<float> pdOutput;
    std::vector<float> pdInput;
    std::vector<float> pendingPdOutput;
    int pendingPdFrames = 0;
    std::function<void(int, const juce::MidiMessage&)> midiOutputCallback;
    std::function<void(const juce::String&, const juce::String&, const juce::StringArray&)> messageOutputCallback;
    std::map<juce::String, void*> messageSubscriptions;

    static std::atomic<PdAudioEngine*> activeMidiOutputEngine;
    static std::atomic<PdAudioEngine*> activeMessageOutputEngine;
    static void messageBangHook (const char* receiver);
    static void messageFloatHook (const char* receiver, float value);
    static void messageSymbolHook (const char* receiver, const char* symbol);
    static void messageListHook (const char* receiver, int argc, struct _atom* argv);
    static void messageTypedHook (const char* receiver, const char* selector, int argc, struct _atom* argv);
    static void midiNoteOnHook (int channel, int pitch, int velocity);
    static void midiControlChangeHook (int channel, int controller, int value);
    static void midiProgramChangeHook (int channel, int value);
    static void midiPitchBendHook (int channel, int value);
    static void midiAftertouchHook (int channel, int value);
    static void midiPolyAftertouchHook (int channel, int pitch, int value);
    static void midiByteHook (int port, int byte);
    void dispatchMidiOutput (int port, const juce::MidiMessage& message) const;
    void dispatchMessageOutput (const juce::String& receiver, const juce::String& selector, const juce::StringArray& atoms) const;

    LoadedPatch* findOrLoadPatchLocked (const juce::String& patch, const juce::String& searchPath, float durationSeconds);
    bool loadPatchLocked (const juce::String& patch, const juce::String& searchPath, LoadedPatch& loadedPatch);
    void unloadExpiredPatchesLocked();
    void closePatchLocked (LoadedPatch& loadedPatch);
    void setCurrentInstanceIfNeeded() const noexcept;
    juce::String makePatchReceiver();
    juce::File makePatchFile (const juce::String& receiver) const;
    static void addSearchPathIfDirectory (const juce::File& directory);
    static void addBundledPdSearchPaths();
    static juce::String namespaceTriggerReceiver (const juce::String& patch, const juce::String& receiver);
    static juce::StringArray declaredSearchPaths (const juce::String& patch);
    static juce::StringArray declaredLibraries (const juce::String& patch);
    static bool isBundledLibrary (const juce::String& libraryName);
    static juce::File resolveDeclaredPath (const juce::String& path, const juce::File& baseDirectory);
};
