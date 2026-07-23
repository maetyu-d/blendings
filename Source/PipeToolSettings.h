#pragma once

#include <JuceHeader.h>

#include "AppTheme.h"
#include "InspectorStyle.h"
#include "WorkspaceModel.h"

namespace blendings
{
using namespace ui;

class TapSettingsComponent final : public juce::Component
{
public:
    TapSettingsComponent (PipeTap initial, juce::StringArray clockNames, std::function<void(const PipeTap&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 16.0f, true); title.setText ("Tap", juce::dontSendNotification); addAndMakeVisible (title);
        configureSectionLabel (timingSection, "TIMING");
        configureSectionLabel (speedSection, "SPEED");
        configureSectionLabel (outputSection, "OUTPUT");
        styleEditorLabel (clockLabel, 12.0f, false); clockLabel.setText ("Clock", juce::dontSendNotification); addAndMakeVisible (clockLabel);
        clock.addItemList (clockNames, 1);
        clock.setSelectedId (juce::jlimit (0, juce::jmax (0, clockNames.size() - 1), value.clockIndex) + 1, juce::dontSendNotification);
        clock.setTooltip ("Clock that schedules this tap"); addAndMakeVisible (clock);
        configureSlider (interval, intervalLabel, "Every", 0.25, 8.0, 0.25, value.intervalBeats, " beats");
        configureSlider (total, totalLabel, "Drop count", 0.0, 99.0, 1.0, value.totalDrops, "");
        total.setTooltip ("0 means no limit");
        randomInterval.setButtonText ("Vary interval");
        randomInterval.setToggleState (value.randomInterval, juce::dontSendNotification);
        randomInterval.setColour (juce::ToggleButton::textColourId, textPrimary()); addAndMakeVisible (randomInterval);
        configureSlider (low, lowLabel, "Minimum", 0.25, 32.0, 0.25, value.intervalLowBeats, " beats");
        configureSlider (high, highLabel, "Maximum", 0.25, 32.0, 0.25, value.intervalHighBeats, " beats");
        configureSlider (speed, speedLabel, "Multiplier", 0.25, maxDropSpeedMultiplier, 0.05, value.speed, "x");
        randomSpeed.setButtonText ("Vary speed");
        randomSpeed.setToggleState (value.randomSpeed, juce::dontSendNotification);
        randomSpeed.setColour (juce::ToggleButton::textColourId, textPrimary()); addAndMakeVisible (randomSpeed);
        configureSlider (speedLow, speedLowLabel, "Minimum", 0.25, maxDropSpeedMultiplier, 0.05, value.speedLow, "x");
        configureSlider (speedHigh, speedHighLabel, "Maximum", 0.25, maxDropSpeedMultiplier, 0.05, value.speedHigh, "x");
        configureSlider (probability, probabilityLabel, "Emit chance", 0.0, 100.0, 1.0, value.probability * 100.0, "%");
        styleEditorLabel (directionLabel, 12.0f, false); directionLabel.setText ("Direction", juce::dontSendNotification); addAndMakeVisible (directionLabel);
        enabled.setButtonText ("Enabled"); enabled.setToggleState (value.enabled, juce::dontSendNotification); enabled.setColour (juce::ToggleButton::textColourId, textPrimary()); addAndMakeVisible (enabled);
        direction.addItem ("Along pipe", 1); direction.addItem ("Backwards", 2); direction.setSelectedId (value.reverse ? 2 : 1, juce::dontSendNotification); addAndMakeVisible (direction);
        enabled.onClick = [this] { value.enabled = enabled.getToggleState(); updateControlState(); notify(); };
        clock.onChange = [this] { value.clockIndex = juce::jmax (0, clock.getSelectedId() - 1); notify(); };
        direction.onChange = [this] { value.reverse = direction.getSelectedId() == 2; notify(); };
        interval.onValueChange = [this] { value.intervalBeats = interval.getValue(); notify(); };
        total.onValueChange = [this] { value.totalDrops = static_cast<int> (total.getValue()); notify(); };
        randomInterval.onClick = [this] { value.randomInterval = randomInterval.getToggleState(); updateControlState(); notify(); };
        low.onValueChange = [this]
        {
            value.intervalLowBeats = low.getValue();
            if (high.getValue() < low.getValue()) high.setValue (low.getValue(), juce::sendNotificationSync);
            notify();
        };
        high.onValueChange = [this]
        {
            value.intervalHighBeats = high.getValue();
            if (low.getValue() > high.getValue()) low.setValue (high.getValue(), juce::sendNotificationSync);
            notify();
        };
        speed.onValueChange = [this] { value.speed = speed.getValue(); notify(); };
        randomSpeed.onClick = [this] { value.randomSpeed = randomSpeed.getToggleState(); updateControlState(); notify(); };
        speedLow.onValueChange = [this]
        {
            value.speedLow = speedLow.getValue();
            if (speedHigh.getValue() < speedLow.getValue()) speedHigh.setValue (speedLow.getValue(), juce::sendNotificationSync);
            notify();
        };
        speedHigh.onValueChange = [this]
        {
            value.speedHigh = speedHigh.getValue();
            if (speedLow.getValue() > speedHigh.getValue()) speedLow.setValue (speedHigh.getValue(), juce::sendNotificationSync);
            notify();
        };
        probability.onValueChange = [this] { value.probability = probability.getValue() / 100.0; notify(); };
        updateControlState();
        setSize (360, 609);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (surfaceColour());
        g.setColour (subtleStroke().withAlpha (0.55f));
        g.drawHorizontalLine (timingSection.getBottom() + 2, 14.0f, static_cast<float> (getWidth() - 14));
        g.drawHorizontalLine (speedSection.getBottom() + 2, 14.0f, static_cast<float> (getWidth() - 14));
        g.drawHorizontalLine (outputSection.getBottom() + 2, 14.0f, static_cast<float> (getWidth() - 14));
    }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 10);
        auto heading = area.removeFromTop (28);
        title.setBounds (heading.removeFromLeft (170));
        enabled.setBounds (heading);
        area.removeFromTop (5);
        timingSection.setBounds (area.removeFromTop (24));
        area.removeFromTop (4);
        auto clockRow = area.removeFromTop (39);
        clockLabel.setBounds (clockRow.removeFromLeft (112));
        clock.setBounds (clockRow.reduced (0, 3));
        layoutRow (area, intervalLabel, interval);
        layoutRow (area, totalLabel, total);
        randomInterval.setBounds (area.removeFromTop (30).withTrimmedLeft (112));
        layoutRow (area, lowLabel, low);
        layoutRow (area, highLabel, high);
        area.removeFromTop (5);
        speedSection.setBounds (area.removeFromTop (24));
        area.removeFromTop (4);
        layoutRow (area, speedLabel, speed);
        randomSpeed.setBounds (area.removeFromTop (30).withTrimmedLeft (112));
        layoutRow (area, speedLowLabel, speedLow);
        layoutRow (area, speedHighLabel, speedHigh);
        area.removeFromTop (5);
        outputSection.setBounds (area.removeFromTop (24));
        area.removeFromTop (4);
        layoutRow (area, probabilityLabel, probability);
        directionLabel.setBounds (area.removeFromLeft (112).removeFromTop (32));
        direction.setBounds (area.removeFromTop (32));
    }

