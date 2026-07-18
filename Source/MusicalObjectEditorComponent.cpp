#include "MusicalObjectEditorComponent.h"

namespace
{
const auto background = juce::Colour (0xff111815);
const auto raised = juce::Colour (0xff1b2420);
const auto stroke = juce::Colour (0xff35443e);
const auto ink = juce::Colour (0xffedf4ef);
const auto muted = juce::Colour (0xff91a099);
const auto accent = juce::Colour (0xff2f8cff);
}

MusicalObjectEditorComponent::MusicalObjectEditorComponent (MusicalObjectSound initialState)
    : state (std::move (initialState))
{
    setSize (440, 520);
    titleLabel.setText ("Sound", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (20.0f).withStyle ("Bold"));

    for (auto* label : { &titleLabel, &playbackLabel, &pitchLabel, &durationLabel, &sourceLabel })
    {
        label->setColour (juce::Label::textColourId, label == &titleLabel ? ink : muted);
        addAndMakeVisible (*label);
    }
    playbackLabel.setText ("Playback", juce::dontSendNotification);
    pitchLabel.setText ("Pitch", juce::dontSendNotification);
    durationLabel.setText ("One-shot duration", juce::dontSendNotification);
    sourceLabel.setText ("Source", juce::dontSendNotification);

    playbackBox.addItem ("Tuned synth", 1);
    playbackBox.addItem ("SuperCollider", 2);
    playbackBox.addItem ("Pure Data", 3);
    playbackBox.setColour (juce::ComboBox::backgroundColourId, raised);
    playbackBox.setColour (juce::ComboBox::textColourId, ink);
    playbackBox.setColour (juce::ComboBox::outlineColourId, stroke);
    addAndMakeVisible (playbackBox);

    const auto configureSlider = [this] (juce::Slider& slider)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 76, 26);
        slider.setColour (juce::Slider::trackColourId, stroke);
        slider.setColour (juce::Slider::thumbColourId, accent);
        slider.setColour (juce::Slider::textBoxTextColourId, ink);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, raised);
        addAndMakeVisible (slider);
    };
    configureSlider (pitchSlider);
    pitchSlider.setRange (0, 127, 1);
    configureSlider (durationSlider);
    durationSlider.setRange (0.05, 30.0, 0.05);
    durationSlider.setTextValueSuffix (" s");

    sourceEditor.setMultiLine (true);
    sourceEditor.setReturnKeyStartsNewLine (true);
    sourceEditor.setScrollbarsShown (true);
    sourceEditor.setFont (juce::FontOptions (13.0f));
    sourceEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b110e));
    sourceEditor.setColour (juce::TextEditor::textColourId, ink);
    sourceEditor.setColour (juce::TextEditor::outlineColourId, stroke);
    sourceEditor.setColour (juce::TextEditor::focusedOutlineColourId, accent);
    addAndMakeVisible (sourceEditor);

    for (auto* button : { &resetButton, &previewButton })
    {
        button->setColour (juce::TextButton::buttonColourId, raised);
        button->setColour (juce::TextButton::textColourOffId, ink);
        addAndMakeVisible (*button);
    }
    previewButton.setColour (juce::TextButton::buttonColourId, accent);

    playbackBox.onChange = [this]
    {
        if (updating) return;
        state.playback = static_cast<MusicalObjectSound::Playback> (juce::jlimit (0, 2, playbackBox.getSelectedItemIndex()));
        if (state.scSource.trim().isEmpty()) state.scSource = defaultScSource();
        if (state.pdSource.trim().isEmpty()) state.pdSource = defaultPdSource();
        refresh();
        changed();
    };
    pitchSlider.onValueChange = [this] { if (! updating) { state.midiNote = (int) pitchSlider.getValue(); changed(); } };
    durationSlider.onValueChange = [this] { if (! updating) { state.durationSeconds = (float) durationSlider.getValue(); changed(); } };
    sourceEditor.onTextChange = [this]
    {
        if (updating) return;
        if (state.playback == MusicalObjectSound::Playback::superCollider) state.scSource = sourceEditor.getText();
        if (state.playback == MusicalObjectSound::Playback::pureData) state.pdSource = sourceEditor.getText();
        changed();
    };
    resetButton.onClick = [this]
    {
        if (state.playback == MusicalObjectSound::Playback::superCollider) state.scSource = defaultScSource();
        if (state.playback == MusicalObjectSound::Playback::pureData) state.pdSource = defaultPdSource();
        refresh();
        changed();
    };
    previewButton.onClick = [this] { if (onPreview) onPreview (state); };

    refresh();
}

