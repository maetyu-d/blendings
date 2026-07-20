#pragma once

#include <JuceHeader.h>

#include "AppTheme.h"
#include "InspectorStyle.h"
#include "WorkspaceModel.h"

namespace blendings
{
using namespace ui;

class ClockSettingsComponent final : public juce::Component
{
public:
    using ClockBank = std::array<SequencingClock, 4>;

    ClockSettingsComponent (ClockBank initial, std::function<void(const ClockBank&)> changed)
        : clocks (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText ("Clocks", juce::dontSendNotification);
        addAndMakeVisible (title);

        styleEditorLabel (clockLabel, 12.0f, false);
        clockLabel.setText ("Auxiliary clock", juce::dontSendNotification);
        addAndMakeVisible (clockLabel);
        addAndMakeVisible (clockChoice);
        rebuildClockChoices();

        enabled.setButtonText ("Enabled");
        enabled.setColour (juce::ToggleButton::textColourId, textPrimary());
        addAndMakeVisible (enabled);

        styleEditorLabel (nameLabel, 12.0f, false);
        nameLabel.setText ("Name", juce::dontSendNotification);
        addAndMakeVisible (nameLabel);
        name.setColour (juce::TextEditor::backgroundColourId, raisedSurface());
        name.setColour (juce::TextEditor::textColourId, textPrimary());
        name.setColour (juce::TextEditor::outlineColourId, subtleStroke());
        name.setColour (juce::TextEditor::focusedOutlineColourId, accentColour());
        name.setIndents (8, 4);
        name.setSelectAllWhenFocused (true);
        addAndMakeVisible (name);

        configureSlider (ratioLabel, ratio, "Rate", 0.125, 8.0, 0.125, "x");
        configureSlider (phaseLabel, phase, "Phase", 0.0, 16.0, 0.125, " beats");
        configureSlider (swingLabel, swing, "Swing", 0.0, 75.0, 1.0, "%");

        clockChoice.onChange = [this] { selected = juce::jlimit (0, 3, clockChoice.getSelectedId() - 1); loadSelected(); };
        enabled.onClick = [this] { if (! updating) { clocks[static_cast<size_t> (selected)].enabled = enabled.getToggleState(); controlsChanged(); } };
        name.onFocusLost = [this] { commitName(); };
        name.onReturnKey = [this] { commitName(); name.giveAwayKeyboardFocus(); };
        ratio.onValueChange = [this] { if (! updating) { clocks[static_cast<size_t> (selected)].ratio = ratio.getValue(); controlsChanged(); } };
        phase.onValueChange = [this] { if (! updating) { clocks[static_cast<size_t> (selected)].phaseBeats = phase.getValue(); controlsChanged(); } };
        swing.onValueChange = [this] { if (! updating) { clocks[static_cast<size_t> (selected)].swing = swing.getValue() / 100.0; controlsChanged(); } };

        clockChoice.setSelectedId (1, juce::dontSendNotification);
        loadSelected();
        setSize (380, 276);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (27));
        area.removeFromTop (5);
        auto row = area.removeFromTop (38);
        clockLabel.setBounds (row.removeFromLeft (112));
        clockChoice.setBounds (row.reduced (0, 3));
        area.removeFromTop (2);
        enabled.setBounds (area.removeFromTop (30).withTrimmedLeft (112));
        row = area.removeFromTop (38);
        nameLabel.setBounds (row.removeFromLeft (112));
        name.setBounds (row.reduced (0, 3));
        layoutSliderRow (area, ratioLabel, ratio);
        layoutSliderRow (area, phaseLabel, phase);
        layoutSliderRow (area, swingLabel, swing);
    }

private:
    ClockBank clocks;
    std::function<void(const ClockBank&)> onChange;
    int selected = 0;
    bool updating = false;
    juce::Label title, clockLabel, nameLabel, ratioLabel, phaseLabel, swingLabel;
    juce::ComboBox clockChoice;
    juce::ToggleButton enabled;
    juce::TextEditor name;
    juce::Slider ratio, phase, swing;