private:
    PipeTap value;
    std::function<void(const PipeTap&)> onChange;
    juce::Label title, timingSection, speedSection, outputSection, clockLabel, intervalLabel, totalLabel, lowLabel, highLabel, speedLabel, speedLowLabel, speedHighLabel, probabilityLabel, directionLabel;
    juce::Slider interval, total, low, high, speed, speedLow, speedHigh, probability;
    juce::ToggleButton enabled, randomInterval, randomSpeed;
    juce::ComboBox clock, direction;

    void configureSectionLabel (juce::Label& label, const juce::String& text)
    {
        styleEditorLabel (label, 10.5f, true);
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, pipeElementColour().withAlpha (0.88f));
        addAndMakeVisible (label);
    }

    void configureSlider (juce::Slider& slider, juce::Label& label, const juce::String& name, double min, double max, double step, double current, const juce::String& suffix)
    {
        styleEditorLabel (label, 12.0f, false); label.setText (name, juce::dontSendNotification); addAndMakeVisible (label);
        slider.setName (name); slider.setRange (min, max, step); slider.setValue (current, juce::dontSendNotification); slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 76, 26); slider.setTextValueSuffix (suffix); slider.setColour (juce::Slider::thumbColourId, pipeElementColour());
        slider.setTooltip (name); addAndMakeVisible (slider);
    }
    static void layoutRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
    {
        auto row = area.removeFromTop (39);
        label.setBounds (row.removeFromLeft (112));
        slider.setBounds (row);
    }
    void updateControlState()
    {
        interval.setEnabled (value.enabled && ! value.randomInterval); total.setEnabled (value.enabled);
        randomInterval.setEnabled (value.enabled); low.setEnabled (value.enabled && value.randomInterval); high.setEnabled (value.enabled && value.randomInterval);
        speed.setEnabled (value.enabled && ! value.randomSpeed); randomSpeed.setEnabled (value.enabled);
        speedLow.setEnabled (value.enabled && value.randomSpeed); speedHigh.setEnabled (value.enabled && value.randomSpeed);
        clock.setEnabled (value.enabled); probability.setEnabled (value.enabled); direction.setEnabled (value.enabled);
        intervalLabel.setEnabled (value.enabled && ! value.randomInterval); totalLabel.setEnabled (value.enabled);
        lowLabel.setEnabled (value.enabled && value.randomInterval); highLabel.setEnabled (value.enabled && value.randomInterval);
        speedLabel.setEnabled (value.enabled && ! value.randomSpeed);
        speedLowLabel.setEnabled (value.enabled && value.randomSpeed); speedHighLabel.setEnabled (value.enabled && value.randomSpeed);
        clockLabel.setEnabled (value.enabled); probabilityLabel.setEnabled (value.enabled); directionLabel.setEnabled (value.enabled);
    }
    void notify() { if (onChange) onChange (value); }
};

