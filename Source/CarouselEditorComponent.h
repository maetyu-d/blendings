#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_data_structures/juce_data_structures.h>

#include <functional>
#include <map>
#include <set>
#include <vector>

#include "MusicalObjectEditorComponent.h"

struct CarouselDocument
{
    enum class ItemType { tone, orbit, post, plank };
    enum class PlaybackType { synth = 0, superCollider, pureData };
    struct Item
    {
        int id = 0;
        ItemType type = ItemType::tone;
        float x = 0.0f, y = 0.0f;
        int midi = 60, voice = 0, ownerOrbit = -1;
        float radius = 1.0f, speed = 0.25f, phase = 0.0f;
        bool euclidean = false;
        PlaybackType playback = PlaybackType::synth;
        float durationSeconds = 1.0f;
        juce::String scCode;
        juce::String pdPatch;
    };

    int columns = 12, rows = 8, nextId = 20;
    double bpm = 92.0;
    std::vector<Item> items;
    juce::String recoveryNotice;

    static CarouselDocument createDefault();
    juce::ValueTree toValueTree() const;
    static CarouselDocument fromValueTree (const juce::ValueTree& tree);
};

class CarouselEditorComponent final : public juce::Component,
                                      private juce::Timer
{
public:
    CarouselEditorComponent();
    ~CarouselEditorComponent() override;

    void setDocument (const CarouselDocument&);
    [[nodiscard]] CarouselDocument getDocument() const { return document; }
    void setRunning (bool);
    void setSampleClock (std::function<double()> clockSeconds);
    [[nodiscard]] bool isRunning() const noexcept { return running; }
    std::function<void(const CarouselDocument&)> onChange;
    std::function<void(const CarouselDocument::Item&)> onTone;
    using SoundCommit = std::function<void (const juce::String&, float)>;
    using SoundEditorRequest = std::function<void (const juce::String&, float, SoundCommit)>;
    SoundEditorRequest onScEditorRequested;
    SoundEditorRequest onPdEditorRequested;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool openFullSoundEditor (int toneId);
    bool runPerformanceSmokeChecks (juce::String& failureMessage);
    bool runAttachmentSmokeChecks (juce::String& failureMessage);
    bool runVisualInteractionSmokeChecks (juce::String& failureMessage);
    bool undo();
    bool redo();

private:
    enum class Tool { select, tone, orbit, post, plank };
    CarouselDocument document;
    Tool tool = Tool::select;
    int selected = -1, dragged = -1, hoveredAttachmentTarget = -1, dragAttachmentTarget = -1;
    bool running = false, panning = false, suppress = false;
    bool restoringHistory = false, dragHistoryStarted = false, dragDetachedFromParent = false;
    bool sliderHistoryStarted = false;
    double lastTime = 0.0;
    std::function<double()> sampleClockSeconds;
    float zoom = 1.0f;
    juce::Point<float> pan, panStart, mouseStart, dragOffset;
    std::set<juce::String> contacts;
    std::map<int, double> activeToneUntilMs;
    std::vector<CarouselDocument> undoHistory, redoHistory;

    juce::TextButton selectButton { "select" }, toneButton { "tone" }, orbitButton { "orbit" }, postButton { "post" }, plankButton { "plank" };
    juce::TextButton playButton { "play" }, clearButton { "clear all" }, deleteButton { "delete" }, fitButton { "fit" };
    juce::TextButton resetCodeButton { "Reset template" }, soundButton { "Sound..." }, auditionButton { "Audition" };
    juce::Slider bpmSlider, pitchSlider, durationSlider, speedSlider, radiusSlider, countSlider;
    juce::ComboBox columnsBox, rowsBox, playbackBox;
    juce::ComboBox attachmentBox;
    juce::TextEditor codeEditor;
    juce::ToggleButton euclideanButton { "Euclidean spacing" };
    juce::Label titleLabel, detailLabel, globalLabel, selectionLabel, bpmLabel, columnsLabel, rowsLabel,
                pitchLabel, playbackLabel, durationLabel, codeLabel, speedLabel, radiusLabel, countLabel,
                attachmentLabel, attachmentSummaryLabel;

    void timerCallback() override;
    [[nodiscard]] double currentClockMs() const;
    void changed();
    void pushUndoState();
    void refreshAttachmentInspector();
    void changeSelectedAttachment();
    void refreshInspector();
    CarouselDocument::Item* selectedItem();
    const CarouselDocument::Item* selectedItem() const;
    void setTool (Tool);
    void deleteSelected();
    void arrangeOrbit (int orbitId);
    void updatePlankGeometry (CarouselDocument::Item& plank);
    void positionPlanksForOrbit (int orbitId);
    void translateDependents (int parentId, float dx, float dy);
    int compatibleAttachmentTargetAt (juce::Point<float> gridPosition, int childId) const;
    bool wouldCreateAttachmentCycle (int childId, int parentId) const;
    bool attachItemTo (int childId, int parentId);
    int orbitToneCount (int orbitId) const;
    void setOrbitToneCount (int count);
    void triggerTone (const CarouselDocument::Item&);
    void resetSelectedToneCode();
    void openSelectedToneSoundEditor();
    float cellSize() const;
    juce::Rectangle<float> fieldViewport() const;
    juce::Rectangle<float> gridBounds() const;
    juce::Point<float> screenPoint (juce::Point<float>) const;
    juce::Point<float> gridPoint (juce::Point<float>) const;
    static juce::String noteName (int midi);
    static juce::Colour itemColour (const CarouselDocument::Item&);
};