    void configureSlider (juce::Label& label, juce::Slider& slider, const juce::String& text,
                          double min, double max, double step, const juce::String& suffix)
    {
        styleEditorLabel (label, 12.0f, false);
        label.setText (text, juce::dontSendNotification);
        addAndMakeVisible (label);
        slider.setRange (min, max, step);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 26);
        slider.setTextValueSuffix (suffix);
        slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff53d8fb));
        addAndMakeVisible (slider);
    }

    static void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
    {
        auto row = area.removeFromTop (39);
        label.setBounds (row.removeFromLeft (112));
        slider.setBounds (row);
    }

    void rebuildClockChoices()
    {
        const auto oldSelection = juce::jmax (1, clockChoice.getSelectedId());
        clockChoice.clear (juce::dontSendNotification);
        for (int i = 0; i < static_cast<int> (clocks.size()); ++i)
        {
            auto label = juce::String (i + 1) + "  " + clocks[static_cast<size_t> (i)].name;
            clockChoice.addItem (label, i + 1);
        }
        clockChoice.setSelectedId (juce::jlimit (1, 4, oldSelection), juce::dontSendNotification);
    }

    void loadSelected()
    {
        const juce::ScopedValueSetter<bool> guard (updating, true);
        const auto& clock = clocks[static_cast<size_t> (selected)];
        enabled.setToggleState (clock.enabled, juce::dontSendNotification);
        name.setText (clock.name, false);
        ratio.setValue (clock.ratio, juce::dontSendNotification);
        phase.setValue (clock.phaseBeats, juce::dontSendNotification);
        swing.setValue (clock.swing * 100.0, juce::dontSendNotification);
        updateEnabledState();
    }

    void updateEnabledState()
    {
        const auto isEnabled = clocks[static_cast<size_t> (selected)].enabled;
        name.setEnabled (isEnabled);
        ratio.setEnabled (isEnabled); phase.setEnabled (isEnabled); swing.setEnabled (isEnabled);
        nameLabel.setEnabled (isEnabled); ratioLabel.setEnabled (isEnabled);
        phaseLabel.setEnabled (isEnabled); swingLabel.setEnabled (isEnabled);
    }

    void commitName()
    {
        if (updating) return;
        auto entered = name.getText().trim();
        if (entered.isEmpty()) entered = "Clock " + juce::String (selected + 1);
        clocks[static_cast<size_t> (selected)].name = entered;
        name.setText (entered, false);
        rebuildClockChoices();
        controlsChanged();
    }

    void controlsChanged()
    {
        updateEnabledState();
        if (onChange) onChange (clocks);
    }
};

class ModulatorSettingsComponent final : public juce::Component
{
public:
    ModulatorSettingsComponent (Modulator initial, juce::StringArray clockNames, std::function<void(const Modulator&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText ("Modulator", juce::dontSendNotification);
        addAndMakeVisible (title);

        enabled.setButtonText ("Enabled");
        enabled.setToggleState (value.enabled, juce::dontSendNotification);
        enabled.setColour (juce::ToggleButton::textColourId, textPrimary());
        enabled.onClick = [this] { value.enabled = enabled.getToggleState(); notify(); updateEnabledState(); };
        addAndMakeVisible (enabled);

        styleEditorLabel (nameLabel, 12.0f, false);
        nameLabel.setText ("Name", juce::dontSendNotification);
        addAndMakeVisible (nameLabel);
        name.setText (value.name, false);
        name.setSelectAllWhenFocused (true);
        name.setColour (juce::TextEditor::backgroundColourId, raisedSurface());
        name.setColour (juce::TextEditor::textColourId, textPrimary());
        name.setColour (juce::TextEditor::outlineColourId, subtleStroke());
        name.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xffd884ff));
        name.onFocusLost = [this] { commitName(); };
        name.onReturnKey = [this] { commitName(); name.giveAwayKeyboardFocus(); };
        addAndMakeVisible (name);

        styleEditorLabel (shapeLabel, 12.0f, false);
        shapeLabel.setText ("Shape", juce::dontSendNotification);
        addAndMakeVisible (shapeLabel);
        shape.addItemList ({ "Sine", "Triangle", "Square", "Random steps", "Saw up", "Saw down", "Smooth random" }, 1);
        shape.setSelectedId (static_cast<int> (value.shape) + 1, juce::dontSendNotification);
        shape.onChange = [this]
        {
            value.shape = static_cast<Modulator::Shape> (juce::jlimit (0, 6, shape.getSelectedId() - 1));
            notify();
        };
        addAndMakeVisible (shape);