class DrainSettingsComponent final : public juce::Component
{
public:
    DrainSettingsComponent (PipeDrain initial, std::function<void(const PipeDrain&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText ("Drain", juce::dontSendNotification);
        addAndMakeVisible (title);
        styleEditorLabel (chanceLabel, 12.0f, false);
        chanceLabel.setText ("Destroy passing drops", juce::dontSendNotification);
        addAndMakeVisible (chanceLabel);
        chance.setRange (0.0, 100.0, 1.0);
        chance.setValue (value.destructionProbability * 100.0, juce::dontSendNotification);
        chance.setSliderStyle (juce::Slider::LinearHorizontal);
        chance.setTextBoxStyle (juce::Slider::TextBoxRight, false, 66, 26);
        chance.setTextValueSuffix ("%");
        chance.setColour (juce::Slider::thumbColourId, pdPatchElementColour());
        chance.onValueChange = [this]
        {
            value.destructionProbability = chance.getValue() / 100.0;
            if (onChange) onChange (value);
        };
        addAndMakeVisible (chance);
        setSize (310, 112);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (25));
        area.removeFromTop (8);
        chanceLabel.setBounds (area.removeFromTop (18));
        chance.setBounds (area.removeFromTop (34));
    }

private:
    PipeDrain value;
    std::function<void(const PipeDrain&)> onChange;
    juce::Label title, chanceLabel;
    juce::Slider chance;
};

class ClonerSettingsComponent final : public juce::Component
{
public:
    ClonerSettingsComponent (PipeCloner initial, std::function<void(const PipeCloner&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText ("Quantum directions", juce::dontSendNotification);
        addAndMakeVisible (title);
        configureRow (straightLabel, straight, "Straight pipe", value.straightMode);
        configureRow (twoLabel, two, "2-way junction", value.twoWayMode);
        configureRow (threeLabel, three, "3-way junction", value.threeWayMode);
        configureRow (fourLabel, four, "4-way junction", value.fourWayMode);
        configureSlider (maxClonesLabel, maxClones, "Maximum clones", 1.0, 16.0, 1.0, value.maxClones, "");
        configureSlider (probabilityLabel, probability, "Chance per clone", 0.0, 100.0, 1.0,
                         value.cloneProbability * 100.0, "%");
        straight.onChange = [this] { value.straightMode = selectedMode (straight); notify(); };
        two.onChange = [this] { value.twoWayMode = selectedMode (two); notify(); };
        three.onChange = [this] { value.threeWayMode = selectedMode (three); notify(); };
        four.onChange = [this] { value.fourWayMode = selectedMode (four); notify(); };
        maxClones.onValueChange = [this] { value.maxClones = static_cast<int> (maxClones.getValue()); notify(); };
        probability.onValueChange = [this] { value.cloneProbability = probability.getValue() / 100.0; notify(); };
        setSize (340, 318);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (27));
        area.removeFromTop (6);
        layoutRow (area, straightLabel, straight);
        layoutRow (area, twoLabel, two);
        layoutRow (area, threeLabel, three);
        layoutRow (area, fourLabel, four);
        layoutSliderRow (area, maxClonesLabel, maxClones);
        layoutSliderRow (area, probabilityLabel, probability);
    }

private:
    PipeCloner value;
    std::function<void(const PipeCloner&)> onChange;
    juce::Label title, straightLabel, twoLabel, threeLabel, fourLabel, maxClonesLabel, probabilityLabel;
    juce::ComboBox straight, two, three, four;
    juce::Slider maxClones, probability;

    void configureRow (juce::Label& label, juce::ComboBox& box, const juce::String& text,
                       PipeCloner::DirectionMode mode)
    {
        styleEditorLabel (label, 12.0f, false);
        label.setText (text, juce::dontSendNotification);
        addAndMakeVisible (label);
        box.addItemList ({ "All directions", "Random direction", "Straightest", "Leftmost", "Rightmost" }, 1);
        box.setSelectedId (static_cast<int> (mode) + 1, juce::dontSendNotification);
        addAndMakeVisible (box);
    }

    static PipeCloner::DirectionMode selectedMode (const juce::ComboBox& box)
    {
        return static_cast<PipeCloner::DirectionMode> (juce::jlimit (0, 4, box.getSelectedId() - 1));
    }

    void configureSlider (juce::Label& label, juce::Slider& slider, const juce::String& text,
                          double min, double max, double step, double current, const juce::String& suffix)
    {
        styleEditorLabel (label, 12.0f, false);
        label.setText (text, juce::dontSendNotification);
        addAndMakeVisible (label);
        slider.setRange (min, max, step);
        slider.setValue (current, juce::dontSendNotification);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 26);
        slider.setTextValueSuffix (suffix);
        slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffb888ff));
        addAndMakeVisible (slider);
    }

    static void layoutRow (juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& box)
    {
        auto row = area.removeFromTop (43);
        label.setBounds (row.removeFromLeft (124));
        box.setBounds (row.reduced (0, 5));
    }

    static void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
    {
        auto row = area.removeFromTop (43);
        label.setBounds (row.removeFromLeft (124));
        slider.setBounds (row);
    }

    void notify() { if (onChange) onChange (value); }
};

