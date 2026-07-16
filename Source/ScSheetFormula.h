#pragma once

#include <juce_core/juce_core.h>
#include <unordered_map>

class ScoreFormula
{
public:
    using Variables = std::unordered_map<std::string, double>;

    static juce::Result validate(const juce::String& expression);
    static double evaluate(const juce::String& expression, const Variables& variables, double fallback, juce::String* error = nullptr);
};