        styleEditorLabel (clockLabel, 12.0f, false); clockLabel.setText ("Clock", juce::dontSendNotification); addAndMakeVisible (clockLabel);
        clock.addItemList (clockNames, 1); clock.setSelectedId (juce::jlimit (1, clock.getNumItems(), value.clockIndex + 1), juce::dontSendNotification);
        clock.onChange = [this] { value.clockIndex = juce::jmax (0, clock.getSelectedId() - 1); notify(); }; addAndMakeVisible (clock);

        bipolar.setButtonText ("Bipolar output (- / +)");
        bipolar.setToggleState (value.bipolar, juce::dontSendNotification);
        bipolar.setColour (juce::ToggleButton::textColourId, textPrimary());
        bipolar.onClick = [this] { value.bipolar = bipolar.getToggleState(); notify(); };
        addAndMakeVisible (bipolar);

        configureSlider (cycleLabel, cycle, "Cycle", 0.125, 64.0, 0.125, value.cycleBeats, " beats");
        configureSlider (phaseLabel, phase, "Phase", 0.0, 360.0, 1.0, value.phase * 360.0, " deg");
        cycle.onValueChange = [this] { value.cycleBeats = cycle.getValue(); notify(); };
        phase.onValueChange = [this] { value.phase = phase.getValue() / 360.0; notify(); };
        setSize (370, 320);
        updateEnabledState();
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (27));
        enabled.setBounds (area.removeFromTop (28));
        layoutEditorRow (area, nameLabel, name);
        auto row = area.removeFromTop (39);
        shapeLabel.setBounds (row.removeFromLeft (96));
        shape.setBounds (row.reduced (0, 4));
        row = area.removeFromTop (39);
        clockLabel.setBounds (row.removeFromLeft (96));
        clock.setBounds (row.reduced (0, 4));
        layoutSliderRow (area, cycleLabel, cycle);
        layoutSliderRow (area, phaseLabel, phase);
        bipolar.setBounds (area.removeFromTop (30).withTrimmedLeft (96));
    }

private:
    Modulator value;
    std::function<void(const Modulator&)> onChange;
    juce::Label title, nameLabel, shapeLabel, clockLabel, cycleLabel, phaseLabel;
    juce::ToggleButton enabled, bipolar;
    juce::TextEditor name;
    juce::ComboBox shape, clock;
    juce::Slider cycle, phase;

    void configureSlider (juce::Label& label, juce::Slider& slider, const juce::String& text,
                          double min, double max, double step, double current, const juce::String& suffix)
    {
        styleEditorLabel (label, 12.0f, false);
        label.setText (text, juce::dontSendNotification);
        addAndMakeVisible (label);
        slider.setRange (min, max, step);
        slider.setValue (current, juce::dontSendNotification);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 82, 26);
        slider.setTextValueSuffix (suffix);
        slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffd884ff));
        addAndMakeVisible (slider);
    }

    static void layoutEditorRow (juce::Rectangle<int>& area, juce::Label& label, juce::TextEditor& editor)
    {
        auto row = area.removeFromTop (39);
        label.setBounds (row.removeFromLeft (96));
        editor.setBounds (row.reduced (0, 4));
    }

    static void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
    {
        auto row = area.removeFromTop (39);
        label.setBounds (row.removeFromLeft (96));
        slider.setBounds (row);
    }

    void commitName()
    {
        auto entered = name.getText().trim();
        if (entered.isEmpty()) entered = "Modulator";
        value.name = entered;
        name.setText (entered, false);
        notify();
    }

    void updateEnabledState()
    {
        for (auto* component : std::array<juce::Component*, 7> { &name, &shape, &clock, &cycle, &phase, &bipolar, &clockLabel })
            component->setEnabled (value.enabled);
        nameLabel.setEnabled (value.enabled); shapeLabel.setEnabled (value.enabled);
        cycleLabel.setEnabled (value.enabled); phaseLabel.setEnabled (value.enabled);
    }

    void notify() { if (onChange) onChange (value); }
};