class SpeedLimitSettingsComponent final : public juce::Component
{
public:
    SpeedLimitSettingsComponent (PipeSpeedLimit initial, std::function<void(const PipeSpeedLimit&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText ("Speed limit", juce::dontSendNotification);
        addAndMakeVisible (title);
        styleEditorLabel (speedLabel, 12.0f, false);
        speedLabel.setText ("Relative to global BPM", juce::dontSendNotification);
        addAndMakeVisible (speedLabel);
        speed.setRange (minDropSpeedMultiplier, maxDropSpeedMultiplier, 0.125);
        speed.setValue (value.bpmMultiplier, juce::dontSendNotification);
        speed.setSliderStyle (juce::Slider::LinearHorizontal);
        speed.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 26);
        speed.setTextValueSuffix ("x");
        speed.setColour (juce::Slider::thumbColourId, juce::Colour (0xffffb84d));
        speed.onValueChange = [this]
        {
            value.bpmMultiplier = speed.getValue();
            if (onChange) onChange (value);
        };
        addAndMakeVisible (speed);
        styleEditorLabel (chanceLabel, 12.0f, false);
        chanceLabel.setText ("Affect chance", juce::dontSendNotification);
        addAndMakeVisible (chanceLabel);
        chance.setRange (0.0, 100.0, 1.0);
        chance.setValue (value.affectProbability * 100.0, juce::dontSendNotification);
        chance.setSliderStyle (juce::Slider::LinearHorizontal);
        chance.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 26);
        chance.setTextValueSuffix ("%");
        chance.setColour (juce::Slider::thumbColourId, juce::Colour (0xffffb84d));
        chance.onValueChange = [this]
        {
            value.affectProbability = chance.getValue() / 100.0;
            if (onChange) onChange (value);
        };
        addAndMakeVisible (chance);
        setSize (310, 170);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (25));
        area.removeFromTop (8);
        speedLabel.setBounds (area.removeFromTop (18));
        speed.setBounds (area.removeFromTop (34));
        area.removeFromTop (7);
        chanceLabel.setBounds (area.removeFromTop (18));
        chance.setBounds (area.removeFromTop (34));
    }

private:
    PipeSpeedLimit value;
    std::function<void(const PipeSpeedLimit&)> onChange;
    juce::Label title, speedLabel, chanceLabel;
    juce::Slider speed, chance;
};

class WaitSettingsComponent final : public juce::Component
{
public:
    WaitSettingsComponent (PipeWait initial, std::function<void(const PipeWait&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText ("Wait", juce::dontSendNotification);
        addAndMakeVisible (title);
        styleEditorLabel (beatsLabel, 12.0f, false);
        beatsLabel.setText ("Hold passing drops", juce::dontSendNotification);
        addAndMakeVisible (beatsLabel);
        beats.setRange (0.25, 32.0, 0.25);
        beats.setValue (value.beats, juce::dontSendNotification);
        beats.setSliderStyle (juce::Slider::LinearHorizontal);
        beats.setTextBoxStyle (juce::Slider::TextBoxRight, false, 76, 26);
        beats.setTextValueSuffix (" beats");
        beats.setColour (juce::Slider::thumbColourId, juce::Colour (0xffffd166));
        beats.onValueChange = [this]
        {
            value.beats = beats.getValue();
            if (onChange) onChange (value);
        };
        addAndMakeVisible (beats);
        setSize (310, 112);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (25));
        area.removeFromTop (8);
        beatsLabel.setBounds (area.removeFromTop (18));
        beats.setBounds (area.removeFromTop (34));
    }

private:
    PipeWait value;
    std::function<void(const PipeWait&)> onChange;
    juce::Label title, beatsLabel;
    juce::Slider beats;
};

class StrikeSettingsComponent final : public juce::Component
{
public:
    StrikeSettingsComponent (PipeStrike initial, std::function<void(const PipeStrike&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true); title.setText ("Strike", juce::dontSendNotification); addAndMakeVisible (title);
        styleEditorLabel (countLabel, 12.0f, false); countLabel.setText ("Maximum discs", juce::dontSendNotification); addAndMakeVisible (countLabel);
        count.setRange (1, 4, 1); count.setValue (value.maxDiscs, juce::dontSendNotification);
        count.setSliderStyle (juce::Slider::LinearHorizontal); count.setTextBoxStyle (juce::Slider::TextBoxRight, false, 54, 26);
        count.setColour (juce::Slider::thumbColourId, juce::Colour (0xffff6b8a)); addAndMakeVisible (count);
        configureToggle (left, "NW", value.left); configureToggle (right, "NE", value.right);
        configureToggle (up, "SW", value.up); configureToggle (down, "SE", value.down);
        count.onValueChange = [this] { value.maxDiscs = static_cast<int> (count.getValue()); notify(); };
        left.onClick = [this] { value.left = left.getToggleState(); notify(); };
        right.onClick = [this] { value.right = right.getToggleState(); notify(); };
        up.onClick = [this] { value.up = up.getToggleState(); notify(); };
        down.onClick = [this] { value.down = down.getToggleState(); notify(); };
        setSize (310, 160);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (27)); area.removeFromTop (5);
        auto countRow = area.removeFromTop (38); countLabel.setBounds (countRow.removeFromLeft (110)); count.setBounds (countRow);
        area.removeFromTop (7);
        auto directions = area.removeFromTop (30);
        const auto width = directions.getWidth() / 4;
        left.setBounds (directions.removeFromLeft (width)); right.setBounds (directions.removeFromLeft (width));
        up.setBounds (directions.removeFromLeft (width)); down.setBounds (directions);
    }

private:
    PipeStrike value;
    std::function<void(const PipeStrike&)> onChange;
    juce::Label title, countLabel;
    juce::Slider count;
    juce::ToggleButton left, right, up, down;
    void configureToggle (juce::ToggleButton& button, const juce::String& text, bool enabled)
    {
        button.setButtonText (text); button.setToggleState (enabled, juce::dontSendNotification);
        button.setColour (juce::ToggleButton::textColourId, textPrimary()); addAndMakeVisible (button);
    }
    void notify() { if (onChange) onChange (value); }
};

