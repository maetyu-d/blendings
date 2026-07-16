#include "ScSheetScore.h"

#include <cmath>

namespace ids
{
    const juce::Identifier score { "ScoreDocument" };
    const juce::Identifier row { "ScoreRow" };
    const juce::Identifier section { "ScoreSection" };
}

juce::String toString(VoiceType value)
{
    switch (value)
    {
        case VoiceType::tone: return "tone";
        case VoiceType::bass: return "bass";
        case VoiceType::noise: return "noise";
        case VoiceType::kick: return "kick";
        case VoiceType::sample: return "sample";
        case VoiceType::external: return "external";
    }
    return "tone";
}

juce::String toString(ScaleType value)
{
    switch (value)
    {
        case ScaleType::minorPentatonic: return "minorPentatonic";
        case ScaleType::dorian: return "dorian";
        case ScaleType::aeolian: return "aeolian";
        case ScaleType::chromatic: return "chromatic";
        case ScaleType::whole: return "whole";
    }
    return "minorPentatonic";
}

juce::String toString(ContourType value)
{
    switch (value)
    {
        case ContourType::rise: return "rise";
        case ContourType::fall: return "fall";
        case ContourType::arch: return "arch";
        case ContourType::valley: return "valley";
        case ContourType::zigzag: return "zigzag";
        case ContourType::random: return "random";
        case ContourType::flat: return "flat";
    }
    return "flat";
}

juce::String toString(PatternType value)
{
    switch (value)
    {
        case PatternType::probability: return "probability";
        case PatternType::euclidean: return "euclidean";
        case PatternType::sine: return "sine";
        case PatternType::golden: return "golden";
        case PatternType::logistic: return "logistic";
    }
    return "probability";
}

juce::String toString(MacroStructureType value)
{
    switch (value)
    {
        case MacroStructureType::continuous: return "continuous";
        case MacroStructureType::rampIn: return "rampIn";
        case MacroStructureType::rampOut: return "rampOut";
        case MacroStructureType::arch: return "arch";
        case MacroStructureType::islands: return "islands";
        case MacroStructureType::palindrome: return "palindrome";
    }
    return "continuous";
}

juce::String toString(PhraseStructureType value)
{
    switch (value)
    {
        case PhraseStructureType::continuous: return "continuous";
        case PhraseStructureType::callResponse: return "callResponse";
        case PhraseStructureType::waves: return "waves";
        case PhraseStructureType::staircase: return "staircase";
        case PhraseStructureType::canon: return "canon";
        case PhraseStructureType::breathe: return "breathe";
    }
    return "continuous";
}

juce::String toString(AccentType value)
{
    switch (value)
    {
        case AccentType::flat: return "flat";
        case AccentType::downbeat: return "downbeat";
        case AccentType::offbeat: return "offbeat";
        case AccentType::clave: return "clave";
        case AccentType::fibonacci: return "fibonacci";
        case AccentType::prime: return "prime";
    }
    return "flat";
}

juce::String toString(TransitionType value)
{
    switch (value)
    {
        case TransitionType::cut: return "cut";
        case TransitionType::linear: return "linear";
        case TransitionType::equalPower: return "equalPower";
        case TransitionType::probabilistic: return "probabilistic";
    }
    return "cut";
}

VoiceType voiceFromString(const juce::String& text)
{
    auto t = text.trim().toLowerCase();
    if (t == "bass") return VoiceType::bass;
    if (t == "noise") return VoiceType::noise;
    if (t == "kick") return VoiceType::kick;
    if (t == "sample") return VoiceType::sample;
    if (t == "external") return VoiceType::external;
    return VoiceType::tone;
}

ScaleType scaleFromString(const juce::String& text)
{
    auto t = text.trim().toLowerCase();
    if (t == "dorian") return ScaleType::dorian;
    if (t == "aeolian") return ScaleType::aeolian;
    if (t == "chromatic") return ScaleType::chromatic;
    if (t == "whole") return ScaleType::whole;
    return ScaleType::minorPentatonic;
}

ContourType contourFromString(const juce::String& text)
{
    auto t = text.trim().toLowerCase();
    if (t == "rise") return ContourType::rise;
    if (t == "fall") return ContourType::fall;
    if (t == "arch") return ContourType::arch;
    if (t == "valley") return ContourType::valley;
    if (t == "zigzag") return ContourType::zigzag;
    if (t == "random") return ContourType::random;
    return ContourType::flat;
}