class ModulationConnectionSettingsComponent final : public juce::Component
{
public:
    ModulationConnectionSettingsComponent (ModulationConnection initial,
                                           std::function<void(const ModulationConnection&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText (modulationTargetName (value.targetKind), juce::dontSendNotification);
        addAndMakeVisible (title);
        styleEditorLabel (parameterLabel, 12.0f, false);
        parameterLabel.setText ("Parameter", juce::dontSendNotification);
        addAndMakeVisible (parameterLabel);
        parameter.addItemList (modulationParameterNames (value.targetKind), 1);
        parameter.setSelectedId (juce::jlimit (1, parameter.getNumItems(), value.parameter + 1), juce::dontSendNotification);
        parameter.onChange = [this] { value.parameter = juce::jmax (0, parameter.getSelectedId() - 1); notify(); };
        addAndMakeVisible (parameter);

        enabled.setButtonText ("Route enabled");
        enabled.setToggleState (value.enabled, juce::dontSendNotification);
        enabled.setColour (juce::ToggleButton::textColourId, textPrimary());
        enabled.onClick = [this] { value.enabled = enabled.getToggleState(); notify(); updateEnabledState(); };
        addAndMakeVisible (enabled);

        styleEditorLabel (curveLabel, 12.0f, false);
        curveLabel.setText ("Response", juce::dontSendNotification);
        addAndMakeVisible (curveLabel);
        curve.addItemList ({ "Linear", "Ease in", "Ease out", "Smooth S-curve" }, 1);
        curve.setSelectedId (static_cast<int> (value.curve) + 1, juce::dontSendNotification);
        curve.onChange = [this]
        {
            value.curve = static_cast<ModulationConnection::Curve> (juce::jlimit (0, 3, curve.getSelectedId() - 1));
            notify();
        };
        addAndMakeVisible (curve);

        styleEditorLabel (depthLabel, 12.0f, false);
        depthLabel.setText ("Depth", juce::dontSendNotification);
        addAndMakeVisible (depthLabel);
        depth.setRange (-100.0, 100.0, 1.0);
        depth.setValue (value.depth * 100.0, juce::dontSendNotification);
        depth.setSliderStyle (juce::Slider::LinearHorizontal);
        depth.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 26);
        depth.setTextValueSuffix ("%");
        depth.setColour (juce::Slider::thumbColourId, juce::Colour (0xffd884ff));
        depth.onValueChange = [this] { value.depth = depth.getValue() / 100.0; notify(); };
        addAndMakeVisible (depth);

        styleEditorLabel (offsetLabel, 12.0f, false);
        offsetLabel.setText ("Offset", juce::dontSendNotification);
        addAndMakeVisible (offsetLabel);
        offset.setRange (-100.0, 100.0, 1.0);
        offset.setValue (value.offset * 100.0, juce::dontSendNotification);
        offset.setSliderStyle (juce::Slider::LinearHorizontal);
        offset.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 26);
        offset.setTextValueSuffix ("%");
        offset.setColour (juce::Slider::thumbColourId, juce::Colour (0xff65e6d4));
        offset.onValueChange = [this] { value.offset = offset.getValue() / 100.0; notify(); };
        addAndMakeVisible (offset);

        styleEditorLabel (smoothingLabel, 12.0f, false);
        smoothingLabel.setText ("Smoothing", juce::dontSendNotification); addAndMakeVisible (smoothingLabel);
        smoothing.setRange (0.0, 8.0, 0.0625); smoothing.setValue (value.smoothingBeats, juce::dontSendNotification);
        smoothing.setSliderStyle (juce::Slider::LinearHorizontal);
        smoothing.setTextBoxStyle (juce::Slider::TextBoxRight, false, 82, 26);
        smoothing.setTextValueSuffix (" beats"); smoothing.setColour (juce::Slider::thumbColourId, juce::Colour (0xffffc857));
        smoothing.onValueChange = [this] { value.smoothingBeats = smoothing.getValue(); notify(); }; addAndMakeVisible (smoothing);

        inverted.setButtonText ("Invert modulation");
        inverted.setToggleState (value.inverted, juce::dontSendNotification);
        inverted.setColour (juce::ToggleButton::textColourId, textPrimary());
        inverted.onClick = [this] { value.inverted = inverted.getToggleState(); notify(); };
        addAndMakeVisible (inverted);
        setSize (350, 320);
        updateEnabledState();
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (28));
        enabled.setBounds (area.removeFromTop (30).withTrimmedLeft (92));
        auto row = area.removeFromTop (39);
        parameterLabel.setBounds (row.removeFromLeft (92));
        parameter.setBounds (row.reduced (0, 4));
        row = area.removeFromTop (39);
        curveLabel.setBounds (row.removeFromLeft (92));
        curve.setBounds (row.reduced (0, 4));
        row = area.removeFromTop (39);
        depthLabel.setBounds (row.removeFromLeft (92));
        depth.setBounds (row);
        row = area.removeFromTop (39);
        offsetLabel.setBounds (row.removeFromLeft (92));
        offset.setBounds (row);
        row = area.removeFromTop (39);
        smoothingLabel.setBounds (row.removeFromLeft (92));
        smoothing.setBounds (row);
        inverted.setBounds (area.removeFromTop (30).withTrimmedLeft (92));
    }