class TeleportSettingsComponent final : public juce::Component
{
public:
    TeleportSettingsComponent (PipeTeleport initial, juce::StringArray destinationNames,
                               juce::StringArray destinationIds,
                               std::function<void(const PipeTeleport&)> changed)
        : value (std::move (initial)), ids (std::move (destinationIds)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true); title.setText ("Teleport", juce::dontSendNotification); addAndMakeVisible (title);
        styleEditorLabel (destinationLabel, 12.0f, false); destinationLabel.setText ("Destination", juce::dontSendNotification); addAndMakeVisible (destinationLabel);
        destination.addItem ("Any (random)", 1); destination.addItemList (destinationNames, 2);
        auto selected = 1; for (int i = 0; i < ids.size(); ++i) if (ids[i] == value.destinationId) selected = i + 2;
        destination.setSelectedId (selected, juce::dontSendNotification); addAndMakeVisible (destination);
        configureSlider (chanceLabel, chance, "Teleport chance", 0, 100, 1, value.probability * 100.0, "%");
        configureSlider (limitLabel, limit, "Maximum teleports", 1, 64, 1, value.maxPerWindow, "");
        configureSlider (barsLabel, bars, "Per window", 0.25, 16, 0.25, value.windowBars, " bars");
        configureSlider (stopLabel, stop, "Stop after", 0, 1000, 1, value.stopAfter, " drops");
        destination.onChange = [this]
        {
            const auto index = destination.getSelectedId() - 2;
            value.destinationId = juce::isPositiveAndBelow (index, ids.size()) ? ids[index] : juce::String(); notify();
        };
        chance.onValueChange = [this] { value.probability = chance.getValue() / 100.0; notify(); };
        limit.onValueChange = [this] { value.maxPerWindow = static_cast<int> (limit.getValue()); notify(); };
        bars.onValueChange = [this] { value.windowBars = bars.getValue(); notify(); };
        stop.onValueChange = [this] { value.stopAfter = static_cast<int> (stop.getValue()); notify(); };
        setSize (360, 270);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11); title.setBounds (area.removeFromTop (27)); area.removeFromTop (5);
        auto destinationRow = area.removeFromTop (40); destinationLabel.setBounds (destinationRow.removeFromLeft (124)); destination.setBounds (destinationRow.reduced (0, 4));
        layoutRow (area, chanceLabel, chance); layoutRow (area, limitLabel, limit);
        layoutRow (area, barsLabel, bars); layoutRow (area, stopLabel, stop);
    }

private:
    PipeTeleport value; juce::StringArray ids; std::function<void(const PipeTeleport&)> onChange;
    juce::Label title, destinationLabel, chanceLabel, limitLabel, barsLabel, stopLabel;
    juce::ComboBox destination; juce::Slider chance, limit, bars, stop;
    void configureSlider (juce::Label& label, juce::Slider& slider, const juce::String& text,
                          double min, double max, double step, double current, const juce::String& suffix)
    {
        styleEditorLabel (label, 12.0f, false); label.setText (text, juce::dontSendNotification); addAndMakeVisible (label);
        slider.setRange (min, max, step); slider.setValue (current, juce::dontSendNotification);
        slider.setSliderStyle (juce::Slider::LinearHorizontal); slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 82, 26);
        slider.setTextValueSuffix (suffix); slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff53d8fb)); addAndMakeVisible (slider);
    }
    static void layoutRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
    { auto row = area.removeFromTop (42); label.setBounds (row.removeFromLeft (124)); slider.setBounds (row); }
    void notify() { if (onChange) onChange (value); }
};

class FilterSettingsComponent final : public juce::Component
{
public:
    FilterSettingsComponent (PipeFilter initial, std::function<void(const PipeFilter&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true); title.setText ("Speed filter", juce::dontSendNotification); addAndMakeVisible (title);
        styleEditorLabel (modeLabel, 12.0f, false); modeLabel.setText ("Mode", juce::dontSendNotification); addAndMakeVisible (modeLabel);
        mode.addItemList ({ "Highpass", "Lowpass", "Bandpass" }, 1); mode.setSelectedId (static_cast<int> (value.mode) + 1, juce::dontSendNotification); addAndMakeVisible (mode);
        configureSlider (lowLabel, low, "Low cutoff", value.lowSpeed);
        configureSlider (highLabel, high, "High cutoff", value.highSpeed);
        mode.onChange = [this] { value.mode = static_cast<PipeFilter::Mode> (juce::jlimit (0, 2, mode.getSelectedId() - 1)); updateState(); notify(); };
        low.onValueChange = [this] { value.lowSpeed = low.getValue(); if (high.getValue() < low.getValue()) high.setValue (low.getValue(), juce::sendNotificationSync); notify(); };
        high.onValueChange = [this] { value.highSpeed = high.getValue(); if (low.getValue() > high.getValue()) low.setValue (high.getValue(), juce::sendNotificationSync); notify(); };
        updateState(); setSize (330, 184);
    }
    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11); title.setBounds (area.removeFromTop (27)); area.removeFromTop (5);
        auto row = area.removeFromTop (38); modeLabel.setBounds (row.removeFromLeft (105)); mode.setBounds (row.reduced (0, 3));
        layoutRow (area, lowLabel, low); layoutRow (area, highLabel, high);
    }