PatternType patternFromString(const juce::String& text)
{
    auto t = text.trim().toLowerCase();
    if (t == "euclidean") return PatternType::euclidean;
    if (t == "sine") return PatternType::sine;
    if (t == "golden") return PatternType::golden;
    if (t == "logistic") return PatternType::logistic;
    return PatternType::probability;
}

MacroStructureType macroStructureFromString(const juce::String& text)
{
    auto t = text.trim().toLowerCase();
    if (t == "rampin") return MacroStructureType::rampIn;
    if (t == "rampout") return MacroStructureType::rampOut;
    if (t == "arch") return MacroStructureType::arch;
    if (t == "islands") return MacroStructureType::islands;
    if (t == "palindrome") return MacroStructureType::palindrome;
    return MacroStructureType::continuous;
}

PhraseStructureType phraseStructureFromString(const juce::String& text)
{
    auto t = text.trim().toLowerCase();
    if (t == "callresponse") return PhraseStructureType::callResponse;
    if (t == "waves") return PhraseStructureType::waves;
    if (t == "staircase") return PhraseStructureType::staircase;
    if (t == "canon") return PhraseStructureType::canon;
    if (t == "breathe") return PhraseStructureType::breathe;
    return PhraseStructureType::continuous;
}

AccentType accentFromString(const juce::String& text)
{
    auto t = text.trim().toLowerCase();
    if (t == "downbeat") return AccentType::downbeat;
    if (t == "offbeat") return AccentType::offbeat;
    if (t == "clave") return AccentType::clave;
    if (t == "fibonacci") return AccentType::fibonacci;
    if (t == "prime") return AccentType::prime;
    return AccentType::flat;
}

TransitionType transitionFromString(const juce::String& text)
{
    auto t = text.trim().toLowerCase();
    if (t == "linear") return TransitionType::linear;
    if (t == "equalpower") return TransitionType::equalPower;
    if (t == "probabilistic") return TransitionType::probabilistic;
    return TransitionType::cut;
}

juce::StringArray voiceChoices() { return { "tone", "bass", "noise", "kick", "sample", "external" }; }
juce::StringArray scaleChoices() { return { "minorPentatonic", "dorian", "aeolian", "chromatic", "whole" }; }
juce::StringArray contourChoices() { return { "rise", "fall", "arch", "valley", "zigzag", "random", "flat" }; }
juce::StringArray patternChoices() { return { "probability", "euclidean", "sine", "golden", "logistic" }; }
juce::StringArray macroStructureChoices() { return { "continuous", "rampIn", "rampOut", "arch", "islands", "palindrome" }; }
juce::StringArray phraseStructureChoices() { return { "continuous", "callResponse", "waves", "staircase", "canon", "breathe" }; }
juce::StringArray accentChoices() { return { "flat", "downbeat", "offbeat", "clave", "fibonacci", "prime" }; }
juce::StringArray transitionChoices() { return { "cut", "linear", "equalPower", "probabilistic" }; }

juce::ValueTree ScoreRow::toValueTree() const
{
    juce::ValueTree v(ids::row);
    v.setProperty("name", name, nullptr);
    v.setProperty("group", group, nullptr);
    v.setProperty("startBar", startBar, nullptr);
    v.setProperty("bars", bars, nullptr);
    v.setProperty("voice", toString(voice), nullptr);
    v.setProperty("root", root, nullptr);
    v.setProperty("scale", toString(scale), nullptr);
    v.setProperty("midiLo", midiLo, nullptr);
    v.setProperty("midiHi", midiHi, nullptr);
    v.setProperty("stepBeats", stepBeats, nullptr);
    v.setProperty("noteBeats", noteBeats, nullptr);
    v.setProperty("density", density, nullptr);
    v.setProperty("macro", toString(macro), nullptr);
    v.setProperty("phrase", toString(phrase), nullptr);
    v.setProperty("phraseBeats", phraseBeats, nullptr);
    v.setProperty("pattern", toString(pattern), nullptr);
    v.setProperty("accent", toString(accent), nullptr);
    v.setProperty("accentDepth", accentDepth, nullptr);
    v.setProperty("phase", phase, nullptr);
    v.setProperty("curve", curve, nullptr);
    v.setProperty("rootExpr", rootExpr, nullptr);
    v.setProperty("densityExpr", densityExpr, nullptr);
    v.setProperty("ampExpr", ampExpr, nullptr);
    v.setProperty("transposeExpr", transposeExpr, nullptr);
    v.setProperty("amp", amp, nullptr);
    v.setProperty("pan", pan, nullptr);
    v.setProperty("cutoff", cutoff, nullptr);
    v.setProperty("contour", toString(contour), nullptr);
    v.setProperty("drift", drift, nullptr);
    v.setProperty("poly", poly, nullptr);
    v.setProperty("spread", spread, nullptr);
    v.setProperty("transpose", transpose, nullptr);
    v.setProperty("mute", mute, nullptr);
    v.setProperty("solo", solo, nullptr);
    return v;
}

