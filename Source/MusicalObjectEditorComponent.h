#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

struct MusicalObjectSound
{
    enum class Playback { synth = 0, superCollider, pureData };

    Playback playback = Playback::synth;
    int midiNote = 60;
    float durationSeconds = 0.45f;
    juce::String scSource;
    juce::String pdSource;
};

class MusicalObjectEditorComponent final : public juce::Component
{
public:
    explicit MusicalObjectEditorComponent (MusicalObjectSound initialState);

    std::function<void(const MusicalObjectSound&)> onChange;
    std::function<void(const MusicalObjectSound&)> onPreview;

    void paint (juce::Graphics&) override;
    void resized() override;

    static juce::String defaultScSource();
    static juce::String defaultPdSource();

private:
    MusicalObjectSound state;
    bool updating = false;

    juce::Label titleLabel, playbackLabel, pitchLabel, durationLabel, sourceLabel;
    juce::ComboBox playbackBox;
    juce::Slider pitchSlider, durationSlider;
    juce::TextEditor sourceEditor;
    juce::TextButton resetButton { "Reset template" };
    juce::TextButton previewButton { "Audition" };

    void refresh();
    void changed();
};
