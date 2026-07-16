#include "ScSheetTableComponent.h"

namespace
{
    struct ColumnSpec { int id; const char* name; int width; };
    constexpr ColumnSpec columns[] = {
        { 1, "name", 130 }, { 2, "group", 64 }, { 3, "startBar", 76 }, { 4, "bars", 64 }, { 5, "voice", 92 },
        { 6, "root", 58 }, { 7, "rootExpr", 120 }, { 8, "scale", 130 }, { 9, "midiLo", 70 }, { 10, "midiHi", 70 },
        { 11, "stepBeats", 88 }, { 12, "noteBeats", 90 }, { 13, "density", 76 }, { 14, "macro", 108 },
        { 15, "phrase", 116 }, { 16, "phraseBeats", 98 }, { 17, "pattern", 104 }, { 18, "accent", 104 },
        { 19, "accentDepth", 104 }, { 20, "phase", 68 }, { 21, "curve", 68 }, { 22, "densityExpr", 132 },
        { 23, "amp", 60 }, { 24, "ampExpr", 116 }, { 25, "pan", 60 }, { 26, "cutoff", 70 }, { 27, "contour", 90 },
        { 28, "drift", 64 }, { 29, "poly", 58 }, { 30, "spread", 70 }, { 31, "transpose", 88 },
        { 32, "transposeExpr", 132 }, { 33, "mute", 58 }, { 34, "solo", 58 }
    };

    constexpr uint32_t tableText = 0xffdce7df;
    constexpr uint32_t secondary = 0xff91a39a;
    constexpr uint32_t rowA = 0xff14211c;
    constexpr uint32_t rowB = 0xff172720;
    constexpr uint32_t selectedRow = 0xff28473d;
    constexpr uint32_t line = 0xff31473e;
}

class ScoreTableComponent::TextCell : public juce::Label
{
public:
    TextCell(ScoreTableComponent& ownerRef) : owner(ownerRef)
    {
        setEditable(true, true, false);
        setJustificationType(juce::Justification::centredLeft);
        setFont(juce::Font(juce::FontOptions(12.0f)));
        setColour(juce::Label::textColourId, juce::Colour(tableText));
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::backgroundWhenEditingColourId, juce::Colour(0xff0d1512));
        setColour(juce::Label::outlineWhenEditingColourId, juce::Colour(0xff93e0d1));
        onTextChange = [this] { owner.setText(row, column, getText()); };
    }
    void setTarget(int r, int c, const juce::String& text) { row = r; column = c; setText(text, juce::dontSendNotification); }
private:
    ScoreTableComponent& owner;
    int row = 0, column = 0;
};

class ScoreTableComponent::ChoiceCell : public juce::ComboBox
{
public:
    ChoiceCell(ScoreTableComponent& ownerRef) : owner(ownerRef)
    {
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d1512).withAlpha(0.92f));
        setColour(juce::ComboBox::textColourId, juce::Colour(tableText));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(line));
        setColour(juce::ComboBox::buttonColourId, juce::Colour(0xff1d2c26));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(secondary));
        onChange = [this] { owner.setText(row, column, getText()); };
    }
    void setTarget(int r, int c, const juce::StringArray& choices, const juce::String& value)
    {
        row = r; column = c;
        clear(juce::dontSendNotification);
        for (int i = 0; i < choices.size(); ++i)
            addItem(choices[i], i + 1);
        setText(value, juce::dontSendNotification);
    }
private:
    ScoreTableComponent& owner;
    int row = 0, column = 0;
};

class ScoreTableComponent::ToggleCell : public juce::ToggleButton
{
public:
    ToggleCell(ScoreTableComponent& ownerRef) : owner(ownerRef)
    {
        setColour(juce::ToggleButton::textColourId, juce::Colour(tableText));
        setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff93e0d1));
        setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(secondary));
        onClick = [this] { owner.setText(row, column, getToggleState() ? "true" : "false"); };
    }
    void setTarget(int r, int c, bool value)
    {
        row = r; column = c;
        setToggleState(value, juce::dontSendNotification);
    }
private:
    ScoreTableComponent& owner;
    int row = 0, column = 0;
};

