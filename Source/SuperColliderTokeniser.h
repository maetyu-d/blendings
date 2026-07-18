#pragma once

#include <JuceHeader.h>

namespace blendings
{
class SuperColliderTokeniser final : public juce::CodeTokeniser
{
public:
    enum TokenType
    {
        error = 0, comment, keyword, ugen, identifier, number, string,
        symbol, bracket, operatorToken, punctuation
    };

    int readNextToken (juce::CodeDocument::Iterator& source) override;
    juce::CodeEditorComponent::ColourScheme getDefaultColourScheme() override;

private:
    static bool isIdentifierStart (juce::juce_wchar c) noexcept;
    static bool isIdentifierBody (juce::juce_wchar c) noexcept;
    static bool isKeyword (const juce::String& token);
};
}
