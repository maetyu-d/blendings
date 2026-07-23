#include "AppTheme.h"

#include <array>
#include <cmath>

namespace blendings::ui
{
namespace
{
bool rainbowUiEnabled = false;

float linearColourChannel (uint8_t value)
{
    const auto channel = static_cast<float> (value) / 255.0f;
    return channel <= 0.04045f ? channel / 12.92f
                              : std::pow ((channel + 0.055f) / 1.055f, 2.4f);
}

float relativeLuminance (juce::Colour colour)
{
    return 0.2126f * linearColourChannel (colour.getRed())
         + 0.7152f * linearColourChannel (colour.getGreen())
         + 0.0722f * linearColourChannel (colour.getBlue());
}

float contrastRatio (juce::Colour first, juce::Colour second)
{
    const auto lighter = juce::jmax (relativeLuminance (first), relativeLuminance (second));
    const auto darker = juce::jmin (relativeLuminance (first), relativeLuminance (second));
    return (lighter + 0.05f) / (darker + 0.05f);
}
}

void setRainbowUiEnabled (bool enabled) { rainbowUiEnabled = enabled; }
bool isRainbowUiEnabled() noexcept { return rainbowUiEnabled; }

juce::Colour appBackground()     { return rainbowUiEnabled ? juce::Colour (0xfff5f7fb) : juce::Colour (0xff0d100f); }
juce::Colour surfaceColour()     { return rainbowUiEnabled ? juce::Colour (0xffffffff) : juce::Colour (0xff161a18); }
juce::Colour raisedSurface()     { return rainbowUiEnabled ? juce::Colour (0xffe9edf5) : juce::Colour (0xff232725); }
juce::Colour subtleStroke()      { return rainbowUiEnabled ? juce::Colour (0xff9ba6bc) : juce::Colour (0xff363b38); }
juce::Colour textPrimary()       { return rainbowUiEnabled ? juce::Colour (0xff101522) : juce::Colour (0xfff2f2f2); }
juce::Colour textMuted()         { return rainbowUiEnabled ? juce::Colour (0xff566077) : juce::Colour (0xff989d9a); }
juce::Colour backgroundTop()     { return rainbowUiEnabled ? juce::Colour (0xffffffff) : juce::Colour (0xff101a16); }
juce::Colour backgroundBottom()  { return rainbowUiEnabled ? juce::Colour (0xffedf1f7) : juce::Colour (0xff0d1411); }
juce::Colour gridMajorColour()   { return rainbowUiEnabled ? juce::Colour (0x425200ff) : juce::Colour (0x204c7a6f); }
juce::Colour gridMinorColour()   { return rainbowUiEnabled ? juce::Colour (0x1f006cff) : juce::Colour (0x103f6a60); }
juce::Colour lineColour()        { return rainbowUiEnabled ? juce::Colour (0xff00d6b2) : juce::Colour (0xff65f0d0); }
juce::Colour lineDeepColour()    { return rainbowUiEnabled ? juce::Colour (0xff1746ff) : juce::Colour (0xff176175); }
juce::Colour lineLightColour()   { return rainbowUiEnabled ? juce::Colour (0xffffe000) : juce::Colour (0xfffff0a6); }
juce::Colour discColour()        { return rainbowUiEnabled ? juce::Colour (0xffff3d00) : juce::Colour (0xffffa45f); }
juce::Colour discHotColour()     { return rainbowUiEnabled ? juce::Colour (0xffffd600) : juce::Colour (0xffffd66e); }
juce::Colour discCoolColour()    { return rainbowUiEnabled ? juce::Colour (0xff9c00ff) : juce::Colour (0xffc45cff); }
juce::Colour discRimColour()     { return rainbowUiEnabled ? juce::Colour (0xff00c8a0) : juce::Colour (0xff70f5d5); }
juce::Colour soundElementColour(){ return rainbowUiEnabled ? juce::Colour (0xffffc400) : juce::Colour (0xfff2d279); }
juce::Colour worldElementColour(){ return rainbowUiEnabled ? juce::Colour (0xff009dff) : juce::Colour (0xff34c7ff); }
juce::Colour scCodeElementColour(){ return rainbowUiEnabled ? juce::Colour (0xff63d900) : juce::Colour (0xffb9ff5d); }
juce::Colour scSheetElementColour(){ return rainbowUiEnabled ? juce::Colour (0xffff008a) : juce::Colour (0xffff5fb7); }
juce::Colour orcaGridElementColour(){ return rainbowUiEnabled ? juce::Colour (0xff6c35ff) : juce::Colour (0xff9b7cff); }
juce::Colour pdPatchElementColour(){ return rainbowUiEnabled ? juce::Colour (0xffff4b18) : juce::Colour (0xffff744f); }
juce::Colour carouselElementColour(){ return rainbowUiEnabled ? juce::Colour (0xffff9d00) : juce::Colour (0xffffc857); }
juce::Colour pipeElementColour() { return rainbowUiEnabled ? juce::Colour (0xff00bf9a) : juce::Colour (0xff42e8c2); }
juce::Colour orbitsElementColour() { return rainbowUiEnabled ? juce::Colour (0xffff3d91) : juce::Colour (0xffe96ba8); }
juce::Colour accentColour()      { return rainbowUiEnabled ? juce::Colour (0xff1746ff) : juce::Colour (0xff0a84ff); }

juce::Colour contrastingForeground (juce::Colour background)
{
    const auto dark = juce::Colour (0xff101522);
    const auto light = juce::Colours::white;
    return contrastRatio (background, dark) >= contrastRatio (background, light) ? dark : light;
}

juce::Colour pipeColourForIndex (int index)
{
    static constexpr std::array<uint32_t, 7> darkColours {{
        0xffff7aa8, 0xffff9d72, 0xffffdd6d, 0xff8ef6a3,
        0xff58d8ff, 0xff9aa7ff, 0xffd58cff
    }};
    static constexpr std::array<uint32_t, 7> rainbowColours {{
        0xffff1744, 0xffff5a00, 0xffffd600, 0xff00c853,
        0xff00a8ff, 0xff3d3dff, 0xffa100ff
    }};
    const auto& colours = rainbowUiEnabled ? rainbowColours : darkColours;
    return juce::Colour (colours[static_cast<size_t> (std::abs (index) % static_cast<int> (colours.size()))]);
}

int pipePaletteIndex (juce::Colour colour)
{
    for (int i = 0; i < 7; ++i)
        if (pipeColourForIndex (i) == colour)
            return i;
    return 0;
}

void styleEditorLabel (juce::Label& label, float height, bool bold)
{
    label.setColour (juce::Label::textColourId, bold ? textPrimary() : textMuted());
    label.setJustificationType (juce::Justification::centredLeft);
    label.setFont (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

void styleEditorButton (juce::TextButton& button, const juce::String& text)
{
    button.setButtonText (text);
    button.setColour (juce::TextButton::buttonColourId, raisedSurface());
    button.setColour (juce::TextButton::buttonOnColourId, accentColour());
    button.setColour (juce::TextButton::textColourOffId, textPrimary());
    button.setColour (juce::TextButton::textColourOnId, contrastingForeground (accentColour()));
}

void styleScDurationBox (juce::TextEditor& editor)
{
    editor.setMultiLine (false);
    editor.setReturnKeyStartsNewLine (false);
    editor.setInputRestrictions (8, "0123456789.-");
    editor.setJustification (juce::Justification::centredLeft);
    editor.setFont (juce::FontOptions (13.0f));
    editor.setColour (juce::TextEditor::backgroundColourId, appBackground());
    editor.setColour (juce::TextEditor::textColourId, textPrimary());
    editor.setColour (juce::TextEditor::outlineColourId, subtleStroke());
    editor.setColour (juce::TextEditor::focusedOutlineColourId, scCodeElementColour());
    editor.setTextToShowWhenEmpty ("-", textMuted());
}

gridcollider::GridEditorComponent::Theme createOrcaTheme()
{
    gridcollider::GridEditorComponent::Theme theme;
    theme.background = appBackground();
    theme.viewportBackground = surfaceColour();
    theme.gridLine = subtleStroke();
    theme.text = textPrimary();
    theme.mutedText = textMuted();
    theme.rulerBackground = raisedSurface();
    theme.rulerText = textMuted();
    theme.cursor = orcaGridElementColour();
    theme.cursorText = juce::Colour (0xff070b18);
    theme.selection = orcaGridElementColour().withAlpha (0.20f);
    theme.selectionBorder = orcaGridElementColour();
    theme.playhead = orcaGridElementColour().withAlpha (0.12f);
    return theme;
}
}
