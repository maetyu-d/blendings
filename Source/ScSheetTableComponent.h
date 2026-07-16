#pragma once

#include "ScSheetScore.h"

#include <juce_gui_basics/juce_gui_basics.h>

class ScoreTableComponent : public juce::Component,
                            public juce::TableListBoxModel
{
public:
    using ChangeCallback = std::function<void()>;

    ScoreTableComponent();
    void setDocument(ScoreDocument* documentToUse);
    void setOnChange(ChangeCallback callback);
    void setGroupFilter(const juce::String& activeGroups);
    void clearGroupFilter();
    void refresh();
    int getSelectedRow() const;

    int getNumRows() override;
    void paintRowBackground(juce::Graphics&, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics&, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;
    void resized() override;

private:
    enum Column
    {
        name = 1, group, startBar, bars, voice, root, rootExpr, scale, midiLo, midiHi, stepBeats, noteBeats,
        density, macro, phrase, phraseBeats, pattern, accent, accentDepth, phase, curve,
        densityExpr, amp, ampExpr, pan, cutoff, contour, drift, poly, spread, transpose, transposeExpr, mute, solo
    };

    class TextCell;
    class ChoiceCell;
    class ToggleCell;

    juce::String getText(int row, int column) const;
    void setText(int row, int column, const juce::String& text);
    int documentRowForVisibleRow(int row) const;
    bool rowPassesFilter(const ScoreRow& row) const;
    void rebuildVisibleRows();
    juce::StringArray choicesForColumn(int column) const;
    void notifyChanged();

    ScoreDocument* document = nullptr;
    ChangeCallback onChange;
    juce::String groupFilter;
    std::vector<int> visibleRows;
    juce::TableListBox table { "Score", this };
};
