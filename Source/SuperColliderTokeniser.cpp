#include "SuperColliderTokeniser.h"

#include "AppTheme.h"

namespace blendings
{
int SuperColliderTokeniser::readNextToken (juce::CodeDocument::Iterator& source)
{
    source.skipWhitespace();
    const auto c = source.peekNextChar();
    if (c == 0)
        return error;

    auto peekAhead = [&source] (int distance)
    {
        auto copy = source;
        for (int i = 0; i < distance; ++i)
            copy.skip();
        return copy.peekNextChar();
    };

    if (c == '/' && peekAhead (1) == '/')
    {
        source.skipToEndOfLine();
        return comment;
    }
    if (c == '/' && peekAhead (1) == '*')
    {
        source.skip();
        source.skip();
        while (source.peekNextChar() != 0)
        {
            if (source.peekNextChar() == '*' && peekAhead (1) == '/')
            {
                source.skip();
                source.skip();
                break;
            }
            source.skip();
        }
        return comment;
    }
    if (c == '"' || c == '\'')
    {
        const auto quote = c;
        source.skip();
        while (source.peekNextChar() != 0)
        {
            const auto next = source.nextChar();
            if (next == '\\' && source.peekNextChar() != 0)
            {
                source.skip();
                continue;
            }
            if (next == quote)
                break;
        }
        return string;
    }
    if (c == '\\')
    {
        source.skip();
        while (isIdentifierBody (source.peekNextChar()))
            source.skip();
        return symbol;
    }
    if (juce::CharacterFunctions::isDigit (c)
        || (c == '.' && juce::CharacterFunctions::isDigit (peekAhead (1))))
    {
        bool seenDot = false;
        bool seenExponent = false;
        while (true)
        {
            const auto next = source.peekNextChar();
            if (juce::CharacterFunctions::isDigit (next))
            {
                source.skip();
                continue;
            }
            if (next == '.' && ! seenDot && ! seenExponent)
            {
                seenDot = true;
                source.skip();
                continue;
            }
            if ((next == 'e' || next == 'E') && ! seenExponent)
            {
                seenExponent = true;
                source.skip();
                if (source.peekNextChar() == '+' || source.peekNextChar() == '-')
                    source.skip();
                continue;
            }
            break;
        }
        return number;
    }
    if (isIdentifierStart (c))
    {
        juce::String token;
        while (isIdentifierBody (source.peekNextChar()))
            token << juce::String::charToString (source.nextChar());
        if (isKeyword (token))
            return keyword;
        if (token.isNotEmpty() && juce::CharacterFunctions::isUpperCase (token[0]))
            return ugen;
        return identifier;
    }

    source.skip();
    if (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}')
        return bracket;
    if (c == ',' || c == ';' || c == ':')
        return punctuation;
    if (juce::String ("+-*/%=<>!&|?@~").containsChar (c))
        return operatorToken;
    return punctuation;
}

juce::CodeEditorComponent::ColourScheme SuperColliderTokeniser::getDefaultColourScheme()
{
    juce::CodeEditorComponent::ColourScheme scheme;
    scheme.set ("Error", juce::Colour (0xffff5a68));
    scheme.set ("Comment", juce::Colour (0xff607069));
    scheme.set ("Keyword", juce::Colour (0xffffd76a));
    scheme.set ("UGen/Class", juce::Colour (0xff34c7ff));
    scheme.set ("Identifier", ui::textPrimary());
    scheme.set ("Number", juce::Colour (0xffff9a62));
    scheme.set ("String", juce::Colour (0xffb9ff5d));
    scheme.set ("Symbol", juce::Colour (0xffff5fb7));
    scheme.set ("Bracket", juce::Colour (0xff9b7cff));
    scheme.set ("Operator", juce::Colour (0xff93e0d1));
    scheme.set ("Punctuation", ui::textMuted());
    return scheme;
}

bool SuperColliderTokeniser::isIdentifierStart (juce::juce_wchar c) noexcept
{
    return c == '_' || juce::CharacterFunctions::isLetter (c);
}

bool SuperColliderTokeniser::isIdentifierBody (juce::juce_wchar c) noexcept
{
    return c == '_' || juce::CharacterFunctions::isLetterOrDigit (c);
}

bool SuperColliderTokeniser::isKeyword (const juce::String& token)
{
    static const char* const keywords[] {
        "arg", "var", "const", "class", "super", "this", "true", "false", "nil", "inf",
        "if", "while", "for", "do", "switch", "case", "return", "break", "continue",
        "doneAction", "out", "amp", "pan", "pitch", "tempo", "sustain"
    };
    for (const auto* keyword : keywords)
        if (token == keyword)
            return true;
    return false;
}
}