private:
    ModulationConnection value;
    std::function<void(const ModulationConnection&)> onChange;
    juce::Label title, parameterLabel, curveLabel, depthLabel, offsetLabel, smoothingLabel;
    juce::ComboBox parameter, curve;
    juce::Slider depth, offset, smoothing;
    juce::ToggleButton enabled, inverted;
    void updateEnabledState()
    {
        for (auto* component : std::array<juce::Component*, 9> { &parameter, &curve, &depth, &offset, &smoothing,
                                                                  &inverted, &parameterLabel, &curveLabel, &depthLabel })
            component->setEnabled (value.enabled);
        offsetLabel.setEnabled (value.enabled); smoothingLabel.setEnabled (value.enabled);
    }
    void notify() { if (onChange) onChange (value); }
};

class PipeSettingsComponent final : public juce::Component
{
public:
    PipeSettingsComponent (bool warp, float lengthInGridUnits, std::function<void(bool)> changed)
        : onChange (std::move (changed))
    {
        styleEditorLabel (title, 16.0f, true); title.setText ("Pipe", juce::dontSendNotification); addAndMakeVisible (title);
        styleEditorLabel (typeLabel, 12.0f, false); typeLabel.setText ("Type", juce::dontSendNotification); addAndMakeVisible (typeLabel);
        type.addItemList ({ "Flow pipe", "Warp pipe" }, 1);
        type.setSelectedId (warp ? 2 : 1, juce::dontSendNotification);
        type.onChange = [this] { if (onChange) onChange (type.getSelectedId() == 2); };
        addAndMakeVisible (type);
        styleEditorLabel (lengthLabel, 12.0f, false); lengthLabel.setText ("Length", juce::dontSendNotification); addAndMakeVisible (lengthLabel);
        styleEditorLabel (lengthValue, 12.0f, true);
        lengthValue.setText (juce::String (lengthInGridUnits, 1) + " grid units", juce::dontSendNotification);
        lengthValue.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (lengthValue);
        setSize (340, 124);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (27)); area.removeFromTop (5);
        auto row = area.removeFromTop (38); typeLabel.setBounds (row.removeFromLeft (92)); type.setBounds (row.reduced (0, 3));
        row = area.removeFromTop (30); lengthLabel.setBounds (row.removeFromLeft (92)); lengthValue.setBounds (row);
    }

private:
    std::function<void(bool)> onChange;
    juce::Label title, typeLabel, lengthLabel, lengthValue;
    juce::ComboBox type;
};

class SelectionInspectorComponent final : public juce::Component
{
public:
    SelectionInspectorComponent (juce::String selectionName, int selectionCount,
                                 std::function<void()> duplicateSelection,
                                 std::function<void()> deleteSelection)
    {
        styleEditorLabel (title, 16.0f, true);
        title.setText (std::move (selectionName), juce::dontSendNotification);
        addAndMakeVisible (title);
        styleEditorLabel (summary, 12.0f, false);
        summary.setText (selectionCount == 1 ? "1 selected" : juce::String (selectionCount) + " selected",
                         juce::dontSendNotification);
        addAndMakeVisible (summary);
        styleEditorButton (duplicate, "Duplicate");
        styleEditorButton (remove, "Delete");
        duplicate.onClick = std::move (duplicateSelection);
        remove.onClick = std::move (deleteSelection);
        addAndMakeVisible (duplicate);
        addAndMakeVisible (remove);
        setSize (340, 126);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (26));
        summary.setBounds (area.removeFromTop (22));
        area.removeFromTop (12);
        auto buttons = area.removeFromTop (32);
        const auto width = (buttons.getWidth() - 8) / 2;
        duplicate.setBounds (buttons.removeFromLeft (width));
        buttons.removeFromLeft (8);
        remove.setBounds (buttons);
    }

private:
    juce::Label title, summary;
    juce::TextButton duplicate, remove;
};

} // namespace blendings