ScoreRow ScoreRow::fromValueTree(const juce::ValueTree& v)
{
    ScoreRow r;
    r.name = v.getProperty("name", r.name);
    r.group = v.getProperty("group", r.group);
    r.startBar = static_cast<double>(v.getProperty("startBar", r.startBar));
    r.bars = static_cast<double>(v.getProperty("bars", r.bars));
    r.voice = voiceFromString(v.getProperty("voice", toString(r.voice)).toString());
    r.root = static_cast<int>(v.getProperty("root", r.root));
    r.scale = scaleFromString(v.getProperty("scale", toString(r.scale)).toString());
    r.midiLo = static_cast<int>(v.getProperty("midiLo", r.midiLo));
    r.midiHi = static_cast<int>(v.getProperty("midiHi", r.midiHi));
    r.stepBeats = static_cast<double>(v.getProperty("stepBeats", r.stepBeats));
    r.noteBeats = static_cast<double>(v.getProperty("noteBeats", r.noteBeats));
    r.density = static_cast<double>(v.getProperty("density", r.density));
    r.macro = macroStructureFromString(v.getProperty("macro", toString(r.macro)).toString());
    r.phrase = phraseStructureFromString(v.getProperty("phrase", toString(r.phrase)).toString());
    r.phraseBeats = static_cast<double>(v.getProperty("phraseBeats", r.phraseBeats));
    r.pattern = patternFromString(v.getProperty("pattern", toString(r.pattern)).toString());
    r.accent = accentFromString(v.getProperty("accent", toString(r.accent)).toString());
    r.accentDepth = static_cast<double>(v.getProperty("accentDepth", r.accentDepth));
    r.phase = static_cast<double>(v.getProperty("phase", r.phase));
    r.curve = static_cast<double>(v.getProperty("curve", r.curve));
    r.rootExpr = v.getProperty("rootExpr", r.rootExpr);
    r.densityExpr = v.getProperty("densityExpr", r.densityExpr);
    r.ampExpr = v.getProperty("ampExpr", r.ampExpr);
    r.transposeExpr = v.getProperty("transposeExpr", r.transposeExpr);
    r.amp = static_cast<float>(v.getProperty("amp", r.amp));
    r.pan = static_cast<float>(v.getProperty("pan", r.pan));
    r.cutoff = static_cast<float>(v.getProperty("cutoff", r.cutoff));
    r.contour = contourFromString(v.getProperty("contour", toString(r.contour)).toString());
    r.drift = static_cast<float>(v.getProperty("drift", r.drift));
    r.poly = static_cast<int>(v.getProperty("poly", r.poly));
    r.spread = static_cast<float>(v.getProperty("spread", r.spread));
    r.transpose = static_cast<int>(v.getProperty("transpose", r.transpose));
    r.mute = static_cast<bool>(v.getProperty("mute", r.mute));
    r.solo = static_cast<bool>(v.getProperty("solo", r.solo));
    return r;
}

juce::ValueTree ScoreSection::toValueTree() const
{
    juce::ValueTree v(ids::section);
    v.setProperty("name", name, nullptr);
    v.setProperty("startBar", startBar, nullptr);
    v.setProperty("bars", bars, nullptr);
    v.setProperty("activeGroups", activeGroups, nullptr);
    v.setProperty("transpose", transpose, nullptr);
    v.setProperty("densityScale", densityScale, nullptr);
    v.setProperty("ampScale", ampScale, nullptr);
    v.setProperty("transition", toString(transition), nullptr);
    v.setProperty("transitionBars", transitionBars, nullptr);
    v.setProperty("transitionProbability", transitionProbability, nullptr);
    v.setProperty("mapX", mapX, nullptr);
    v.setProperty("mapY", mapY, nullptr);
    v.setProperty("mapManual", mapManual, nullptr);
    return v;
}

