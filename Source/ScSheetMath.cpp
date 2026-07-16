#include "ScSheetMath.h"

namespace ScoreMath
{
    double wrapUnit(double value)
    {
        value = std::fmod(value, 1.0);
        return value < 0.0 ? value + 1.0 : value;
    }

    double deterministicNoise(int rowSeed, int step, int voiceIndex)
    {
        const auto value = std::sin((rowSeed + 1) * 12.9898 + (step + 1) * 78.233 + (voiceIndex + 1) * 37.719) * 43758.5453123;
        return (value - std::floor(value)) * 2.0 - 1.0;
    }

    bool shouldTriggerStep(const ScoreRow& row, int step, int steps)
    {
        if (row.density <= 0.0)
            return false;
        if (row.density >= 1.0)
            return true;

        const auto phase = wrapUnit(row.phase);
        const auto rotated = (step + static_cast<int>(std::round(phase * steps))) % juce::jmax(1, steps);
        const auto density = juce::jlimit(0.0, 1.0, row.density);

        if (row.pattern == PatternType::euclidean)
        {
            const auto hits = juce::jlimit(1, steps, static_cast<int>(std::round(density * steps)));
            return ((rotated * hits) % steps) < hits;
        }

        if (row.pattern == PatternType::sine)
        {
            const auto wave = 0.5 + 0.5 * std::sin(juce::MathConstants<double>::twoPi * (static_cast<double>(step) / steps + phase));
            return std::pow(wave, juce::jlimit(0.1, 16.0, row.curve)) >= 1.0 - density;
        }

        if (row.pattern == PatternType::golden)
        {
            constexpr auto phi = 0.6180339887498948;
            const auto value = std::fmod((rotated + 1) * phi + phase, 1.0);
            return value < density;
        }

        if (row.pattern == PatternType::logistic)
        {
            auto x = 0.21 + 0.58 * phase;
            for (int i = 0; i <= rotated; ++i)
                x = 3.88 * x * (1.0 - x);
            return x < density;
        }

        return std::abs(deterministicNoise(row.root, rotated, 0)) < density;
    }

    double contourPosition(const ScoreRow& row, int step, int steps)
    {
        const auto denominator = juce::jmax(1, steps - 1);
        auto x = wrapUnit(static_cast<double>(step) / denominator + row.phase);

        double shaped = x;
        if (row.contour == ContourType::fall) shaped = 1.0 - x;
        else if (row.contour == ContourType::arch) shaped = 1.0 - std::abs(2.0 * x - 1.0);
        else if (row.contour == ContourType::valley) shaped = std::abs(2.0 * x - 1.0);
        else if (row.contour == ContourType::zigzag) shaped = step % 2 == 0 ? x : 1.0 - x;
        else if (row.contour == ContourType::random) shaped = 0.5 + 0.5 * deterministicNoise(row.root + 17, step, 0);
        else if (row.contour == ContourType::flat) shaped = 0.5;

        return std::pow(juce::jlimit(0.0, 1.0, shaped), juce::jlimit(0.1, 16.0, row.curve));
    }

    double macroLevel(const ScoreRow& row, int step, int steps)
    {
        const auto denominator = juce::jmax(1, steps - 1);
        auto x = wrapUnit(static_cast<double>(step) / denominator + row.phase);
        const auto curve = juce::jlimit(0.1, 16.0, row.curve);

        if (row.macro == MacroStructureType::rampIn) return std::pow(x, curve);
        if (row.macro == MacroStructureType::rampOut) return std::pow(1.0 - x, curve);
        if (row.macro == MacroStructureType::arch) return std::pow(std::sin(juce::MathConstants<double>::pi * x), 1.0 / curve);
        if (row.macro == MacroStructureType::palindrome) return std::pow(1.0 - std::abs(2.0 * x - 1.0), 1.0 / curve);
        if (row.macro == MacroStructureType::islands)
        {
            constexpr auto phi = 0.6180339887498948;
            const auto islandIndex = static_cast<int>(std::floor(x * 13.0));
            const auto value = std::fmod((islandIndex + 1) * phi + row.phase, 1.0);
            return value < row.density ? 1.0 : 0.12;
        }
        return 1.0;
    }

    double phraseLevel(const ScoreRow& row, double localBeat)
    {
        const auto phraseBeats = juce::jmax(0.001, row.phraseBeats);
        auto x = wrapUnit(localBeat / phraseBeats + row.phase);
        const auto curve = juce::jlimit(0.1, 16.0, row.curve);

        if (row.phrase == PhraseStructureType::callResponse) return x < 0.5 ? 1.0 : 0.45;
        if (row.phrase == PhraseStructureType::waves) return 0.25 + 0.75 * std::pow(0.5 + 0.5 * std::sin(juce::MathConstants<double>::twoPi * x), 1.0 / curve);
        if (row.phrase == PhraseStructureType::staircase) return 0.35 + 0.65 * (std::floor(x * 4.0) / 3.0);
        if (row.phrase == PhraseStructureType::canon) return 0.7 + 0.3 * (static_cast<int>(std::floor(localBeat / phraseBeats)) % 2);
        if (row.phrase == PhraseStructureType::breathe) return std::pow(std::sin(juce::MathConstants<double>::pi * x), 1.0 / curve);
        return 1.0;
    }

    double accentLevel(const ScoreRow& row, int step)
    {
        const auto depth = juce::jlimit(0.0, 1.0, row.accentDepth);
        if (depth <= 0.0 || row.accent == AccentType::flat)
            return 1.0;

        const auto s16 = step % 16;
        bool hit = false;
        if (row.accent == AccentType::downbeat) hit = step % 4 == 0;
        else if (row.accent == AccentType::offbeat) hit = step % 2 == 1;
        else if (row.accent == AccentType::clave) hit = s16 == 0 || s16 == 3 || s16 == 6 || s16 == 10 || s16 == 12;
        else if (row.accent == AccentType::fibonacci) hit = s16 == 0 || s16 == 1 || s16 == 2 || s16 == 3 || s16 == 5 || s16 == 8 || s16 == 13;
        else if (row.accent == AccentType::prime) hit = s16 == 2 || s16 == 3 || s16 == 5 || s16 == 7 || s16 == 11 || s16 == 13;

        return hit ? 1.0 + 0.85 * depth : 1.0 - 0.55 * depth;
    }

    int phrasePitchOffset(const ScoreRow& row, double localBeat)
    {
        const auto phraseBeats = juce::jmax(0.001, row.phraseBeats);
        const auto phraseIndex = static_cast<int>(std::floor(localBeat / phraseBeats));
        const auto x = wrapUnit(localBeat / phraseBeats + row.phase);

        if (row.phrase == PhraseStructureType::callResponse) return x < 0.5 ? 0 : 7;
        if (row.phrase == PhraseStructureType::staircase) return static_cast<int>(std::floor(x * 4.0)) * 2;
        if (row.phrase == PhraseStructureType::canon)
        {
            static constexpr std::array<int, 4> offsets { 0, 5, 7, 12 };
            return offsets[static_cast<size_t>(phraseIndex % static_cast<int>(offsets.size()))];
        }
        return 0;
    }
}