void MusicalObjectEditorComponent::paint (juce::Graphics& g)
{
    g.fillAll (background);
    g.setColour (stroke.withAlpha (0.65f));
    g.drawHorizontalLine (56, 18.0f, (float) getWidth() - 18.0f);
}

void MusicalObjectEditorComponent::resized()
{
    auto area = getLocalBounds().reduced (18);
    titleLabel.setBounds (area.removeFromTop (32));
    area.removeFromTop (18);
    playbackLabel.setBounds (area.removeFromTop (18));
    playbackBox.setBounds (area.removeFromTop (34));
    area.removeFromTop (10);
    pitchLabel.setBounds (area.removeFromTop (18));
    pitchSlider.setBounds (area.removeFromTop (34));
    area.removeFromTop (10);
    durationLabel.setBounds (area.removeFromTop (18));
    durationSlider.setBounds (area.removeFromTop (34));
    area.removeFromTop (10);
    sourceLabel.setBounds (area.removeFromTop (18));
    auto actions = area.removeFromBottom (36);
    previewButton.setBounds (actions.removeFromRight (112));
    actions.removeFromRight (8);
    resetButton.setBounds (actions.removeFromRight (132));
    area.removeFromBottom (10);
    sourceEditor.setBounds (area);
}

void MusicalObjectEditorComponent::refresh()
{
    updating = true;
    playbackBox.setSelectedItemIndex ((int) state.playback, juce::dontSendNotification);
    pitchSlider.setValue (state.midiNote, juce::dontSendNotification);
    durationSlider.setValue (state.durationSeconds, juce::dontSendNotification);
    const auto hasSource = state.playback != MusicalObjectSound::Playback::synth;
    sourceLabel.setVisible (hasSource);
    sourceEditor.setVisible (hasSource);
    resetButton.setVisible (hasSource);
    sourceEditor.setText (state.playback == MusicalObjectSound::Playback::superCollider ? state.scSource : state.pdSource, false);
    updating = false;
}

void MusicalObjectEditorComponent::changed()
{
    if (onChange) onChange (state);
}

juce::String MusicalObjectEditorComponent::defaultScSource()
{
    return "var env = EnvGen.kr(Env.perc(0.005, sustain.max(0.08)), doneAction: 2);\n"
           "var body = SinOsc.ar(pitch.midicps) + (Pulse.ar(pitch.midicps * 0.5, 0.35) * 0.12);\n"
           "Out.ar(out, Pan2.ar(body * env * amp, pan));";
}

juce::String MusicalObjectEditorComponent::defaultPdSource()
{
    return R"PD(#N canvas 120 120 420 300 10;
#X obj 40 36 r trigger;
#X obj 40 72 t f b;
#X obj 40 108 mtof;
#X obj 40 144 osc~ 440;
#X msg 152 108 0 \, 0.22 5 \, 0 420 5;
#X obj 152 144 vline~;
#X obj 88 184 *~;
#X obj 88 224 dac~;
#X connect 0 0 1 0;
#X connect 1 0 2 0;
#X connect 1 1 4 0;
#X connect 2 0 3 0;
#X connect 3 0 6 0;
#X connect 4 0 5 0;
#X connect 5 0 6 1;
#X connect 6 0 7 0;
#X connect 6 0 7 1;
)PD";
}
