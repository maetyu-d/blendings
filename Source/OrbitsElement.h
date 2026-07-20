#pragma once

#include <JuceHeader.h>

#include <functional>
#include <vector>

namespace blendings
{
struct OrbitsLane
{
    juce::String name { "Sound 1" };
    juce::String scCode;
    float durationSeconds = 1.0f;
    float probability = 1.0f;
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
    std::vector<OrbitsTriggerLine> lines;
};

struct OrbitsDocument
{
    std::vector<OrbitsTrack> tracks;

    static OrbitsDocument createDefault();
    juce::ValueTree toValueTree() const;
    static OrbitsDocument fromValueTree (const juce::ValueTree&);

    double loopDurationSeconds (const OrbitsTrack&) const;
    std::vector<double> phasesForLine (const OrbitsTrack&, const OrbitsTriggerLine&) const;
    void refreshTriggerPhases();
};

class OrbitsEditorComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    using Commit = std::function<void (const OrbitsDocument&)>;
    using EditSc = std::function<void (const juce::String&, float, std::function<void (juce::String, float)>)>;
    using Audition = std::function<void (const OrbitsLane&)>;

    OrbitsEditorComponent (OrbitsDocument, Commit, EditSc, Audition);
    ~OrbitsEditorComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class SpiralCanvas;

    OrbitsDocument document;
    Commit commit;
    EditSc editSc;
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
    juce::ComboBox trackBox;
    juce::ToggleButton hideButton { "Hide track" };
    juce::ToggleButton muteButton { "Mute track" };

    juce::Label trackLabel, bpmLabel, meterLabel, barsLabel, shapeLabel, soundLineLabel;
    juce::Label numeratorLabel, denominatorLabel;
    juce::Label thicknessLabel, warpLabel, twistLabel, phaseLabel;
    juce::Label xRotationLabel, yRotationLabel, xOffsetLabel, yOffsetLabel;
    juce::Slider bpmSlider, numeratorSlider, denominatorSlider, barsSlider;
    juce::Slider thicknessSlider, warpSlider, twistSlider, phaseSlider;
    juce::Slider xRotationSlider, yRotationSlider, xOffsetSlider, yOffsetSlider;

    int selectedTrack = 0;
    int selectedLine = -1;
    bool refreshing = false;
    bool previewRunning = false;
    double previewSeconds = 0.0;
    double lastPreviewTime = 0.0;
    std::vector<double> previewPhases;

    OrbitsTrack* track();
    OrbitsTriggerLine* line();
    void configure();
    void configureSlider (juce::Slider&, double minimum, double maximum, double interval);
    void refresh();
    void changed (bool recomputeIntersections = true);
    void editSelectedLine();
    void addTrack();
    void removeTrack();
    void timerCallback() override;
};
}
