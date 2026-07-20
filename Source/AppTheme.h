#pragma once

#include <JuceHeader.h>

#include "GridEditorComponent.h"

namespace blendings::ui
{
void setRainbowUiEnabled (bool enabled);
bool isRainbowUiEnabled() noexcept;

juce::Colour appBackground();
juce::Colour surfaceColour();
juce::Colour raisedSurface();
juce::Colour subtleStroke();
juce::Colour textPrimary();
juce::Colour textMuted();
juce::Colour backgroundTop();
juce::Colour backgroundBottom();
juce::Colour gridMajorColour();
juce::Colour gridMinorColour();
juce::Colour lineColour();
juce::Colour lineDeepColour();
juce::Colour lineLightColour();
juce::Colour discColour();
juce::Colour discHotColour();
juce::Colour discCoolColour();
juce::Colour discRimColour();
juce::Colour soundElementColour();
juce::Colour worldElementColour();
juce::Colour scCodeElementColour();
juce::Colour scSheetElementColour();
juce::Colour orcaGridElementColour();
juce::Colour pdPatchElementColour();
juce::Colour carouselElementColour();
juce::Colour pipeElementColour();
juce::Colour orbitsElementColour();
juce::Colour accentColour();

juce::Colour pipeColourForIndex (int index);
int pipePaletteIndex (juce::Colour colour);

void styleEditorLabel (juce::Label& label, float height, bool bold);
void styleEditorButton (juce::TextButton& button, const juce::String& text);
void styleScDurationBox (juce::TextEditor& editor);
gridcollider::GridEditorComponent::Theme createOrcaTheme();
}
