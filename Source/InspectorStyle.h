#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace BlendingsInspector
{
constexpr float titleSize = 18.0f;
constexpr float sectionSize = 10.5f;
constexpr float labelSize = 11.5f;
constexpr int fieldHeight = 28;
constexpr int rowGap = 8;
constexpr int textBoxWidth = 64;

inline juce::Colour surface() { return juce::Colour (0xff17201c); }
inline juce::Colour stroke() { return juce::Colour (0xff32423b); }
inline juce::Colour primaryText() { return juce::Colour (0xffedf4ee); }
inline juce::Colour secondaryText() { return juce::Colour (0xff87958e); }
inline juce::Colour accent() { return juce::Colour (0xff0a84ff); }

inline void styleTitle (juce::Label& label)
{
    label.setFont (juce::FontOptions (titleSize).withStyle ("Bold"));
    label.setColour (juce::Label::textColourId, primaryText());
    label.setJustificationType (juce::Justification::centredLeft);
}

inline void styleSection (juce::Label& label)
{
    label.setFont (juce::FontOptions (sectionSize).withStyle ("Bold"));
    label.setColour (juce::Label::textColourId, primaryText().withAlpha (0.82f));
    label.setJustificationType (juce::Justification::centredLeft);
}

inline void styleLabel (juce::Label& label)
{
    label.setFont (juce::FontOptions (labelSize));
    label.setColour (juce::Label::textColourId, secondaryText());
    label.setJustificationType (juce::Justification::centredLeft);
}

inline void styleButton (juce::TextButton& button)
{
    button.setColour (juce::TextButton::buttonColourId, surface());
    button.setColour (juce::TextButton::buttonOnColourId, accent());
    button.setColour (juce::TextButton::textColourOffId, primaryText());
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

inline void styleComboBox (juce::ComboBox& box)
{
    box.setColour (juce::ComboBox::backgroundColourId, surface());
    box.setColour (juce::ComboBox::textColourId, primaryText());
    box.setColour (juce::ComboBox::outlineColourId, stroke());
    box.setColour (juce::ComboBox::arrowColourId, secondaryText());
}

inline void styleSlider (juce::Slider& slider, juce::Colour track = juce::Colour (0xff45d7c0))
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, textBoxWidth, fieldHeight);
    slider.setColour (juce::Slider::trackColourId, track);
    slider.setColour (juce::Slider::thumbColourId, accent());
    slider.setColour (juce::Slider::textBoxTextColourId, primaryText());
    slider.setColour (juce::Slider::textBoxBackgroundColourId, surface());
    slider.setColour (juce::Slider::textBoxOutlineColourId, stroke());
}
}
