#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

enum class VoiceType { tone, bass, noise, kick, sample, external };
enum class ScaleType { minorPentatonic, dorian, aeolian, chromatic, whole };
enum class ContourType { rise, fall, arch, valley, zigzag, random, flat };
enum class PatternType { probability, euclidean, sine, golden, logistic };
enum class MacroStructureType { continuous, rampIn, rampOut, arch, islands, palindrome };
enum class PhraseStructureType { continuous, callResponse, waves, staircase, canon, breathe };
enum class AccentType { flat, downbeat, offbeat, clave, fibonacci, prime };
enum class TransitionType { cut, linear, equalPower, probabilistic };

juce::String toString(VoiceType value);
juce::String toString(ScaleType value);
juce::String toString(ContourType value);
juce::String toString(PatternType value);
juce::String toString(MacroStructureType value);
juce::String toString(PhraseStructureType value);
juce::String toString(AccentType value);
juce::String toString(TransitionType value);
VoiceType voiceFromString(const juce::String& text);
ScaleType scaleFromString(const juce::String& text);
ContourType contourFromString(const juce::String& text);
PatternType patternFromString(const juce::String& text);
MacroStructureType macroStructureFromString(const juce::String& text);
PhraseStructureType phraseStructureFromString(const juce::String& text);
AccentType accentFromString(const juce::String& text);
TransitionType transitionFromString(const juce::String& text);
juce::StringArray voiceChoices();
juce::StringArray scaleChoices();
juce::StringArray contourChoices();
juce::StringArray patternChoices();
juce::StringArray macroStructureChoices();
juce::StringArray phraseStructureChoices();
juce::StringArray accentChoices();
juce::StringArray transitionChoices();

struct ScoreRow
{
    juce::String name = "Layer";
    juce::String group = "A";
    double startBar = 0.0;
    double bars = 4.0;
    VoiceType voice = VoiceType::tone;
    int root = 60;
    ScaleType scale = ScaleType::minorPentatonic;
    int midiLo = 48;
    int midiHi = 72;
    double stepBeats = 1.0;
    double noteBeats = 0.8;
    double density = 0.75;
    MacroStructureType macro = MacroStructureType::continuous;
    PhraseStructureType phrase = PhraseStructureType::continuous;
    double phraseBeats = 4.0;
    PatternType pattern = PatternType::probability;
    AccentType accent = AccentType::flat;
    double accentDepth = 0.0;
    double phase = 0.0;
    double curve = 1.0;
    juce::String rootExpr;
    juce::String densityExpr;
    juce::String ampExpr;
    juce::String transposeExpr;
    float amp = 0.25f;
    float pan = 0.0f;
    float cutoff = 0.7f;
    ContourType contour = ContourType::flat;
    float drift = 0.0f;
    int poly = 1;
    float spread = 0.0f;
    int transpose = 0;
    bool mute = false;
    bool solo = false;

    juce::ValueTree toValueTree() const;
    static ScoreRow fromValueTree(const juce::ValueTree& tree);
};

struct ScoreSection
{
    juce::String name = "Section";
    double startBar = 0.0;
    double bars = 16.0;
    juce::String activeGroups = "*";
    int transpose = 0;
    double densityScale = 1.0;
    double ampScale = 1.0;
    TransitionType transition = TransitionType::cut;
    double transitionBars = 0.0;
    double transitionProbability = 1.0;
    float mapX = 0.0f;
    float mapY = 0.0f;
    bool mapManual = false;

    juce::ValueTree toValueTree() const;
    static ScoreSection fromValueTree(const juce::ValueTree& tree);
    bool includesGroup(const juce::String& group) const;
};

class ScoreDocument
{
public:
    void addDefaultRow();
    void removeRow(int index);
    void duplicateRow(int index);
    void addDefaultSection();
    void removeSection(int index);
    void duplicateSection(int index);
    int getNumRows() const noexcept { return static_cast<int>(rows.size()); }
    int getNumSections() const noexcept { return static_cast<int>(sections.size()); }
    ScoreRow& getRow(int index) { return rows.at(static_cast<size_t>(index)); }
    const ScoreRow& getRow(int index) const { return rows.at(static_cast<size_t>(index)); }
    ScoreSection& getSection(int index) { return sections.at(static_cast<size_t>(index)); }
    const ScoreSection& getSection(int index) const { return sections.at(static_cast<size_t>(index)); }
    const std::vector<ScoreRow>& getRows() const noexcept { return rows; }
    std::vector<ScoreRow>& getRows() noexcept { return rows; }
    const std::vector<ScoreSection>& getSections() const noexcept { return sections; }
    std::vector<ScoreSection>& getSections() noexcept { return sections; }
    void setRows(std::vector<ScoreRow> newRows);
    void setSections(std::vector<ScoreSection> newSections);

    juce::ValueTree toValueTree() const;
    static ScoreDocument fromValueTree(const juce::ValueTree& tree);

private:
    std::vector<ScoreRow> rows;
    std::vector<ScoreSection> sections;
};