private:
    PipeFilter value; std::function<void(const PipeFilter&)> onChange;
    juce::Label title, modeLabel, lowLabel, highLabel; juce::ComboBox mode; juce::Slider low, high;
    void configureSlider (juce::Label& label, juce::Slider& slider, const juce::String& text, double current)
    {
        styleEditorLabel (label, 12.0f, false); label.setText (text, juce::dontSendNotification); addAndMakeVisible (label);
        slider.setRange (minDropSpeedMultiplier, maxDropSpeedMultiplier, 0.125); slider.setValue (current, juce::dontSendNotification);
        slider.setSliderStyle (juce::Slider::LinearHorizontal); slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 26);
        slider.setTextValueSuffix ("x"); slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff7dd3fc)); addAndMakeVisible (slider);
    }
    static void layoutRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
    { auto row = area.removeFromTop (42); label.setBounds (row.removeFromLeft (105)); slider.setBounds (row); }
    void updateState()
    {
        low.setEnabled (value.mode != PipeFilter::Mode::lowpass); lowLabel.setEnabled (low.isEnabled());
        high.setEnabled (value.mode != PipeFilter::Mode::highpass); highLabel.setEnabled (high.isEnabled());
    }
    void notify() { if (onChange) onChange (value); }
};

class LogicSettingsComponent final : public juce::Component, private juce::Timer
{
public:
    LogicSettingsComponent (PipeLogic initial, std::function<void(const PipeLogic&)> changed,
                            std::function<PipeLogic()> liveReader = {}, std::function<juce::String()> connectionReader = {})
        : value (std::move (initial)), onChange (std::move (changed)), readLive (std::move (liveReader)),
          readConnections (std::move (connectionReader))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText ("Logic", juce::dontSendNotification);
        addAndMakeVisible (title);

        styleEditorLabel (modeLabel, 12.0f, false);
        modeLabel.setText ("Rule", juce::dontSendNotification);
        addAndMakeVisible (modeLabel);
        mode.addItemList ({ "Gate", "Count to open", "Switch", "Speed comparison", "Flip-flop", "Every Nth drop",
                            "AND", "OR", "XOR" }, 1);
        mode.setSelectedId (static_cast<int> (value.mode) + 1, juce::dontSendNotification);
        addAndMakeVisible (mode);

        styleEditorLabel (orientationLabel, 12.0f, false);
        orientationLabel.setText ("Faces", juce::dontSendNotification);
        addAndMakeVisible (orientationLabel);
        orientation.addItemList ({ "Right", "Down", "Left", "Up" }, 1);
        orientation.setSelectedId (static_cast<int> (value.orientation) + 1, juce::dontSendNotification);
        addAndMakeVisible (orientation);

        styleEditorLabel (coincidenceLabel, 12.0f, false);
        coincidenceLabel.setText ("Input window", juce::dontSendNotification);
        addAndMakeVisible (coincidenceLabel);
        coincidence.setRange (0.0625, 4.0, 0.0625);
        coincidence.setValue (value.coincidenceBeats, juce::dontSendNotification);
        coincidence.setSliderStyle (juce::Slider::LinearHorizontal);
        coincidence.setTextBoxStyle (juce::Slider::TextBoxRight, false, 78, 26);
        coincidence.setTextValueSuffix (" beats");
        coincidence.setColour (juce::Slider::thumbColourId, juce::Colour (0xff39f58a));
        addAndMakeVisible (coincidence);

        styleEditorLabel (signalModeLabel, 12.0f, false);
        signalModeLabel.setText ("Input mode", juce::dontSendNotification);
        addAndMakeVisible (signalModeLabel);
        signalMode.addItemList ({ "Edge", "Level" }, 1);
        signalMode.setSelectedId (static_cast<int> (value.signalMode) + 1, juce::dontSendNotification);
        addAndMakeVisible (signalMode);

        styleEditorLabel (timeoutLabel, 12.0f, false);
        timeoutLabel.setText ("On timeout", juce::dontSendNotification);
        addAndMakeVisible (timeoutLabel);
        timeout.addItemList ({ "Discard", "Release", "Reverse", "Reroute" }, 1);
        timeout.setSelectedId (static_cast<int> (value.timeoutAction) + 1, juce::dontSendNotification);
        addAndMakeVisible (timeout);

        gateOpen.setButtonText ("Gate open");
        gateOpen.setToggleState (value.gateOpen, juce::dontSendNotification);
        gateOpen.setColour (juce::ToggleButton::textColourId, textPrimary());
        addAndMakeVisible (gateOpen);