ScoreSection ScoreSection::fromValueTree(const juce::ValueTree& v)
{
    ScoreSection s;
    s.name = v.getProperty("name", s.name);
    s.startBar = static_cast<double>(v.getProperty("startBar", s.startBar));
    s.bars = static_cast<double>(v.getProperty("bars", s.bars));
    s.activeGroups = v.getProperty("activeGroups", s.activeGroups);
    s.transpose = static_cast<int>(v.getProperty("transpose", s.transpose));
    s.densityScale = static_cast<double>(v.getProperty("densityScale", s.densityScale));
    s.ampScale = static_cast<double>(v.getProperty("ampScale", s.ampScale));
    s.transition = transitionFromString(v.getProperty("transition", toString(s.transition)).toString());
    s.transitionBars = static_cast<double>(v.getProperty("transitionBars", s.transitionBars));
    s.transitionProbability = static_cast<double>(v.getProperty("transitionProbability", s.transitionProbability));
    s.mapX = static_cast<float>(v.getProperty("mapX", s.mapX));
    s.mapY = static_cast<float>(v.getProperty("mapY", s.mapY));
    s.mapManual = static_cast<bool>(v.getProperty("mapManual",
                                                  std::abs(s.mapX) > 0.01f || std::abs(s.mapY) > 0.01f));
    return s;
}

bool ScoreSection::includesGroup(const juce::String& group) const
{
    auto trimmed = activeGroups.trim();
    if (trimmed.isEmpty() || trimmed == "*")
        return true;

    juce::StringArray groups;
    groups.addTokens(trimmed, ",; ", "\"");
    groups.trim();
    groups.removeEmptyStrings();
    return groups.contains(group.trim(), true);
}

void ScoreDocument::addDefaultRow()
{
    ScoreRow row;
    row.name = "Layer " + juce::String(getNumRows() + 1);
    rows.push_back(row);
}

void ScoreDocument::addDefaultSection()
{
    ScoreSection section;
    section.name = "Section " + juce::String(getNumSections() + 1);
    sections.push_back(section);
}

void ScoreDocument::removeRow(int index)
{
    if (juce::isPositiveAndBelow(index, getNumRows()))
        rows.erase(rows.begin() + index);
}

void ScoreDocument::duplicateRow(int index)
{
    if (juce::isPositiveAndBelow(index, getNumRows()))
    {
        auto copy = rows[static_cast<size_t>(index)];
        copy.name += " copy";
        rows.insert(rows.begin() + index + 1, copy);
    }
}

void ScoreDocument::removeSection(int index)
{
    if (juce::isPositiveAndBelow(index, getNumSections()))
        sections.erase(sections.begin() + index);
}

void ScoreDocument::duplicateSection(int index)
{
    if (juce::isPositiveAndBelow(index, getNumSections()))
    {
        auto copy = sections[static_cast<size_t>(index)];
        copy.name += " copy";
        copy.mapX = 0.0f;
        copy.mapY = 0.0f;
        copy.mapManual = false;
        sections.insert(sections.begin() + index + 1, copy);
    }
}

void ScoreDocument::setRows(std::vector<ScoreRow> newRows)
{
    rows = std::move(newRows);
}

void ScoreDocument::setSections(std::vector<ScoreSection> newSections)
{
    sections = std::move(newSections);
}

juce::ValueTree ScoreDocument::toValueTree() const
{
    juce::ValueTree v(ids::score);
    for (const auto& row : rows)
        v.addChild(row.toValueTree(), -1, nullptr);
    for (const auto& section : sections)
        v.addChild(section.toValueTree(), -1, nullptr);
    return v;
}

ScoreDocument ScoreDocument::fromValueTree(const juce::ValueTree& tree)
{
    ScoreDocument doc;
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        if (tree.getChild(i).hasType(ids::row))
            doc.getRows().push_back(ScoreRow::fromValueTree(tree.getChild(i)));
        else if (tree.getChild(i).hasType(ids::section))
            doc.getSections().push_back(ScoreSection::fromValueTree(tree.getChild(i)));
    }
    return doc;
}