ScoreTableComponent::ScoreTableComponent()
{
    addAndMakeVisible(table);
    table.setMultipleSelectionEnabled(false);
    table.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff101614));
    table.setColour(juce::ListBox::outlineColourId, juce::Colour(line));
    table.getHeader().setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour(0xff1d2c26));
    table.getHeader().setColour(juce::TableHeaderComponent::textColourId, juce::Colour(secondary));
    table.getHeader().setColour(juce::TableHeaderComponent::outlineColourId, juce::Colour(line));
    for (const auto& c : columns)
        table.getHeader().addColumn(c.name, c.id, c.width, 44, 260, juce::TableHeaderComponent::defaultFlags);
}

void ScoreTableComponent::setDocument(ScoreDocument* documentToUse)
{
    document = documentToUse;
    refresh();
}

void ScoreTableComponent::setOnChange(ChangeCallback callback) { onChange = std::move(callback); }
void ScoreTableComponent::setGroupFilter(const juce::String& activeGroups)
{
    groupFilter = activeGroups.trim();
    refresh();
}

void ScoreTableComponent::clearGroupFilter()
{
    groupFilter.clear();
    refresh();
}

void ScoreTableComponent::refresh()
{
    rebuildVisibleRows();
    table.updateContent();
    table.repaint();
}

int ScoreTableComponent::getSelectedRow() const
{
    return documentRowForVisibleRow(table.getSelectedRow());
}

int ScoreTableComponent::getNumRows() { return static_cast<int>(visibleRows.size()); }

void ScoreTableComponent::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool selected)
{
    g.fillAll(selected ? juce::Colour(selectedRow) : juce::Colour(rowNumber % 2 == 0 ? rowA : rowB));
    g.setColour(juce::Colour(line));
    g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));
}

void ScoreTableComponent::paintCell(juce::Graphics& g, int, int, int width, int height, bool)
{
    g.setColour(juce::Colour(line));
    g.drawVerticalLine(width - 1, 0.0f, static_cast<float>(height));
}

juce::Component* ScoreTableComponent::refreshComponentForCell(int row, int column, bool, juce::Component* existing)
{
    if (column == mute || column == solo)
    {
        auto* cell = dynamic_cast<ToggleCell*>(existing);
        if (cell == nullptr) cell = new ToggleCell(*this);
        cell->setTarget(row, column, getText(row, column) == "true");
        return cell;
    }

    if (column == voice || column == scale || column == contour || column == pattern || column == macro || column == phrase || column == accent)
    {
        auto* cell = dynamic_cast<ChoiceCell*>(existing);
        if (cell == nullptr) cell = new ChoiceCell(*this);
        cell->setTarget(row, column, choicesForColumn(column), getText(row, column));
        return cell;
    }

    auto* cell = dynamic_cast<TextCell*>(existing);
    if (cell == nullptr) cell = new TextCell(*this);
    cell->setTarget(row, column, getText(row, column));
    return cell;
}

void ScoreTableComponent::resized()
{
    table.setBounds(getLocalBounds());
}

juce::String ScoreTableComponent::getText(int row, int column) const
{
    const auto documentRow = documentRowForVisibleRow(row);
    if (document == nullptr || ! juce::isPositiveAndBelow(documentRow, document->getNumRows()))
        return {};
    const auto& r = document->getRow(documentRow);
    switch (column)
    {
        case name: return r.name;
        case group: return r.group;
        case startBar: return juce::String(r.startBar);
        case bars: return juce::String(r.bars);
        case voice: return toString(r.voice);
        case root: return juce::String(r.root);
        case rootExpr: return r.rootExpr;
        case scale: return toString(r.scale);
        case midiLo: return juce::String(r.midiLo);
        case midiHi: return juce::String(r.midiHi);
        case stepBeats: return juce::String(r.stepBeats);
        case noteBeats: return juce::String(r.noteBeats);
        case density: return juce::String(r.density);
        case macro: return toString(r.macro);
        case phrase: return toString(r.phrase);
        case phraseBeats: return juce::String(r.phraseBeats);
        case pattern: return toString(r.pattern);
        case accent: return toString(r.accent);
        case accentDepth: return juce::String(r.accentDepth);
        case phase: return juce::String(r.phase);
        case curve: return juce::String(r.curve);
        case densityExpr: return r.densityExpr;
        case amp: return juce::String(r.amp);
        case ampExpr: return r.ampExpr;
        case pan: return juce::String(r.pan);
        case cutoff: return juce::String(r.cutoff);
        case contour: return toString(r.contour);
        case drift: return juce::String(r.drift);
        case poly: return juce::String(r.poly);
        case spread: return juce::String(r.spread);
        case transpose: return juce::String(r.transpose);
        case transposeExpr: return r.transposeExpr;
        case mute: return r.mute ? "true" : "false";
        case solo: return r.solo ? "true" : "false";
        default: return {};
    }
}

