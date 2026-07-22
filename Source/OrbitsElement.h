#pragma once

#include <JuceHeader.h>

#include <functional>
#include <map>
#include <vector>

namespace blendings
{
struct OrbitsLane
{
    enum class PlaybackType { superCollider = 0, pureData };

    juce::String name { "Sound 1" };
    juce::String scCode;
    juce::String pdPatch;
    PlaybackType playback = PlaybackType::superCollider;
    float durationSeconds = 1.0f;
    float probability = 1.0f;
    float gain = 1.0f;
    float pan = 0.0f;
    int colourIndex = -1;
    bool enabled = true;
};

struct OrbitsTriggerLine
{
    juce::String id;
    juce::Point<float> start { 0.25f, 0.5f };
    juce::Point<float> end { 0.75f, 0.5f };
    OrbitsLane sound;
    std::vector<double> triggerPhases;
};

struct OrbitsTrack
{
    enum class ClockMode { free = 0, project, ratio };

    juce::String name { "Track 1" };
    int colourIndex = 0;
    double bpm = 120.0;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
    double loopBars = 2.0;
    double thickness = 1.0;
    double orbitWarpAmount = 0.0;
    double spiralTwistAmount = 0.0;
    double phaseOffsetDegrees = 0.0;
    double xRotationDegrees = 0.0;
    double yRotationDegrees = 0.0;
    double xOffset = 0.0;
    double yOffset = 0.0;
    bool hidden = false;
    bool muted = false;
    ClockMode clockMode = ClockMode::free;
    double tempoRatio = 1.0;
    bool resetPhaseOnStart = true;
    std::vector<OrbitsTriggerLine> lines;
};

struct OrbitsDocument
{
    std::vector<OrbitsTrack> tracks;
    double projectTempoBpm = 120.0;
    bool snapToMusicalGrid = false;
    int snapDivisionsPerBeat = 4;

    static OrbitsDocument createDefault();
    juce::ValueTree toValueTree() const;
    static OrbitsDocument fromValueTree (const juce::ValueTree&);

    double loopDurationSeconds (const OrbitsTrack&) const;
    double effectiveBpm (const OrbitsTrack&) const;
    std::vector<double> phasesForLine (const OrbitsTrack&, const OrbitsTriggerLine&) const;
    void snapLineToMusicalGrid (const OrbitsTrack&, OrbitsTriggerLine&) const;
    void refreshTriggerPhases();
};

class OrbitsEditorComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    using Commit = std::function<void (const OrbitsDocument&)>;
    using EditSc = std::function<void (const juce::String&, float, std::function<void (juce::String, float)>)>;
    using EditPd = std::function<void (const juce::String&, float, std::function<void (juce::String, float)>)>;
    using Audition = std::function<void (const OrbitsLane&)>;

    OrbitsEditorComponent (OrbitsDocument, Commit, EditSc, EditPd, Audition);
    ~OrbitsEditorComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class SpiralCanvas;

    OrbitsDocument document;
    Commit commit;
    EditSc editSc;
    EditPd editPd;
    Audition audition;
    std::unique_ptr<SpiralCanvas> canvas;
    juce::Viewport inspectorViewport;
    juce::Component inspectorContent;

    juce::TextButton addTrackButton { "+ Track" };
    juce::TextButton removeTrackButton { "-" };
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton editSoundButton { "Edit SC" };
    juce::TextButton auditionButton { "Audition" };
    juce::TextButton deleteLineButton { "Delete line" };
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    juce::ComboBox trackBox;
    juce::ComboBox clockModeBox, snapDivisionBox, playbackBox, lineColourBox;
    juce::ToggleButton hideButton { "Hide track" };
    juce::ToggleButton muteButton { "Mute track" };
    juce::ToggleButton resetPhaseButton { "Reset phase on start" };
    juce::ToggleButton snapButton { "Snap sound lines" };
    juce::ToggleButton lineEnabledButton { "Enabled" };
    juce::TextEditor lineNameEditor;

    juce::Label trackLabel, bpmLabel, meterLabel, barsLabel, shapeLabel, soundLineLabel;
    juce::Label numeratorLabel, denominatorLabel;
    juce::Label thicknessLabel, warpLabel, twistLabel, phaseLabel;
    juce::Label xRotationLabel, yRotationLabel, xOffsetLabel, yOffsetLabel;
    juce::Label clockModeLabel, ratioLabel, snapLabel;
    juce::Label lineNameLabel, playbackLabel, lineColourLabel, durationLabel, probabilityLabel, gainLabel, panLabel;
    juce::Slider bpmSlider, numeratorSlider, denominatorSlider, barsSlider;
    juce::Slider thicknessSlider, warpSlider, twistSlider, phaseSlider;
    juce::Slider xRotationSlider, yRotationSlider, xOffsetSlider, yOffsetSlider;
    juce::Slider ratioSlider, durationSlider, probabilitySlider, gainSlider, panSlider;

    int selectedTrack = 0;
    int selectedLine = -1;
    bool refreshing = false;
    bool previewRunning = false;
    double previewSeconds = 0.0;
    double lastPreviewTime = 0.0;
    std::vector<double> previewPhases;
    std::map<juce::String, std::pair<bool, double>> triggerFeedback;
    std::vector<OrbitsDocument> undoHistory, redoHistory;
    bool restoringHistory = false;
    bool sliderGestureActive = false;

    OrbitsTrack* track();
    OrbitsTriggerLine* line();
    void configure();
    void configureSlider (juce::Slider&, double minimum, double maximum, double interval);
    void refresh();
    void changed (bool recomputeIntersections = true);
    void editSelectedLine();
    void pushUndoState();
    void undo();
    void redo();
    void restoreHistory (const OrbitsDocument&);
    void addTrack();
    void removeTrack();
    void timerCallback() override;
};
}
