#pragma once

#include "EmbeddedScAudioEngine.h"
#include "GridModel.h"
#include "PdAudioEngine.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <atomic>
#include <map>
#include <set>
#include <vector>

struct DiscAudioTrigger
{
    int soundElementCount = 0;
    bool nestedWorldPulse = false;
    int depth = 0;
    int branchIndex = 0;
    juce::String scCode;
    float scDurationSeconds = -1.0f;
    juce::String pdPatch;
    juce::String pdSearchPath;
    float pdDurationSeconds = -1.0f;
    juce::ValueTree scSheet;
    bool hasScSheet = false;
    gridcollider::GridModel::Snapshot orcaGrid;
    bool hasOrcaGrid = false;
    float gain = 1.0f;
    float pan = 0.0f;
    int midiNote = -1;

    [[nodiscard]] bool hasScCode() const noexcept { return scCode.trim().isNotEmpty(); }
    [[nodiscard]] bool hasPdPatch() const noexcept { return pdPatch.trim().isNotEmpty(); }
    [[nodiscard]] bool hasScSheetScore() const noexcept { return hasScSheet && scSheet.isValid(); }
    [[nodiscard]] bool hasOrcaGridScore() const noexcept { return hasOrcaGrid && orcaGrid.width > 0 && orcaGrid.height > 0; }
};

class ScDiscAudioEngine
{
public:
    void prepare (double sampleRate, int maximumBlockSize, int outputChannels, int inputChannels = 0);
    void release() noexcept;
    void render (juce::AudioBuffer<float>& output, const juce::AudioBuffer<float>* input = nullptr);

    void trigger (const DiscAudioTrigger& trigger);
    void triggerMany (const std::vector<DiscAudioTrigger>& triggers);
    void scheduleTriggerManyAtSample (const std::vector<DiscAudioTrigger>& triggers, std::int64_t dueSample);
    void triggerMidiNote (int midiNote, float velocity = 0.8f);
    bool triggerPdGui (const juce::String& patch, const juce::String& receiver, float value, bool bangOnly, float durationSeconds, const juce::String& searchPath = {});
    bool triggerPdMessage (const juce::String& patch, const juce::String& receiver, const juce::String& selector, const juce::StringArray& atoms, float durationSeconds, const juce::String& searchPath = {});
    bool sendPdMidiSmokeEvents();
    bool sendPdMidiMessage (const juce::MidiMessage& message, int port = 0);
    void setPdMidiOutputCallback (std::function<void(int, const juce::MidiMessage&)> callback);
    void setPdMessageOutputCallback (std::function<void(const juce::String&, const juce::String&, const juce::StringArray&)> callback);
    void setPdMessageSubscriptions (const juce::StringArray& receivers);
    void setPdAudioInputMuted (bool shouldBeMuted) noexcept { pdAudioInputMuted.store (shouldBeMuted); }
    [[nodiscard]] bool isPdAudioInputMuted() const noexcept { return pdAudioInputMuted.load(); }
    [[nodiscard]] int getPdPatchDollarZero (const juce::String& patch, const juce::String& searchPath = {});
    [[nodiscard]] std::vector<PdAudioEngine::ArraySnapshot> readPdArrays (const juce::String& patch,
                                                                         const juce::StringArray& names,
                                                                         const juce::String& searchPath = {},
                                                                         int maximumSamples = 65536);
    bool runPdMidiOutputSmoke();
    void setTempo (double bpm) noexcept;
    [[nodiscard]] std::int64_t getRenderedSamplePosition() const noexcept { return renderedSamples.load(); }
    [[nodiscard]] double getSampleRate() const noexcept { return currentSampleRate; }
    [[nodiscard]] std::int64_t alignedEventSample (std::int64_t sample) const noexcept;
    void suspendScheduledEvents();
    void resumeScheduledEvents();
    void cancelScheduledEvents();
    void stopPreview();

    [[nodiscard]] bool isReady() const noexcept;
    [[nodiscard]] juce::String getStatusText() const;

private:
    struct ScheduledEvent
    {
        std::int64_t dueSample = 0;
        gridcollider::InternalEvent event;
    };

    struct ScheduledPdTrigger
    {
        std::int64_t dueSample = 0;
        juce::String patch;
        juce::String searchPath;
        float durationSeconds = -1.0f;
        float triggerValue = -1.0f;
    };

    gridcollider::EmbeddedScAudioEngine embeddedSc;
    PdAudioEngine pdAudio;
    double currentSampleRate = 44100.0;
    std::atomic<double> currentBpm { 120.0 };
    int currentOutputChannels = 2;
    std::atomic<std::uint64_t> tick { 0 };
    int triggerCounter = 0;
    bool ready = false;
    int nextProgramId = 1;
    int nextPdProgramId = 1;
    std::map<juce::String, juce::String> loadedScPrograms;
    std::map<juce::String, juce::String> loadedPdPrograms;
    std::set<juce::String> failedScPrograms;
    std::set<juce::String> failedPdPrograms;
    mutable juce::CriticalSection scheduledEventLock;
    std::vector<ScheduledEvent> scheduledEvents;
    std::vector<ScheduledPdTrigger> scheduledPdTriggers;
    std::vector<ScheduledEvent> suspendedEvents;
    std::vector<ScheduledPdTrigger> suspendedPdTriggers;
    std::atomic<std::int64_t> renderedSamples { 0 };
    std::atomic<bool> pdAudioInputMuted { true };

    gridcollider::InternalEvent makeNoteEvent (const DiscAudioTrigger& trigger, int elementIndex);
    gridcollider::InternalEvent makeScProgramEvent (const DiscAudioTrigger& trigger);
    gridcollider::InternalEvent makePdPatchEvent (const DiscAudioTrigger& trigger);
    bool scheduleScSheet (const DiscAudioTrigger& trigger, std::int64_t startSample);
    bool scheduleOrcaGrid (const DiscAudioTrigger& trigger, std::int64_t startSample);
    void dispatchDueScheduledEvents (std::int64_t now);
    [[nodiscard]] std::int64_t nextScheduledSampleAfter (std::int64_t now) const;
    juce::String synthForProgram (const juce::String& program);
    juce::String compileProgram (const juce::String& program);
    juce::String synthForPdPatch (const juce::String& patch);
    juce::String compilePdPatch (const juce::String& patch);
    static juce::String wrappedProgramSource (const juce::String& synthName, const juce::String& program);
    static juce::String wrappedPdPatchSource (const juce::String& synthName, const juce::String& patch);
    std::uint64_t ticksForSeconds (float seconds) const noexcept;
};