void ScoreTableComponent::setText(int row, int column, const juce::String& text)
{
    const auto documentRow = documentRowForVisibleRow(row);
    if (document == nullptr || ! juce::isPositiveAndBelow(documentRow, document->getNumRows()))
        return;
    auto& r = document->getRow(documentRow);
    switch (column)
    {
        case name: r.name = text; break;
        case group: r.group = text; break;
        case startBar: r.startBar = text.getDoubleValue(); break;
        case bars: r.bars = text.getDoubleValue(); break;
        case voice: r.voice = voiceFromString(text); break;
        case root: r.root = text.getIntValue(); break;
        case rootExpr: r.rootExpr = text; break;
        case scale: r.scale = scaleFromString(text); break;
        case midiLo: r.midiLo = text.getIntValue(); break;
        case midiHi: r.midiHi = text.getIntValue(); break;
        case stepBeats: r.stepBeats = text.getDoubleValue(); break;
        case noteBeats: r.noteBeats = text.getDoubleValue(); break;
        case density: r.density = text.getDoubleValue(); break;
        case macro: r.macro = macroStructureFromString(text); break;
        case phrase: r.phrase = phraseStructureFromString(text); break;
        case phraseBeats: r.phraseBeats = text.getDoubleValue(); break;
        case pattern: r.pattern = patternFromString(text); break;
        case accent: r.accent = accentFromString(text); break;
        case accentDepth: r.accentDepth = text.getDoubleValue(); break;
        case phase: r.phase = text.getDoubleValue(); break;
        case curve: r.curve = text.getDoubleValue(); break;
        case densityExpr: r.densityExpr = text; break;
        case amp: r.amp = text.getFloatValue(); break;
        case ampExpr: r.ampExpr = text; break;
        case pan: r.pan = text.getFloatValue(); break;
        case cutoff: r.cutoff = text.getFloatValue(); break;
        case contour: r.contour = contourFromString(text); break;
        case drift: r.drift = text.getFloatValue(); break;
        case poly: r.poly = text.getIntValue(); break;
        case spread: r.spread = text.getFloatValue(); break;
        case transpose: r.transpose = text.getIntValue(); break;
        case transposeExpr: r.transposeExpr = text; break;
        case mute: r.mute = text == "true"; break;
        case solo: r.solo = text == "true"; break;
        default: break;
    }
    notifyChanged();
}

int ScoreTableComponent::documentRowForVisibleRow(int row) const
{
    if (! juce::isPositiveAndBelow(row, static_cast<int>(visibleRows.size())))
        return -1;

    return visibleRows[static_cast<size_t>(row)];
}

bool ScoreTableComponent::rowPassesFilter(const ScoreRow& row) const
{
    auto trimmed = groupFilter.trim();
    if (trimmed.isEmpty() || trimmed == "*")
        return true;

    juce::StringArray groups;
    groups.addTokens(trimmed, ",; ", "\"");
    groups.trim();
    groups.removeEmptyStrings();
    return groups.contains(row.group.trim(), true);
}

void ScoreTableComponent::rebuildVisibleRows()
{
    visibleRows.clear();
    if (document == nullptr)
        return;

    for (int i = 0; i < document->getNumRows(); ++i)
        if (rowPassesFilter(document->getRow(i)))
            visibleRows.push_back(i);
}

juce::StringArray ScoreTableComponent::choicesForColumn(int column) const
{
    if (column == voice) return voiceChoices();
    if (column == scale) return scaleChoices();
    if (column == contour) return contourChoices();
    if (column == pattern) return patternChoices();
    if (column == macro) return macroStructureChoices();
    if (column == phrase) return phraseStructureChoices();
    if (column == accent) return accentChoices();
    return {};
}

void ScoreTableComponent::notifyChanged()
{
    if (onChange)
        onChange();
}