        styleEditorLabel (targetLabel, 12.0f, false);
        addAndMakeVisible (targetLabel);
        target.setRange (1, 64, 1);
        target.setValue (value.targetCount, juce::dontSendNotification);
        target.setSliderStyle (juce::Slider::LinearHorizontal);
        target.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 26);
        target.setColour (juce::Slider::thumbColourId, juce::Colour (0xffffd166));
        addAndMakeVisible (target);

        styleEditorLabel (branchLabel, 12.0f, false);
        branchLabel.setText ("Send towards", juce::dontSendNotification);
        addAndMakeVisible (branchLabel);
        branch.addItemList ({ "Left", "Straight", "Right", "Random" }, 1);
        branch.setSelectedId (static_cast<int> (value.branch) + 1, juce::dontSendNotification);
        addAndMakeVisible (branch);

        styleEditorLabel (comparisonLabel, 12.0f, false);
        comparisonLabel.setText ("Comparison", juce::dontSendNotification);
        addAndMakeVisible (comparisonLabel);
        comparison.addItemList ({ "Less than", "Less or equal", "Equal", "Greater or equal", "Greater than" }, 1);
        comparison.setSelectedId (static_cast<int> (value.comparison) + 1, juce::dontSendNotification);
        addAndMakeVisible (comparison);

        styleEditorLabel (speedLabel, 12.0f, false);
        speedLabel.setText ("Drop speed", juce::dontSendNotification);
        addAndMakeVisible (speedLabel);
        speed.setRange (minDropSpeedMultiplier, maxDropSpeedMultiplier, 0.125);
        speed.setValue (value.compareSpeed, juce::dontSendNotification);
        speed.setSliderStyle (juce::Slider::LinearHorizontal);
        speed.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 26);
        speed.setTextValueSuffix ("x");
        speed.setColour (juce::Slider::thumbColourId, juce::Colour (0xffffd166));
        addAndMakeVisible (speed);

        styleEditorLabel (explanation, 11.0f, false);
        explanation.setColour (juce::Label::textColourId, textMuted());
        explanation.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (explanation);

        styleEditorLabel (diagnostics, 11.0f, false);
        diagnostics.setColour (juce::Label::textColourId, textMuted());
        diagnostics.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (diagnostics);

        mode.onChange = [this]
        {
            value.mode = static_cast<PipeLogic::Mode> (juce::jlimit (0, 8, mode.getSelectedId() - 1));
            updateState();
            notify();
        };
        orientation.onChange = [this] { value.orientation = static_cast<PipeLogic::Orientation> (juce::jlimit (0, 3, orientation.getSelectedId() - 1)); notify(); };
        coincidence.onValueChange = [this]
        {
            if (value.signalMode == PipeLogic::SignalMode::level) value.levelHoldBeats = coincidence.getValue();
            else value.coincidenceBeats = coincidence.getValue();
            notify();
        };
        signalMode.onChange = [this]
        {
            value.signalMode = static_cast<PipeLogic::SignalMode> (juce::jlimit (0, 1, signalMode.getSelectedId() - 1));
            coincidenceLabel.setText (value.signalMode == PipeLogic::SignalMode::edge ? "Input window" : "Level hold", juce::dontSendNotification);
            coincidence.setValue (value.signalMode == PipeLogic::SignalMode::edge ? value.coincidenceBeats : value.levelHoldBeats, juce::dontSendNotification);
            notify();
        };
        timeout.onChange = [this] { value.timeoutAction = static_cast<PipeLogic::TimeoutAction> (juce::jlimit (0, 3, timeout.getSelectedId() - 1)); notify(); };
        coincidence.onDragEnd = [this]
        {
            if (value.signalMode == PipeLogic::SignalMode::level) value.levelHoldBeats = coincidence.getValue();
            else value.coincidenceBeats = coincidence.getValue();
            notify();
        };
        gateOpen.onClick = [this] { value.gateOpen = gateOpen.getToggleState(); notify(); };
        target.onValueChange = [this] { value.targetCount = static_cast<int> (target.getValue()); notify(); };
        branch.onChange = [this] { value.branch = static_cast<PipeLogic::Branch> (juce::jlimit (0, 3, branch.getSelectedId() - 1)); notify(); };
        comparison.onChange = [this] { value.comparison = static_cast<PipeLogic::Comparison> (juce::jlimit (0, 4, comparison.getSelectedId() - 1)); notify(); };
        speed.onValueChange = [this] { value.compareSpeed = speed.getValue(); notify(); };

        setSize (350, 170);
        updateState();
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (27));
        area.removeFromTop (5);
        auto modeRow = area.removeFromTop (38);
        modeLabel.setBounds (modeRow.removeFromLeft (105));
        mode.setBounds (modeRow.reduced (0, 3));
        area.removeFromTop (5);

        if (orientation.isVisible())
        {
            auto row = area.removeFromTop (38);
            orientationLabel.setBounds (row.removeFromLeft (105));
            orientation.setBounds (row.reduced (0, 3));
        }
        if (coincidence.isVisible())
        {
            auto signalModeRow = area.removeFromTop (38);
            signalModeLabel.setBounds (signalModeRow.removeFromLeft (105));
            signalMode.setBounds (signalModeRow.reduced (0, 3));
            auto row = area.removeFromTop (38);
            coincidenceLabel.setBounds (row.removeFromLeft (105));
            coincidence.setBounds (row);
            row = area.removeFromTop (38);
            timeoutLabel.setBounds (row.removeFromLeft (105));
            timeout.setBounds (row.reduced (0, 3));
        }

        if (gateOpen.isVisible()) gateOpen.setBounds (area.removeFromTop (34));
        if (target.isVisible())
        {
            auto row = area.removeFromTop (38);
            targetLabel.setBounds (row.removeFromLeft (105));
            target.setBounds (row);
        }
        if (branch.isVisible())
        {
            auto row = area.removeFromTop (38);
            branchLabel.setBounds (row.removeFromLeft (105));
            branch.setBounds (row.reduced (0, 3));
        }
        if (comparison.isVisible())
        {
            auto row = area.removeFromTop (38);
            comparisonLabel.setBounds (row.removeFromLeft (105));
            comparison.setBounds (row.reduced (0, 3));
            row = area.removeFromTop (38);
            speedLabel.setBounds (row.removeFromLeft (105));
            speed.setBounds (row);
        }
        area.removeFromTop (3);
        explanation.setBounds (area.removeFromTop (38));
        diagnostics.setBounds (area.removeFromTop (64));
    }

