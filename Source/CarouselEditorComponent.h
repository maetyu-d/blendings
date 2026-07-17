#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_data_structures/juce_data_structures.h>

#include <functional>
#include <set>
#include <vector>

struct CarouselDocument
{
    enum class ItemType { tone, orbit, post };
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
    [[nodiscard]] bool isRunning() const noexcept { return running; }
    std::function<void(const CarouselDocument&)> onChange;
    std::function<void(const CarouselDocument::Item&)> onTone;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    enum class Tool { select, tone, orbit, post };
    CarouselDocument document;
    Tool tool = Tool::select;
    int selected = -1, dragged = -1;
    bool running = false, panning = false, suppress = false;
    double lastTime = 0.0;
    float zoom = 1.0f;
    juce::Point<float> pan, panStart, mouseStart, dragOffset;
    std::set<juce::String> contacts;

    juce::TextButton selectButton { "select" }, toneButton { "tone" }, orbitButton { "orbit" }, postButton { "post" };
    juce::TextButton playButton { "play" }, clearButton { "clear all" }, deleteButton { "delete" }, fitButton { "fit" };
    juce::TextButton resetCodeButton { "Reset template" };
    juce::Slider bpmSlider, pitchSlider, durationSlider, speedSlider, radiusSlider, countSlider;
    juce::ComboBox columnsBox, rowsBox, playbackBox;
    juce::TextEditor codeEditor;
    juce::ToggleButton euclideanButton { "Euclidean spacing" };
    juce::Label titleLabel, detailLabel, globalLabel, selectionLabel, bpmLabel, columnsLabel, rowsLabel,
                pitchLabel, playbackLabel, durationLabel, codeLabel, speedLabel, radiusLabel, countLabel;

    void timerCallback() override;
    void changed();
    void refreshInspector();
    CarouselDocument::Item* selectedItem();
    const CarouselDocument::Item* selectedItem() const;
    void setTool (Tool);
    void deleteSelected();
    void arrangeOrbit (int orbitId);
    int orbitToneCount (int orbitId) const;
    void setOrbitToneCount (int count);
    void triggerTone (const CarouselDocument::Item&);
    void resetSelectedToneCode();
    float cellSize() const;
    juce::Rectangle<float> fieldViewport() const;
    juce::Rectangle<float> gridBounds() const;
    juce::Point<float> screenPoint (juce::Point<float>) const;
    juce::Point<float> gridPoint (juce::Point<float>) const;
    static juce::String noteName (int midi);
    static juce::Colour itemColour (const CarouselDocument::Item&);
};