private:
    PipeLogic value;
    std::function<void(const PipeLogic&)> onChange;
    std::function<PipeLogic()> readLive;
    std::function<juce::String()> readConnections;
    juce::Label title, modeLabel, orientationLabel, coincidenceLabel, signalModeLabel, timeoutLabel, targetLabel, branchLabel, comparisonLabel, speedLabel, explanation, diagnostics;
    juce::ComboBox mode, orientation, signalMode, timeout, branch, comparison;
    juce::ToggleButton gateOpen;
    juce::Slider coincidence, target, speed;

    void updateState()
    {
        const auto isGate = value.mode == PipeLogic::Mode::gate;
        const auto isCount = value.mode == PipeLogic::Mode::counter || value.mode == PipeLogic::Mode::everyNth;
        const auto isSwitch = value.mode == PipeLogic::Mode::switcher;
        const auto isComparator = value.mode == PipeLogic::Mode::comparator;
        const auto isBinaryGate = value.mode == PipeLogic::Mode::andGate
                               || value.mode == PipeLogic::Mode::orGate
                               || value.mode == PipeLogic::Mode::xorGate;
        const auto isAnyGate = isGate || isBinaryGate;

        orientationLabel.setVisible (isAnyGate); orientation.setVisible (isAnyGate);
        coincidenceLabel.setVisible (isBinaryGate); coincidence.setVisible (isBinaryGate);
        signalModeLabel.setVisible (isBinaryGate); signalMode.setVisible (isBinaryGate);
        timeoutLabel.setVisible (value.mode == PipeLogic::Mode::andGate); timeout.setVisible (value.mode == PipeLogic::Mode::andGate);
        diagnostics.setVisible (isBinaryGate);
        gateOpen.setVisible (isGate);
        targetLabel.setVisible (isCount); target.setVisible (isCount);
        branchLabel.setVisible (isSwitch); branch.setVisible (isSwitch);
        comparisonLabel.setVisible (isComparator); comparison.setVisible (isComparator);
        speedLabel.setVisible (isComparator); speed.setVisible (isComparator);
        targetLabel.setText (value.mode == PipeLogic::Mode::counter ? "Open after" : "Pass every", juce::dontSendNotification);

        if (isGate) explanation.setText ("Closed gates remove passing drops.", juce::dontSendNotification);
        else if (value.mode == PipeLogic::Mode::counter) explanation.setText ("Blocks drops until the target count, then remains open.", juce::dontSendNotification);
        else if (isSwitch) explanation.setText ("Chooses an exit at a pipe junction.", juce::dontSendNotification);
        else if (isComparator) explanation.setText ("Only drops matching the speed comparison pass.", juce::dontSendNotification);
        else if (value.mode == PipeLogic::Mode::flipFlop) explanation.setText ("Alternates junction exits, or pass and block on a straight pipe.", juce::dontSendNotification);
        else if (value.mode == PipeLogic::Mode::andGate) explanation.setText ("Passes when drops arrive at both inputs together.", juce::dontSendNotification);
        else if (value.mode == PipeLogic::Mode::orGate) explanation.setText ("Passes a drop arriving at either input.", juce::dontSendNotification);
        else if (value.mode == PipeLogic::Mode::xorGate) explanation.setText ("Passes when only one input is active.", juce::dontSendNotification);
        else explanation.setText ("Only each selected numbered drop passes.", juce::dontSendNotification);

        coincidenceLabel.setText (value.signalMode == PipeLogic::SignalMode::edge ? "Input window" : "Level hold", juce::dontSendNotification);
        coincidence.setValue (value.signalMode == PipeLogic::SignalMode::edge ? value.coincidenceBeats : value.levelHoldBeats, juce::dontSendNotification);
        setSize (350, isComparator ? 220 : (isBinaryGate ? (value.mode == PipeLogic::Mode::andGate ? 425 : 385) : (isGate ? 220 : 180)));
        resized();
        repaint();
    }

    void timerCallback() override
    {
        if (! readLive) return;
        const auto live = readLive();
        const auto now = juce::Time::getMillisecondCounterHiRes();
        const auto a = live.inputAFlashUntilMs > now;
        const auto b = live.inputBFlashUntilMs > now;
        const auto out = live.outputFlashUntilMs > now;
        juce::String truth;
        if (live.mode == PipeLogic::Mode::andGate) truth = "00→0   01→0   10→0   11→1";
        else if (live.mode == PipeLogic::Mode::orGate) truth = "00→0   01→1   10→1   11→1";
        else if (live.mode == PipeLogic::Mode::xorGate) truth = "00→0   01→1   10→1   11→0";
        auto connections = readConnections ? readConnections() : juce::String();
        diagnostics.setText ("A " + juce::String (a ? "ON" : "off") + "   B " + (b ? "ON" : "off")
                             + "   OUT " + (out ? "ON" : "off") + "\n" + truth + "\n"
                             + connections + (connections.isNotEmpty() ? "  ·  " : "") + live.lastEvent,
                             juce::dontSendNotification);
    }

    void notify() { if (onChange) onChange (value); }
};

} // namespace blendings
