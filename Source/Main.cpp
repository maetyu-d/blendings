#include <JuceHeader.h>

#include "GridEditorComponent.h"
#include "CarouselEditorComponent.h"
#include "GridModel.h"
#include "PipeWorkspaceComponent.h"
#include "ScDiscAudioEngine.h"
#include "ScSheetScore.h"
#include "ScSheetTableComponent.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <limits>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

#ifndef OTHERWARE_PD_EXTRA_ROOT
#define OTHERWARE_PD_EXTRA_ROOT ""
#endif

namespace
{
constexpr float gridSize = 24.0f;
constexpr float snapRadius = 18.0f;
constexpr float discLineSnapRadius = gridSize;
constexpr float minPointDistance = 10.0f;
constexpr float routeJoinTolerance = 2.0f;
constexpr float nodeRadius = 5.0f;
constexpr float lineWidth = 4.0f;
constexpr float minViewScale = 0.35f;
constexpr float maxViewScale = 4.0f;
constexpr int toolbarHeight = 52;
constexpr int dataPaneWidth = 380;
constexpr int maxDiscElementsPerOrbitRing = 16;
constexpr float discOrbitRingSpacing = 12.0f;
constexpr const char* pdStdPathPrefix = "std:";

juce::File bundledPdExtraRoot()
{
    return juce::File (juce::String (OTHERWARE_PD_EXTRA_ROOT).unquoted());
}

juce::Colour appBackground()     { return juce::Colour (0xff0d100f); }
juce::Colour surfaceColour()     { return juce::Colour (0xff161a18); }
juce::Colour raisedSurface()     { return juce::Colour (0xff232725); }
juce::Colour subtleStroke()      { return juce::Colour (0xff363b38); }
juce::Colour textPrimary()       { return juce::Colour (0xfff2f2f2); }
juce::Colour textMuted()         { return juce::Colour (0xff989d9a); }
juce::Colour backgroundTop()     { return juce::Colour (0xff101a16); }
juce::Colour backgroundBottom()  { return juce::Colour (0xff0d1411); }
juce::Colour gridMajorColour()   { return juce::Colour (0x204c7a6f); }
juce::Colour gridMinorColour()   { return juce::Colour (0x103f6a60); }
juce::Colour lineColour()        { return juce::Colour (0xff65f0d0); }
juce::Colour lineDeepColour()    { return juce::Colour (0xff176175); }
juce::Colour lineLightColour()   { return juce::Colour (0xfffff0a6); }
juce::Colour discColour()        { return juce::Colour (0xffffa45f); }
juce::Colour discHotColour()     { return juce::Colour (0xffffd66e); }
juce::Colour discCoolColour()    { return juce::Colour (0xffc45cff); }
juce::Colour discRimColour()     { return juce::Colour (0xff70f5d5); }
juce::Colour soundElementColour(){ return juce::Colour (0xfff2d279); }
juce::Colour worldElementColour(){ return juce::Colour (0xff34c7ff); }
juce::Colour scCodeElementColour(){ return juce::Colour (0xffb9ff5d); }
juce::Colour scSheetElementColour(){ return juce::Colour (0xffff5fb7); }
juce::Colour orcaGridElementColour(){ return juce::Colour (0xff9b7cff); }
juce::Colour pdPatchElementColour(){ return juce::Colour (0xffff744f); }
juce::Colour carouselElementColour(){ return juce::Colour (0xffffc857); }
juce::Colour pipeElementColour()    { return juce::Colour (0xff42e8c2); }
juce::Colour accentColour()      { return juce::Colour (0xff0a84ff); }

juce::Colour pipeColourForIndex (int index)
{
    static constexpr std::array<uint32_t, 7> colours {{
        0xffff7aa8, 0xffff9d72, 0xffffdd6d, 0xff8ef6a3,
        0xff58d8ff, 0xff9aa7ff, 0xffd58cff
    }};
    return juce::Colour (colours[static_cast<size_t> (std::abs (index) % static_cast<int> (colours.size()))]);
}

int pipePaletteIndex (juce::Colour colour)
{
    for (int i = 0; i < 7; ++i) if (pipeColourForIndex (i) == colour) return i;
    return 0;
}

float durationFromText (juce::String text)
{
    const auto trimmed = text.trim();

    if (trimmed.isEmpty() || trimmed.startsWithChar ('-'))
        return -1.0f;

    return juce::jlimit (0.25f, 9999.0f, trimmed.getFloatValue());
}

juce::String durationText (float seconds)
{
    return seconds < 0.0f ? "-" : juce::String (seconds, 2);
}

juce::String durationLabelText (float seconds)
{
    return seconds < 0.0f ? "Duration: -  code decides"
                          : "Duration: " + juce::String (seconds, 2) + " s";
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
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
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

juce::String defaultPdPatch()
{
    return R"PD(#N canvas 120 120 520 360 10;
#X obj 48 42 r trigger;
#X obj 48 76 bng 20 250 50 0 empty empty fire 0 -10 0 12 #fcfcfc #000000 #000000;
#X msg 48 112 0 \, 0.22 5 \, 0 420 5;
#X obj 48 148 vline~;
#X obj 164 112 osc~ 220;
#X obj 164 148 *~;
#X obj 164 188 dac~;
#X connect 0 0 1 0;
#X connect 1 0 2 0;
#X connect 2 0 3 0;
#X connect 3 0 5 1;
#X connect 4 0 5 0;
#X connect 5 0 6 0;
#X connect 5 0 6 1;
)PD";
}

juce::String pdPatchEditorHtml()
{
    static const auto html = []
    {
        const juce::File candidates[] {
            juce::File (OTHERWARE_PD_EDITOR_HTML),
            juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                .getChildFile ("Contents/Resources/PdPatchEditor.html"),
            juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                .getParentDirectory()
                .getSiblingFile ("Resources")
                .getChildFile ("PdPatchEditor.html")
        };

        for (const auto& file : candidates)
            if (file.existsAsFile())
                return file.loadFileAsString();

        return juce::String ("<!doctype html><html><body style=\"font:13px sans-serif;background:#0b0f0d;color:#e9efe8\">Pd editor resource missing.</body></html>");
    }();

    return html;
}

juce::WebBrowserComponent::Resource stringWebResource (const juce::String& text, const juce::String& mimeType)
{
    const auto* raw = text.toRawUTF8();
    const auto size = std::strlen (raw);
    std::vector<std::byte> data (size);
    std::memcpy (data.data(), raw, size);
    return { std::move (data), mimeType };
}

class SuperColliderTokeniser final : public juce::CodeTokeniser
{
public:
    enum TokenType
    {
        error = 0,
        comment,
        keyword,
        ugen,
        identifier,
        number,
        string,
        symbol,
        bracket,
        operatorToken,
        punctuation
    };

    int readNextToken (juce::CodeDocument::Iterator& source) override
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

    juce::CodeEditorComponent::ColourScheme getDefaultColourScheme() override
    {
        juce::CodeEditorComponent::ColourScheme scheme;
        scheme.set ("Error", juce::Colour (0xffff5a68));
        scheme.set ("Comment", juce::Colour (0xff607069));
        scheme.set ("Keyword", juce::Colour (0xffffd76a));
        scheme.set ("UGen/Class", juce::Colour (0xff34c7ff));
        scheme.set ("Identifier", textPrimary());
        scheme.set ("Number", juce::Colour (0xffff9a62));
        scheme.set ("String", juce::Colour (0xffb9ff5d));
        scheme.set ("Symbol", juce::Colour (0xffff5fb7));
        scheme.set ("Bracket", juce::Colour (0xff9b7cff));
        scheme.set ("Operator", juce::Colour (0xff93e0d1));
        scheme.set ("Punctuation", textMuted());
        return scheme;
    }

private:
    static bool isIdentifierStart (juce::juce_wchar c) noexcept
    {
        return c == '_' || juce::CharacterFunctions::isLetter (c);
    }

    static bool isIdentifierBody (juce::juce_wchar c) noexcept
    {
        return c == '_' || juce::CharacterFunctions::isLetterOrDigit (c);
    }

    static bool isKeyword (const juce::String& token)
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
};

juce::String defaultScCode()
{
    return "var trig = Impulse.kr(tempo / 30);\n"
           "var step = Demand.kr(trig, 0, Dseq([0, 3, 7, 10, 12], inf));\n"
           "var hold = Select.kr(sustain < 0, [sustain.max(0.25), 99999]);\n"
           "var env = EnvGen.kr(Env.linen(0.01, hold, 0.35), doneAction: 2);\n"
           "var tone = SinOsc.ar((pitch + step).midicps) * Decay2.kr(trig, 0.01, 0.18);\n"
           "tone = (tone + (Pulse.ar(pitch.midicps * 0.5, 0.35) * 0.12)) * env * amp;\n"
           "Out.ar(out, Pan2.ar(tone.tanh, pan));";
}

ScoreDocument defaultScSheet()
{
    ScoreDocument document;
    document.addDefaultSection();
    auto& section = document.getSection (0);
    section.name = "Disc cell";
    section.startBar = 0.0;
    section.bars = 4.0;
    section.activeGroups = "*";

    document.addDefaultRow();
    auto& kick = document.getRow (0);
    kick.name = "Grid kick";
    kick.group = "drums";
    kick.voice = VoiceType::kick;
    kick.root = 36;
    kick.startBar = 0.0;
    kick.bars = 4.0;
    kick.stepBeats = 1.0;
    kick.noteBeats = 0.16;
    kick.density = 1.0;
    kick.amp = 0.50f;
    kick.pattern = PatternType::euclidean;
    kick.accent = AccentType::downbeat;
    kick.accentDepth = 0.35;
    kick.midiLo = 30;
    kick.midiHi = 42;

    document.addDefaultRow();
    auto& pulse = document.getRow (1);
    pulse.name = "Cell pulse";
    pulse.group = "tone";
    pulse.voice = VoiceType::tone;
    pulse.root = 60;
    pulse.scale = ScaleType::minorPentatonic;
    pulse.midiLo = 48;
    pulse.midiHi = 76;
    pulse.startBar = 0.0;
    pulse.bars = 4.0;
    pulse.stepBeats = 0.5;
    pulse.noteBeats = 0.24;
    pulse.density = 0.68;
    pulse.amp = 0.18f;
    pulse.pan = 0.16f;
    pulse.pattern = PatternType::golden;
    pulse.contour = ContourType::arch;
    pulse.phrase = PhraseStructureType::waves;
    pulse.phraseBeats = 4.0;
    pulse.accent = AccentType::fibonacci;
    pulse.accentDepth = 0.32;

    return document;
}

gridcollider::GridModel::Snapshot defaultOrcaGrid()
{
    gridcollider::GridModel grid (16, 10);
    const juce::StringArray rows {
        "..1D4...........",
        "..*:05Cf4.......",
        "................",
        "..2D8...........",
        "..*:15Ge6.......",
        "................",
        "# edit like ORCA#",
        "................",
        "................",
        "................"
    };

    for (int y = 0; y < rows.size(); ++y)
        for (int x = 0; x < rows[y].length(); ++x)
            grid.setGlyph (x, y, static_cast<char> (rows[y][x]));

    return grid.createSnapshot();
}

float distanceFromLineSegment (juce::Point<float> point,
                               juce::Point<float> lineStart,
                               juce::Point<float> lineEnd)
{
    const auto line = lineEnd - lineStart;
    const auto lengthSquared = line.getDistanceSquaredFromOrigin();

    if (lengthSquared <= 0.0001f)
        return point.getDistanceFrom (lineStart);

    const auto t = juce::jlimit (0.0f, 1.0f,
                                 ((point.x - lineStart.x) * line.x + (point.y - lineStart.y) * line.y) / lengthSquared);
    return point.getDistanceFrom (lineStart + line * t);
}

juce::Point<float> closestPointOnLineSegment (juce::Point<float> point,
                                              juce::Point<float> lineStart,
                                              juce::Point<float> lineEnd)
{
    const auto line = lineEnd - lineStart;
    const auto lengthSquared = line.getDistanceSquaredFromOrigin();

    if (lengthSquared <= 0.0001f)
        return lineStart;

    const auto t = juce::jlimit (0.0f, 1.0f,
                                 ((point.x - lineStart.x) * line.x + (point.y - lineStart.y) * line.y) / lengthSquared);
    return lineStart + line * t;
}

juce::Point<float> snapToGrid (juce::Point<float> point)
{
    return { std::round (point.x / gridSize) * gridSize,
             std::round (point.y / gridSize) * gridSize };
}

}

struct RoadRoute
{
    std::vector<juce::Point<float>> points;
    juce::Colour colour = lineColour();
    bool isPipe = false;
    bool isWarpPipe = false;

    bool isDrawable() const noexcept { return points.size() >= 2; }

    float getLength() const
    {
        float length = 0.0f;

        for (size_t i = 1; i < points.size(); ++i)
            length += points[i - 1].getDistanceFrom (points[i]);

        return length;
    }
};

struct ScCodeElement
{
    juce::String code = defaultScCode();
    float durationSeconds = -1.0f;
};

struct PdPatchElement
{
    juce::String patch = defaultPdPatch();
    juce::String searchPath;
    float durationSeconds = -1.0f;
};

struct SequencingClock
{
    juce::String name;
    double ratio = 1.0;
    double phaseBeats = 0.0;
    double swing = 0.0;
    bool enabled = false;
};

static std::array<SequencingClock, 4> defaultSequencingClocks()
{
    return {{
        { "Half time", 0.5, 0.0, 0.0, false },
        { "Double time", 2.0, 0.0, 0.0, false },
        { "Three over two", 1.5, 0.0, 0.0, false },
        { "Offset", 1.0, 0.5, 0.0, false }
    }};
}

struct PipeTap
{
    juce::Point<float> position;
    double intervalBeats = 1.0;
    double speed = 1.0;
    double probability = 1.0;
    bool reverse = false;
    bool enabled = true;
    int totalDrops = 0;
    bool randomInterval = false;
    double intervalLowBeats = 0.5;
    double intervalHighBeats = 2.0;
    bool randomSpeed = false;
    double speedLow = 0.5;
    double speedHigh = 1.5;
    int clockIndex = 0;
    int emittedDrops = 0;
    double nextEmissionBeat = 0.0;
    juce::String id { juce::Uuid().toString() };
};

struct PipeDrain
{
    juce::Point<float> position;
    double destructionProbability = 0.5;
    bool enabled = true;
    juce::String id { juce::Uuid().toString() };
};

struct PipeCloner
{
    enum class DirectionMode { all = 0, random, straightest, leftmost, rightmost };

    juce::Point<float> position;
    DirectionMode straightMode = DirectionMode::all;
    DirectionMode twoWayMode = DirectionMode::all;
    DirectionMode threeWayMode = DirectionMode::all;
    DirectionMode fourWayMode = DirectionMode::all;
    int maxClones = 4;
    double cloneProbability = 1.0;
    bool enabled = true;
    juce::String id { juce::Uuid().toString() };
};

struct PipeSpeedLimit
{
    juce::Point<float> position;
    double bpmMultiplier = 1.0;
    double affectProbability = 1.0;
    bool enabled = true;
    juce::String id { juce::Uuid().toString() };
};

struct PipeWait
{
    juce::Point<float> position;
    double beats = 1.0;
    bool enabled = true;
    juce::String id { juce::Uuid().toString() };
};

struct PipeStrike
{
    juce::Point<float> position;
    int maxDiscs = 4;
    bool left = true, right = true, up = true, down = true;
    bool enabled = true;
    juce::String id { juce::Uuid().toString() };
};

struct PipeTeleport
{
    juce::Point<float> position;
    juce::String id { juce::Uuid().toString() };
    juce::String destinationId;
    double probability = 1.0;
    int maxPerWindow = 4;
    double windowBars = 1.0;
    int stopAfter = 0;
    int totalTeleported = 0;
    int windowCount = 0;
    double windowStartBeat = 0.0;
    bool enabled = true;
};

struct PipeFilter
{
    enum class Mode { highpass = 0, lowpass, bandpass };
    juce::Point<float> position;
    Mode mode = Mode::lowpass;
    double lowSpeed = 0.5;
    double highSpeed = 1.5;
    bool enabled = true;
    juce::String id { juce::Uuid().toString() };
};

struct PipeLogic
{
    enum class Mode { gate = 0, counter, switcher, comparator, flipFlop, everyNth };
    enum class Comparison { less = 0, lessOrEqual, equal, greaterOrEqual, greater };
    enum class Branch { left = 0, straight, right, random };

    juce::Point<float> position;
    Mode mode = Mode::gate;
    Comparison comparison = Comparison::greaterOrEqual;
    Branch branch = Branch::straight;
    int targetCount = 4;
    double compareSpeed = 1.0;
    bool gateOpen = true;
    bool enabled = true;

    // Runtime state is reset with the transport and is not persisted.
    int count = 0;
    bool flipState = false;
    juce::String id { juce::Uuid().toString() };
};

enum class ModulationTargetKind
{
    disc = 0,
    tap,
    drain,
    quantum,
    speedLimit,
    wait,
    strike,
    teleport,
    filter,
    logic
};

struct Modulator
{
    enum class Shape { sine = 0, triangle, square, random };

    juce::String id { juce::Uuid().toString() };
    juce::Point<float> position;
    juce::String name { "Modulator" };
    Shape shape = Shape::sine;
    double cycleBeats = 4.0;
    double phase = 0.0;
    bool enabled = true;
};

struct ModulationConnection
{
    juce::String sourceId;
    ModulationTargetKind targetKind = ModulationTargetKind::disc;
    juce::String targetId;
    int parameter = 0;
    double depth = 0.5;
    double offset = 0.0;
    bool inverted = false;
};

static juce::String modulationTargetName (ModulationTargetKind kind)
{
    switch (kind)
    {
        case ModulationTargetKind::disc:       return "Disc";
        case ModulationTargetKind::tap:        return "Tap";
        case ModulationTargetKind::drain:      return "Drain";
        case ModulationTargetKind::quantum:    return "Quantum";
        case ModulationTargetKind::speedLimit: return "Speed limit";
        case ModulationTargetKind::wait:       return "Wait";
        case ModulationTargetKind::strike:     return "Strike";
        case ModulationTargetKind::teleport:   return "Teleport";
        case ModulationTargetKind::filter:     return "Filter";
        case ModulationTargetKind::logic:      return "Logic";
    }
    return "Target";
}

static juce::StringArray modulationParameterNames (ModulationTargetKind kind)
{
    switch (kind)
    {
        case ModulationTargetKind::disc:       return { "Level", "Pan", "Element chance" };
        case ModulationTargetKind::tap:        return { "Drop chance", "Speed", "Interval" };
        case ModulationTargetKind::drain:      return { "Drain chance" };
        case ModulationTargetKind::quantum:    return { "Clone chance", "Clone count" };
        case ModulationTargetKind::speedLimit: return { "Speed", "Affect chance" };
        case ModulationTargetKind::wait:       return { "Wait time" };
        case ModulationTargetKind::strike:     return { "Disc count" };
        case ModulationTargetKind::teleport:   return { "Teleport chance" };
        case ModulationTargetKind::filter:     return { "Low threshold", "High threshold" };
        case ModulationTargetKind::logic:      return { "Gate", "Compare speed", "Count" };
    }
    return { "Amount" };
}

struct Disc
{
    enum class TriggerMode { everyDrop = 0, whenFinished, exclusive };
    enum class ElementMode { together = 0, sequential, chain, random, probability };

    juce::Point<float> centre;
    TriggerMode triggerMode = TriggerMode::everyDrop;
    ElementMode elementMode = ElementMode::together;
    double elementProbability = 0.5;
    bool holdDropsUntilFinished = false;
    float level = 1.0f;
    float pan = 0.0f;
    bool muted = false;
    bool solo = false;
    int soundElementCount = 0;
    int nestedWorldCount = 0;
    std::vector<ScCodeElement> scCodeElements;
    std::vector<PdPatchElement> pdPatches;
    std::vector<ScoreDocument> scSheets;
    std::vector<gridcollider::GridModel::Snapshot> orcaGrids;
    std::vector<CarouselDocument> carousels;
    std::vector<juce::String> pipeWorlds;
    std::vector<RoadRoute> nestedRoutes;
    std::vector<PipeTap> nestedPipeTaps;
    std::vector<PipeDrain> nestedPipeDrains;
    std::vector<PipeCloner> nestedPipeCloners;
    std::vector<PipeSpeedLimit> nestedPipeSpeedLimits;
    std::vector<PipeWait> nestedPipeWaits;
    std::vector<PipeStrike> nestedPipeStrikes;
    std::vector<PipeTeleport> nestedPipeTeleports;
    std::vector<PipeFilter> nestedPipeFilters;
    std::vector<PipeLogic> nestedPipeLogics;
    std::vector<Disc> nestedDiscs;
    juce::String id { juce::Uuid().toString() };

    int getElementCount() const noexcept
    {
        return soundElementCount + nestedWorldCount + static_cast<int> (scCodeElements.size())
             + static_cast<int> (pdPatches.size()) + static_cast<int> (scSheets.size())
             + static_cast<int> (orcaGrids.size()) + static_cast<int> (carousels.size())
             + static_cast<int> (pipeWorlds.size());
    }

    bool hasNestedWorld() const noexcept { return nestedWorldCount > 0; }
    bool hasScCodeElement() const noexcept
    {
        return ! scCodeElements.empty();
    }
    bool hasScSheet() const noexcept { return ! scSheets.empty(); }
    bool hasOrcaGrid() const noexcept { return ! orcaGrids.empty(); }
    bool hasPdPatch() const noexcept { return ! pdPatches.empty(); }
    bool hasCarousel() const noexcept { return ! carousels.empty(); }
    bool hasPipeWorld() const noexcept { return ! pipeWorlds.empty(); }
};

class IconButton final : public juce::Button
{
public:
    enum class Icon
    {
        select,
        draw,
        warpPipe,
        edit,
        disc,
        tap,
        drain,
        clone,
        speedLimit,
        wait,
        strike,
        teleport,
        filter,
        logic,
        modulator,
        modConnect,
        fadeOrbits,
        addElement,
        nestedWorld,
        exitWorld,
        erase,
        undo,
        clear,
        exportImage
    };

    IconButton (Icon iconToDraw, juce::String tooltip)
        : juce::Button (std::move (tooltip)), icon (iconToDraw)
    {
        setTooltip (getName());
        setClickingTogglesState (false);
        setTriggeredOnMouseDown (false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setIconScale (float newScale)
    {
        iconScale = juce::jlimit (0.5f, 1.5f, newScale);
        repaint();
    }

    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.5f);
        const auto isActive = getToggleState();
        auto base = isActive ? accentColour()
                             : juce::Colour (0x00000000);

        if (! isActive && shouldDrawButtonAsHighlighted)
            base = raisedSurface();
        else if (shouldDrawButtonAsHighlighted)
            base = base.brighter (0.08f);

        if (shouldDrawButtonAsDown)
            base = base.darker (0.12f);

        if (base.getAlpha() > 0)
        {
            g.setColour (base);
            g.fillRoundedRectangle (bounds, 6.0f);
        }

        if (isActive || shouldDrawButtonAsHighlighted)
        {
            g.setColour ((isActive ? accentColour().brighter (0.18f) : subtleStroke()).withAlpha (0.88f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 0.8f);
        }

        g.setColour (isActive ? juce::Colours::white : textPrimary().withAlpha (0.90f));
        const auto iconArea = bounds.withSizeKeepingCentre ((bounds.getWidth() - 14.0f) * iconScale,
                                                           (bounds.getHeight() - 14.0f) * iconScale);
        drawIcon (g, iconArea);
    }

private:
    Icon icon;
    float iconScale = 1.0f;

    void drawIcon (juce::Graphics& g, juce::Rectangle<float> area) const
    {
        switch (icon)
        {
            case Icon::select:
            {
                juce::Path p;
                p.startNewSubPath (area.getX() + 1.0f, area.getY() + 1.0f);
                p.lineTo (area.getRight() - 1.0f, area.getCentreY() - 1.0f);
                p.lineTo (area.getCentreX() + 2.0f, area.getCentreY() + 2.0f);
                p.lineTo (area.getRight() - 2.0f, area.getBottom() - 3.0f);
                p.lineTo (area.getRight() - 6.0f, area.getBottom());
                p.lineTo (area.getCentreX() - 1.0f, area.getCentreY() + 5.0f);
                p.lineTo (area.getX() + 5.0f, area.getBottom() - 1.0f);
                p.closeSubPath();
                g.fillPath (p);
                break;
            }

            case Icon::draw:
            {
                const auto x0 = area.getX() + 3.0f;
                const auto x1 = area.getCentreX() + 1.0f;
                const auto x2 = area.getRight() - 3.0f;
                const auto y0 = area.getY() + 5.0f;
                const auto y1 = area.getBottom() - 5.0f;

                juce::Path p;
                p.startNewSubPath (x0, y0);
                p.lineTo (x1, y0);
                p.lineTo (x1, y1);
                p.lineTo (x2, y1);
                g.strokePath (p, juce::PathStrokeType (3.6f, juce::PathStrokeType::mitered,
                                                       juce::PathStrokeType::butt));

                // Compact flange caps make the endpoints read as pipe fittings.
                g.drawLine ({ x0, y0 - 3.5f, x0, y0 + 3.5f }, 2.4f);
                g.drawLine ({ x2, y1 - 3.5f, x2, y1 + 3.5f }, 2.4f);

                // Reinforce the elbow so the square joint remains solid at small sizes.
                g.fillRect (juce::Rectangle<float> (3.6f, 3.6f).withCentre ({ x1, y0 }));
                g.fillRect (juce::Rectangle<float> (3.6f, 3.6f).withCentre ({ x1, y1 }));
                break;
            }

            case Icon::warpPipe:
            {
                juce::Path p;
                p.startNewSubPath (area.getX() + 2.0f, area.getCentreY());
                p.lineTo (area.getRight() - 2.0f, area.getCentreY());
                const float dashes[] { 2.5f, 3.5f };
                juce::Path dotted;
                juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
                    .createDashedStroke (dotted, p, dashes, 2);
                g.fillPath (dotted);
                g.drawEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ area.getX() + 2.5f, area.getCentreY() }), 1.6f);
                g.drawEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ area.getRight() - 2.5f, area.getCentreY() }), 1.6f);
                break;
            }

            case Icon::edit:
            {
                const auto left = juce::Point<float> (area.getX() + 4.0f, area.getCentreY() - 3.0f);
                const auto middle = juce::Point<float> (area.getCentreX(), area.getBottom() - 4.0f);
                const auto right = juce::Point<float> (area.getRight() - 4.0f, area.getCentreY() - 3.0f);
                constexpr float nodeDiameter = 6.5f;

                juce::Path path;
                path.startNewSubPath (left);
                path.lineTo (middle);
                path.lineTo (right);
                g.strokePath (path, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));

                for (const auto point : { left, middle, right })
                {
                    const auto node = juce::Rectangle<float> (nodeDiameter, nodeDiameter).withCentre (point);
                    g.fillEllipse (node);
                    g.setColour (getToggleState() ? accentColour() : appBackground());
                    g.fillEllipse (node.reduced (2.0f));
                    g.setColour (getToggleState() ? juce::Colours::white : textPrimary().withAlpha (0.90f));
                }
                break;
            }

            case Icon::disc:
            {
                g.drawEllipse (area.reduced (1.5f), 2.0f);
                g.fillEllipse (area.reduced (6.0f));
                g.setColour ((getToggleState() ? juce::Colours::white : textPrimary()).withAlpha (0.38f));
                g.drawEllipse (area.reduced (4.0f), 1.2f);
                break;
            }

            case Icon::tap:
            {
                const auto handleY = area.getY() + 4.0f;
                g.drawLine ({ area.getX() + 3.0f, handleY,
                              area.getRight() - 3.0f, handleY }, 3.0f);
                g.fillEllipse (juce::Rectangle<float> (5.5f, 5.5f)
                                   .withCentre ({ area.getX() + 3.0f, handleY }));
                g.fillEllipse (juce::Rectangle<float> (5.5f, 5.5f)
                                   .withCentre ({ area.getRight() - 3.0f, handleY }));
                g.drawLine ({ area.getCentreX(), handleY,
                              area.getCentreX(), area.getCentreY() + 1.0f }, 3.0f);
                g.fillEllipse (juce::Rectangle<float> (8.0f, 7.0f)
                                   .withCentre ({ area.getCentreX(), area.getCentreY() + 2.0f }));
                juce::Path spout;
                spout.startNewSubPath (area.getRight() - 1.0f, area.getCentreY() + 1.0f);
                spout.lineTo (area.getX() + 5.0f, area.getCentreY() + 1.0f);
                spout.lineTo (area.getX() + 5.0f, area.getBottom() - 3.0f);
                spout.lineTo (area.getX() + 1.0f, area.getBottom() - 3.0f);
                g.strokePath (spout, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
                g.drawLine ({ area.getRight() - 2.0f, area.getCentreY() - 2.0f,
                              area.getRight() - 2.0f, area.getCentreY() + 5.0f }, 2.0f);
                break;
            }

            case Icon::drain:
            {
                juce::Path funnel;
                funnel.startNewSubPath (area.getX() + 2.0f, area.getY() + 3.0f);
                funnel.lineTo (area.getRight() - 2.0f, area.getY() + 3.0f);
                funnel.lineTo (area.getCentreX() + 2.0f, area.getCentreY() + 2.0f);
                funnel.lineTo (area.getCentreX() + 2.0f, area.getBottom() - 3.0f);
                funnel.lineTo (area.getCentreX() - 2.0f, area.getBottom() - 3.0f);
                funnel.lineTo (area.getCentreX() - 2.0f, area.getCentreY() + 2.0f);
                funnel.closeSubPath();
                g.strokePath (funnel, juce::PathStrokeType (1.8f, juce::PathStrokeType::mitered,
                                                            juce::PathStrokeType::rounded));
                break;
            }

            case Icon::clone:
            {
                const auto centre = area.getCentre();
                juce::Path stem;
                stem.startNewSubPath (area.getX() + 2.0f, centre.y);
                stem.lineTo (centre.x - 2.0f, centre.y);
                stem.lineTo (centre.x + 4.0f, area.getY() + 4.0f);
                stem.startNewSubPath (centre.x - 2.0f, centre.y);
                stem.lineTo (centre.x + 4.0f, area.getBottom() - 4.0f);
                g.strokePath (stem, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
                for (const auto point : { juce::Point<float> (area.getX() + 2.0f, centre.y),
                                          juce::Point<float> (centre.x + 5.0f, area.getY() + 4.0f),
                                          juce::Point<float> (centre.x + 5.0f, area.getBottom() - 4.0f) })
                    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (point));
                break;
            }

            case Icon::speedLimit:
            {
                const auto sign = area.reduced (2.0f);
                g.drawEllipse (sign, 2.4f);
                g.setFont (juce::FontOptions (juce::jmax (9.0f, sign.getHeight() * 0.42f), juce::Font::bold));
                g.drawText ("x", sign, juce::Justification::centred, false);
                break;
            }

            case Icon::wait:
            {
                const auto glass = area.reduced (3.0f, 2.0f);
                g.drawLine ({ glass.getX(), glass.getY(), glass.getRight(), glass.getY() }, 2.2f);
                g.drawLine ({ glass.getX(), glass.getBottom(), glass.getRight(), glass.getBottom() }, 2.2f);
                juce::Path hourglass;
                hourglass.startNewSubPath (glass.getX() + 2.0f, glass.getY() + 1.5f);
                hourglass.lineTo (glass.getCentreX(), glass.getCentreY());
                hourglass.lineTo (glass.getX() + 2.0f, glass.getBottom() - 1.5f);
                hourglass.startNewSubPath (glass.getRight() - 2.0f, glass.getY() + 1.5f);
                hourglass.lineTo (glass.getCentreX(), glass.getCentreY());
                hourglass.lineTo (glass.getRight() - 2.0f, glass.getBottom() - 1.5f);
                g.strokePath (hourglass, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                               juce::PathStrokeType::rounded));
                break;
            }

            case Icon::strike:
            {
                const auto centre = area.getCentre();
                g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (centre));
                for (const auto end : { juce::Point<float> (area.getX() + 3.0f, area.getY() + 3.0f),
                                        juce::Point<float> (area.getRight() - 3.0f, area.getY() + 3.0f),
                                        juce::Point<float> (area.getRight() - 3.0f, area.getBottom() - 3.0f),
                                        juce::Point<float> (area.getX() + 3.0f, area.getBottom() - 3.0f) })
                {
                    g.drawLine ({ centre, end }, 2.2f);
                    g.fillEllipse (juce::Rectangle<float> (4.2f, 4.2f).withCentre (end));
                }
                break;
            }

            case Icon::teleport:
            {
                const auto left = area.withWidth (area.getWidth() * 0.42f).reduced (1.0f);
                const auto right = area.withTrimmedLeft (area.getWidth() * 0.58f).reduced (1.0f);
                g.drawEllipse (left, 2.0f); g.drawEllipse (right, 2.0f);
                g.drawLine ({ left.getRight() + 1.0f, area.getCentreY(), right.getX() - 1.0f, area.getCentreY() }, 2.0f);
                juce::Path arrow;
                arrow.startNewSubPath (right.getX() - 4.0f, area.getCentreY() - 3.0f);
                arrow.lineTo (right.getX(), area.getCentreY());
                arrow.lineTo (right.getX() - 4.0f, area.getCentreY() + 3.0f);
                g.strokePath (arrow, juce::PathStrokeType (1.8f));
                break;
            }

            case Icon::filter:
            {
                const auto box = juce::Rectangle<float> (area.getX() + 4.0f, area.getY() + 2.5f,
                                                         area.getWidth() - 8.0f, area.getHeight() - 5.0f);
                const auto y = box.getCentreY();
                g.drawLine ({ area.getX(), y, box.getX(), y }, 2.4f);
                g.drawLine ({ box.getRight(), y, area.getRight(), y }, 2.4f);
                g.drawRoundedRectangle (box, 1.8f, 2.0f);
                g.setFont (juce::FontOptions (juce::jmax (11.0f, box.getHeight() * 0.58f), juce::Font::bold));
                g.drawText ("F", box, juce::Justification::centred, false);
                break;
            }

            case Icon::logic:
            {
                const auto box = area.reduced (4.0f, 3.0f);
                const auto y = box.getCentreY();
                g.drawLine ({ area.getX(), y, box.getX(), y }, 2.2f);
                g.drawLine ({ box.getRight(), y, area.getRight(), y }, 2.2f);
                g.drawRoundedRectangle (box, 2.5f, 2.0f);
                g.setFont (juce::FontOptions (juce::jmax (8.0f, box.getHeight() * 0.40f), juce::Font::bold));
                g.drawText ("IF", box, juce::Justification::centred, false);
                break;
            }

            case Icon::modulator:
            {
                const auto ring = area.reduced (2.0f);
                g.drawEllipse (ring, 1.8f);
                juce::Path wave;
                const auto left = ring.getX() + 3.0f;
                const auto width = ring.getWidth() - 6.0f;
                wave.startNewSubPath (left, ring.getCentreY());
                for (int i = 1; i <= 18; ++i)
                {
                    const auto phase = static_cast<float> (i) / 18.0f;
                    wave.lineTo (left + width * phase,
                                 ring.getCentreY() - std::sin (phase * juce::MathConstants<float>::twoPi) * ring.getHeight() * 0.22f);
                }
                g.strokePath (wave, juce::PathStrokeType (1.9f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
                break;
            }

            case Icon::modConnect:
            {
                const auto start = juce::Point<float> (area.getX() + 3.0f, area.getBottom() - 4.0f);
                const auto end = juce::Point<float> (area.getRight() - 3.0f, area.getY() + 4.0f);
                g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (start));
                g.drawLine ({ start, end }, 2.2f);
                juce::Path arrow;
                arrow.startNewSubPath (end);
                arrow.lineTo (end + juce::Point<float> (-7.0f, 1.0f));
                arrow.lineTo (end + juce::Point<float> (-1.0f, 7.0f));
                arrow.closeSubPath();
                g.fillPath (arrow);
                break;
            }

            case Icon::fadeOrbits:
            {
                const auto outer = area.reduced (1.0f);
                const auto inner = area.reduced (6.3f);
                g.drawEllipse (outer, 1.7f);
                g.setColour ((getToggleState() ? appBackground() : textPrimary()).withAlpha (0.42f));
                g.drawEllipse (inner, 1.4f);
                g.fillEllipse (juce::Rectangle<float> (4.2f, 4.2f)
                                    .withCentre ({ outer.getRight() - 2.0f, outer.getCentreY() - 5.5f }));
                g.setColour (getToggleState() ? appBackground() : textPrimary().withAlpha (0.82f));
                g.drawLine ({ outer.getX() + 2.0f, outer.getBottom() - 2.0f,
                              outer.getRight() - 1.5f, outer.getY() + 1.5f }, 2.1f);
                break;
            }

            case Icon::addElement:
            {
                g.drawEllipse (area.reduced (2.0f), 2.0f);
                g.drawLine ({ area.getCentreX(), area.getY() + 5.0f, area.getCentreX(), area.getBottom() - 5.0f }, 2.8f);
                g.drawLine ({ area.getX() + 5.0f, area.getCentreY(), area.getRight() - 5.0f, area.getCentreY() }, 2.8f);
                break;
            }

            case Icon::nestedWorld:
            {
                g.drawEllipse (area.reduced (1.0f), 2.0f);
                g.drawEllipse (area.reduced (7.0f), 2.0f);
                g.drawLine ({ area.getCentreX(), area.getY() + 4.0f, area.getCentreX(), area.getBottom() - 4.0f }, 1.8f);
                break;
            }

            case Icon::exitWorld:
            {
                juce::Path arrow;
                arrow.startNewSubPath (area.getCentreX(), area.getY() + 2.0f);
                arrow.lineTo (area.getX() + 5.0f, area.getCentreY() - 4.0f);
                arrow.lineTo (area.getCentreX() - 3.0f, area.getCentreY() - 4.0f);
                arrow.lineTo (area.getCentreX() - 3.0f, area.getBottom() - 3.0f);
                arrow.lineTo (area.getCentreX() + 3.0f, area.getBottom() - 3.0f);
                arrow.lineTo (area.getCentreX() + 3.0f, area.getCentreY() - 4.0f);
                arrow.lineTo (area.getRight() - 5.0f, area.getCentreY() - 4.0f);
                arrow.closeSubPath();
                g.fillPath (arrow);
                break;
            }

            case Icon::erase:
            {
                juce::Path p;
                p.startNewSubPath (area.getX() + 3.0f, area.getBottom() - 6.0f);
                p.lineTo (area.getCentreX() + 1.0f, area.getY() + 1.0f);
                p.lineTo (area.getRight() - 1.0f, area.getCentreY() - 2.0f);
                p.lineTo (area.getCentreX() - 2.0f, area.getBottom() - 2.0f);
                p.closeSubPath();
                g.strokePath (p, juce::PathStrokeType (1.9f, juce::PathStrokeType::mitered,
                                                       juce::PathStrokeType::rounded));
                g.drawLine ({ area.getX() + 1.0f, area.getBottom() - 1.0f,
                              area.getRight() - 1.0f, area.getBottom() - 1.0f }, 1.6f);
                break;
            }

            case Icon::undo:
            {
                juce::Path p;
                p.startNewSubPath (area.getRight(), area.getBottom() - 4.0f);
                p.quadraticTo (area.getCentreX(), area.getY(), area.getX() + 5.0f, area.getCentreY());
                g.strokePath (p, juce::PathStrokeType (2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                juce::Path arrow;
                arrow.startNewSubPath (area.getX() + 5.0f, area.getCentreY());
                arrow.lineTo (area.getX() + 12.0f, area.getCentreY() - 6.0f);
                arrow.lineTo (area.getX() + 12.0f, area.getCentreY() + 6.0f);
                arrow.closeSubPath();
                g.fillPath (arrow);
                break;
            }

            case Icon::clear:
            {
                g.drawLine ({ area.getX() + 3.0f, area.getY() + 3.0f, area.getRight() - 3.0f, area.getBottom() - 3.0f }, 3.0f);
                g.drawLine ({ area.getRight() - 3.0f, area.getY() + 3.0f, area.getX() + 3.0f, area.getBottom() - 3.0f }, 3.0f);
                break;
            }

            case Icon::exportImage:
            {
                g.drawRoundedRectangle (area.reduced (2.0f), 2.0f, 2.0f);
                g.drawLine ({ area.getCentreX(), area.getY() + 5.0f, area.getCentreX(), area.getBottom() - 7.0f }, 2.4f);
                juce::Path arrow;
                arrow.startNewSubPath (area.getCentreX(), area.getBottom() - 4.0f);
                arrow.lineTo (area.getCentreX() - 5.0f, area.getBottom() - 10.0f);
                arrow.lineTo (area.getCentreX() + 5.0f, area.getBottom() - 10.0f);
                arrow.closeSubPath();
                g.fillPath (arrow);
                break;
            }
        }
    }
};

class MinimalLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    juce::Rectangle<int> getTooltipBounds (const juce::String& text,
                                           juce::Point<int> position,
                                           juce::Rectangle<int> parentArea) override
    {
        auto bounds = juce::LookAndFeel_V4::getTooltipBounds (text, position, parentArea);
        auto* hovered = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
        while (hovered != nullptr && dynamic_cast<IconButton*> (hovered) == nullptr)
            hovered = hovered->getParentComponent();

        if (hovered != nullptr)
        {
            const auto globalMouse = juce::Desktop::getMousePosition();
            const auto toolBounds = hovered->getScreenBounds().translated (position.x - globalMouse.x,
                                                                            position.y - globalMouse.y);
            bounds.setPosition (toolBounds.getRight() + 9,
                                toolBounds.getCentreY() - bounds.getHeight() / 2);
            bounds = bounds.constrainedWithin (parentArea);
        }
        return bounds;
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jlimit (12.0f, 13.0f, static_cast<float> (buttonHeight) * 0.38f),
                                              juce::Font::plain));
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int, int, int, int, juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<float> (0.5f, 0.5f, static_cast<float> (width) - 1.0f,
                                                     static_cast<float> (height) - 1.0f);
        auto fill = box.findColour (juce::ComboBox::backgroundColourId);
        if (isButtonDown)
            fill = fill.darker (0.08f);
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId).withAlpha (0.78f));
        g.drawRoundedRectangle (bounds, 6.0f, 0.8f);

        const auto centreX = static_cast<float> (width - 17);
        const auto centreY = static_cast<float> (height) * 0.5f;
        juce::Path chevron;
        chevron.startNewSubPath (centreX - 4.0f, centreY - 2.0f);
        chevron.lineTo (centreX, centreY + 2.0f);
        chevron.lineTo (centreX + 4.0f, centreY - 2.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 32, box.getHeight() - 2);
        label.setFont (juce::FontOptions (13.0f, juce::Font::plain));
    }

    void fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor) override
    {
        if (! static_cast<bool> (editor.getProperties().getWithDefault ("compactTransport", false)))
        {
            juce::LookAndFeel_V4::fillTextEditorBackground (g, width, height, editor);
            return;
        }

        g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle (juce::Rectangle<float> (0.5f, 0.5f, static_cast<float> (width) - 1.0f,
                                                         static_cast<float> (height) - 1.0f), 6.0f);
    }

    void drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor) override
    {
        if (! static_cast<bool> (editor.getProperties().getWithDefault ("compactTransport", false)))
        {
            juce::LookAndFeel_V4::drawTextEditorOutline (g, width, height, editor);
            return;
        }

        const auto colour = editor.hasKeyboardFocus (true)
                                ? editor.findColour (juce::TextEditor::focusedOutlineColourId)
                                : editor.findColour (juce::TextEditor::outlineColourId);
        g.setColour (colour.withAlpha (editor.hasKeyboardFocus (true) ? 1.0f : 0.78f));
        g.drawRoundedRectangle (juce::Rectangle<float> (0.5f, 0.5f, static_cast<float> (width) - 1.0f,
                                                         static_cast<float> (height) - 1.0f), 6.0f, 0.8f);
    }

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const auto enabled = button.isEnabled();
        const auto active = button.getToggleState();
        const auto activeColour = button.findColour (juce::TextButton::buttonOnColourId);
        auto fill = active ? activeColour : backgroundColour;

        if (shouldDrawButtonAsHighlighted && enabled)
            fill = fill.brighter (0.06f);

        if (shouldDrawButtonAsDown && enabled)
            fill = fill.darker (0.10f);

        g.setColour (enabled ? fill : fill.withAlpha (0.36f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour ((active ? activeColour.brighter (0.18f) : subtleStroke()).withAlpha (enabled ? 0.72f : 0.28f));
        g.drawRoundedRectangle (bounds, 6.0f, 0.8f);
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool,
                         bool) override
    {
        const auto enabled = button.isEnabled();
        const auto textColour = button.findColour (button.getToggleState()
                                                       ? juce::TextButton::textColourOnId
                                                       : juce::TextButton::textColourOffId)
                                   .withAlpha (enabled ? 1.0f : 0.38f);

        g.setColour (textColour);
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.drawFittedText (button.getButtonText(),
                          button.getLocalBounds().reduced (8, 1),
                          juce::Justification::centred,
                          1,
                          0.82f);
    }

    void drawToggleButton (juce::Graphics& g,
                           juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat();
        const auto box = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ bounds.getX() + 9.0f,
                                                                             bounds.getCentreY() });
        const auto enabled = button.isEnabled();
        auto fill = button.getToggleState() ? accentColour() : raisedSurface();

        if (shouldDrawButtonAsHighlighted && enabled)
            fill = fill.brighter (0.06f);

        if (shouldDrawButtonAsDown && enabled)
            fill = fill.darker (0.10f);

        g.setColour (fill.withAlpha (enabled ? 1.0f : 0.34f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (subtleStroke().withAlpha (enabled ? 0.85f : 0.35f));
        g.drawRoundedRectangle (box, 4.0f, 0.8f);

        if (button.getToggleState())
        {
            juce::Path tick;
            tick.startNewSubPath (box.getX() + 3.0f, box.getCentreY());
            tick.lineTo (box.getCentreX() - 1.0f, box.getBottom() - 4.0f);
            tick.lineTo (box.getRight() - 3.0f, box.getY() + 4.0f);
            g.setColour (appBackground());
            g.strokePath (tick, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour (textMuted().withAlpha (enabled ? 1.0f : 0.38f));
        g.setFont (juce::FontOptions (13.0f, juce::Font::plain));
        g.drawFittedText (button.getButtonText(),
                          button.getLocalBounds().withTrimmedLeft (22),
                          juce::Justification::centredLeft,
                          1);
    }
};

class TapSettingsComponent final : public juce::Component
{
public:
    TapSettingsComponent (PipeTap initial, juce::StringArray clockNames, std::function<void(const PipeTap&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 16.0f, true); title.setText ("Tap", juce::dontSendNotification); addAndMakeVisible (title);
        configureSectionLabel (timingSection, "TIMING");
        configureSectionLabel (speedSection, "SPEED");
        configureSectionLabel (outputSection, "OUTPUT");
        styleEditorLabel (clockLabel, 12.0f, false); clockLabel.setText ("Clock", juce::dontSendNotification); addAndMakeVisible (clockLabel);
        clock.addItemList (clockNames, 1);
        clock.setSelectedId (juce::jlimit (0, juce::jmax (0, clockNames.size() - 1), value.clockIndex) + 1, juce::dontSendNotification);
        clock.setTooltip ("Clock that schedules this tap"); addAndMakeVisible (clock);
        configureSlider (interval, intervalLabel, "Every", 0.25, 8.0, 0.25, value.intervalBeats, " beats");
        configureSlider (total, totalLabel, "Drop count", 0.0, 99.0, 1.0, value.totalDrops, "");
        total.setTooltip ("0 means no limit");
        randomInterval.setButtonText ("Vary interval");
        randomInterval.setToggleState (value.randomInterval, juce::dontSendNotification);
        randomInterval.setColour (juce::ToggleButton::textColourId, textPrimary()); addAndMakeVisible (randomInterval);
        configureSlider (low, lowLabel, "Minimum", 0.25, 32.0, 0.25, value.intervalLowBeats, " beats");
        configureSlider (high, highLabel, "Maximum", 0.25, 32.0, 0.25, value.intervalHighBeats, " beats");
        configureSlider (speed, speedLabel, "Multiplier", 0.25, 4.0, 0.05, value.speed, "x");
        randomSpeed.setButtonText ("Vary speed");
        randomSpeed.setToggleState (value.randomSpeed, juce::dontSendNotification);
        randomSpeed.setColour (juce::ToggleButton::textColourId, textPrimary()); addAndMakeVisible (randomSpeed);
        configureSlider (speedLow, speedLowLabel, "Minimum", 0.25, 4.0, 0.05, value.speedLow, "x");
        configureSlider (speedHigh, speedHighLabel, "Maximum", 0.25, 4.0, 0.05, value.speedHigh, "x");
        configureSlider (probability, probabilityLabel, "Emit chance", 0.0, 100.0, 1.0, value.probability * 100.0, "%");
        styleEditorLabel (directionLabel, 12.0f, false); directionLabel.setText ("Direction", juce::dontSendNotification); addAndMakeVisible (directionLabel);
        enabled.setButtonText ("Enabled"); enabled.setToggleState (value.enabled, juce::dontSendNotification); enabled.setColour (juce::ToggleButton::textColourId, textPrimary()); addAndMakeVisible (enabled);
        direction.addItem ("Along pipe", 1); direction.addItem ("Backwards", 2); direction.setSelectedId (value.reverse ? 2 : 1, juce::dontSendNotification); addAndMakeVisible (direction);
        enabled.onClick = [this] { value.enabled = enabled.getToggleState(); updateControlState(); notify(); };
        clock.onChange = [this] { value.clockIndex = juce::jmax (0, clock.getSelectedId() - 1); notify(); };
        direction.onChange = [this] { value.reverse = direction.getSelectedId() == 2; notify(); };
        interval.onValueChange = [this] { value.intervalBeats = interval.getValue(); notify(); };
        total.onValueChange = [this] { value.totalDrops = static_cast<int> (total.getValue()); notify(); };
        randomInterval.onClick = [this] { value.randomInterval = randomInterval.getToggleState(); updateControlState(); notify(); };
        low.onValueChange = [this]
        {
            value.intervalLowBeats = low.getValue();
            if (high.getValue() < low.getValue()) high.setValue (low.getValue(), juce::sendNotificationSync);
            notify();
        };
        high.onValueChange = [this]
        {
            value.intervalHighBeats = high.getValue();
            if (low.getValue() > high.getValue()) low.setValue (high.getValue(), juce::sendNotificationSync);
            notify();
        };
        speed.onValueChange = [this] { value.speed = speed.getValue(); notify(); };
        randomSpeed.onClick = [this] { value.randomSpeed = randomSpeed.getToggleState(); updateControlState(); notify(); };
        speedLow.onValueChange = [this]
        {
            value.speedLow = speedLow.getValue();
            if (speedHigh.getValue() < speedLow.getValue()) speedHigh.setValue (speedLow.getValue(), juce::sendNotificationSync);
            notify();
        };
        speedHigh.onValueChange = [this]
        {
            value.speedHigh = speedHigh.getValue();
            if (speedLow.getValue() > speedHigh.getValue()) speedLow.setValue (speedHigh.getValue(), juce::sendNotificationSync);
            notify();
        };
        probability.onValueChange = [this] { value.probability = probability.getValue() / 100.0; notify(); };
        updateControlState();
        setSize (360, 609);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (surfaceColour());
        g.setColour (subtleStroke().withAlpha (0.55f));
        g.drawHorizontalLine (timingSection.getBottom() + 2, 14.0f, static_cast<float> (getWidth() - 14));
        g.drawHorizontalLine (speedSection.getBottom() + 2, 14.0f, static_cast<float> (getWidth() - 14));
        g.drawHorizontalLine (outputSection.getBottom() + 2, 14.0f, static_cast<float> (getWidth() - 14));
    }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 10);
        auto heading = area.removeFromTop (28);
        title.setBounds (heading.removeFromLeft (170));
        enabled.setBounds (heading);
        area.removeFromTop (5);
        timingSection.setBounds (area.removeFromTop (24));
        area.removeFromTop (4);
        auto clockRow = area.removeFromTop (39);
        clockLabel.setBounds (clockRow.removeFromLeft (112));
        clock.setBounds (clockRow.reduced (0, 3));
        layoutRow (area, intervalLabel, interval);
        layoutRow (area, totalLabel, total);
        randomInterval.setBounds (area.removeFromTop (30).withTrimmedLeft (112));
        layoutRow (area, lowLabel, low);
        layoutRow (area, highLabel, high);
        area.removeFromTop (5);
        speedSection.setBounds (area.removeFromTop (24));
        area.removeFromTop (4);
        layoutRow (area, speedLabel, speed);
        randomSpeed.setBounds (area.removeFromTop (30).withTrimmedLeft (112));
        layoutRow (area, speedLowLabel, speedLow);
        layoutRow (area, speedHighLabel, speedHigh);
        area.removeFromTop (5);
        outputSection.setBounds (area.removeFromTop (24));
        area.removeFromTop (4);
        layoutRow (area, probabilityLabel, probability);
        directionLabel.setBounds (area.removeFromLeft (112).removeFromTop (32));
        direction.setBounds (area.removeFromTop (32));
    }

private:
    PipeTap value;
    std::function<void(const PipeTap&)> onChange;
    juce::Label title, timingSection, speedSection, outputSection, clockLabel, intervalLabel, totalLabel, lowLabel, highLabel, speedLabel, speedLowLabel, speedHighLabel, probabilityLabel, directionLabel;
    juce::Slider interval, total, low, high, speed, speedLow, speedHigh, probability;
    juce::ToggleButton enabled, randomInterval, randomSpeed;
    juce::ComboBox clock, direction;

    void configureSectionLabel (juce::Label& label, const juce::String& text)
    {
        styleEditorLabel (label, 10.5f, true);
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, pipeElementColour().withAlpha (0.88f));
        addAndMakeVisible (label);
    }

    void configureSlider (juce::Slider& slider, juce::Label& label, const juce::String& name, double min, double max, double step, double current, const juce::String& suffix)
    {
        styleEditorLabel (label, 12.0f, false); label.setText (name, juce::dontSendNotification); addAndMakeVisible (label);
        slider.setName (name); slider.setRange (min, max, step); slider.setValue (current, juce::dontSendNotification); slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 76, 26); slider.setTextValueSuffix (suffix); slider.setColour (juce::Slider::thumbColourId, pipeElementColour());
        slider.setTooltip (name); addAndMakeVisible (slider);
    }
    static void layoutRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
    {
        auto row = area.removeFromTop (39);
        label.setBounds (row.removeFromLeft (112));
        slider.setBounds (row);
    }
    void updateControlState()
    {
        interval.setEnabled (value.enabled && ! value.randomInterval); total.setEnabled (value.enabled);
        randomInterval.setEnabled (value.enabled); low.setEnabled (value.enabled && value.randomInterval); high.setEnabled (value.enabled && value.randomInterval);
        speed.setEnabled (value.enabled && ! value.randomSpeed); randomSpeed.setEnabled (value.enabled);
        speedLow.setEnabled (value.enabled && value.randomSpeed); speedHigh.setEnabled (value.enabled && value.randomSpeed);
        clock.setEnabled (value.enabled); probability.setEnabled (value.enabled); direction.setEnabled (value.enabled);
        intervalLabel.setEnabled (value.enabled && ! value.randomInterval); totalLabel.setEnabled (value.enabled);
        lowLabel.setEnabled (value.enabled && value.randomInterval); highLabel.setEnabled (value.enabled && value.randomInterval);
        speedLabel.setEnabled (value.enabled && ! value.randomSpeed);
        speedLowLabel.setEnabled (value.enabled && value.randomSpeed); speedHighLabel.setEnabled (value.enabled && value.randomSpeed);
        clockLabel.setEnabled (value.enabled); probabilityLabel.setEnabled (value.enabled); directionLabel.setEnabled (value.enabled);
    }
    void notify() { if (onChange) onChange (value); }
};

class DrainSettingsComponent final : public juce::Component
{
public:
    DrainSettingsComponent (PipeDrain initial, std::function<void(const PipeDrain&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText ("Drain", juce::dontSendNotification);
        addAndMakeVisible (title);
        styleEditorLabel (chanceLabel, 12.0f, false);
        chanceLabel.setText ("Destroy passing drops", juce::dontSendNotification);
        addAndMakeVisible (chanceLabel);
        chance.setRange (0.0, 100.0, 1.0);
        chance.setValue (value.destructionProbability * 100.0, juce::dontSendNotification);
        chance.setSliderStyle (juce::Slider::LinearHorizontal);
        chance.setTextBoxStyle (juce::Slider::TextBoxRight, false, 66, 26);
        chance.setTextValueSuffix ("%");
        chance.setColour (juce::Slider::thumbColourId, pdPatchElementColour());
        chance.onValueChange = [this]
        {
            value.destructionProbability = chance.getValue() / 100.0;
            if (onChange) onChange (value);
        };
        addAndMakeVisible (chance);
        setSize (310, 112);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (25));
        area.removeFromTop (8);
        chanceLabel.setBounds (area.removeFromTop (18));
        chance.setBounds (area.removeFromTop (34));
    }

private:
    PipeDrain value;
    std::function<void(const PipeDrain&)> onChange;
    juce::Label title, chanceLabel;
    juce::Slider chance;
};

class ClonerSettingsComponent final : public juce::Component
{
public:
    ClonerSettingsComponent (PipeCloner initial, std::function<void(const PipeCloner&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText ("Quantum directions", juce::dontSendNotification);
        addAndMakeVisible (title);
        configureRow (straightLabel, straight, "Straight pipe", value.straightMode);
        configureRow (twoLabel, two, "2-way junction", value.twoWayMode);
        configureRow (threeLabel, three, "3-way junction", value.threeWayMode);
        configureRow (fourLabel, four, "4-way junction", value.fourWayMode);
        configureSlider (maxClonesLabel, maxClones, "Maximum clones", 1.0, 16.0, 1.0, value.maxClones, "");
        configureSlider (probabilityLabel, probability, "Chance per clone", 0.0, 100.0, 1.0,
                         value.cloneProbability * 100.0, "%");
        straight.onChange = [this] { value.straightMode = selectedMode (straight); notify(); };
        two.onChange = [this] { value.twoWayMode = selectedMode (two); notify(); };
        three.onChange = [this] { value.threeWayMode = selectedMode (three); notify(); };
        four.onChange = [this] { value.fourWayMode = selectedMode (four); notify(); };
        maxClones.onValueChange = [this] { value.maxClones = static_cast<int> (maxClones.getValue()); notify(); };
        probability.onValueChange = [this] { value.cloneProbability = probability.getValue() / 100.0; notify(); };
        setSize (340, 318);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (27));
        area.removeFromTop (6);
        layoutRow (area, straightLabel, straight);
        layoutRow (area, twoLabel, two);
        layoutRow (area, threeLabel, three);
        layoutRow (area, fourLabel, four);
        layoutSliderRow (area, maxClonesLabel, maxClones);
        layoutSliderRow (area, probabilityLabel, probability);
    }

private:
    PipeCloner value;
    std::function<void(const PipeCloner&)> onChange;
    juce::Label title, straightLabel, twoLabel, threeLabel, fourLabel, maxClonesLabel, probabilityLabel;
    juce::ComboBox straight, two, three, four;
    juce::Slider maxClones, probability;

    void configureRow (juce::Label& label, juce::ComboBox& box, const juce::String& text,
                       PipeCloner::DirectionMode mode)
    {
        styleEditorLabel (label, 12.0f, false);
        label.setText (text, juce::dontSendNotification);
        addAndMakeVisible (label);
        box.addItemList ({ "All directions", "Random direction", "Straightest", "Leftmost", "Rightmost" }, 1);
        box.setSelectedId (static_cast<int> (mode) + 1, juce::dontSendNotification);
        addAndMakeVisible (box);
    }

    static PipeCloner::DirectionMode selectedMode (const juce::ComboBox& box)
    {
        return static_cast<PipeCloner::DirectionMode> (juce::jlimit (0, 4, box.getSelectedId() - 1));
    }

    void configureSlider (juce::Label& label, juce::Slider& slider, const juce::String& text,
                          double min, double max, double step, double current, const juce::String& suffix)
    {
        styleEditorLabel (label, 12.0f, false);
        label.setText (text, juce::dontSendNotification);
        addAndMakeVisible (label);
        slider.setRange (min, max, step);
        slider.setValue (current, juce::dontSendNotification);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 26);
        slider.setTextValueSuffix (suffix);
        slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffb888ff));
        addAndMakeVisible (slider);
    }

    static void layoutRow (juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& box)
    {
        auto row = area.removeFromTop (43);
        label.setBounds (row.removeFromLeft (124));
        box.setBounds (row.reduced (0, 5));
    }

    static void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
    {
        auto row = area.removeFromTop (43);
        label.setBounds (row.removeFromLeft (124));
        slider.setBounds (row);
    }

    void notify() { if (onChange) onChange (value); }
};

class SpeedLimitSettingsComponent final : public juce::Component
{
public:
    SpeedLimitSettingsComponent (PipeSpeedLimit initial, std::function<void(const PipeSpeedLimit&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText ("Speed limit", juce::dontSendNotification);
        addAndMakeVisible (title);
        styleEditorLabel (speedLabel, 12.0f, false);
        speedLabel.setText ("Relative to global BPM", juce::dontSendNotification);
        addAndMakeVisible (speedLabel);
        speed.setRange (0.125, 4.0, 0.125);
        speed.setValue (value.bpmMultiplier, juce::dontSendNotification);
        speed.setSliderStyle (juce::Slider::LinearHorizontal);
        speed.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 26);
        speed.setTextValueSuffix ("x");
        speed.setColour (juce::Slider::thumbColourId, juce::Colour (0xffffb84d));
        speed.onValueChange = [this]
        {
            value.bpmMultiplier = speed.getValue();
            if (onChange) onChange (value);
        };
        addAndMakeVisible (speed);
        styleEditorLabel (chanceLabel, 12.0f, false);
        chanceLabel.setText ("Affect chance", juce::dontSendNotification);
        addAndMakeVisible (chanceLabel);
        chance.setRange (0.0, 100.0, 1.0);
        chance.setValue (value.affectProbability * 100.0, juce::dontSendNotification);
        chance.setSliderStyle (juce::Slider::LinearHorizontal);
        chance.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 26);
        chance.setTextValueSuffix ("%");
        chance.setColour (juce::Slider::thumbColourId, juce::Colour (0xffffb84d));
        chance.onValueChange = [this]
        {
            value.affectProbability = chance.getValue() / 100.0;
            if (onChange) onChange (value);
        };
        addAndMakeVisible (chance);
        setSize (310, 170);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (25));
        area.removeFromTop (8);
        speedLabel.setBounds (area.removeFromTop (18));
        speed.setBounds (area.removeFromTop (34));
        area.removeFromTop (7);
        chanceLabel.setBounds (area.removeFromTop (18));
        chance.setBounds (area.removeFromTop (34));
    }

private:
    PipeSpeedLimit value;
    std::function<void(const PipeSpeedLimit&)> onChange;
    juce::Label title, speedLabel, chanceLabel;
    juce::Slider speed, chance;
};

class WaitSettingsComponent final : public juce::Component
{
public:
    WaitSettingsComponent (PipeWait initial, std::function<void(const PipeWait&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText ("Wait", juce::dontSendNotification);
        addAndMakeVisible (title);
        styleEditorLabel (beatsLabel, 12.0f, false);
        beatsLabel.setText ("Hold passing drops", juce::dontSendNotification);
        addAndMakeVisible (beatsLabel);
        beats.setRange (0.25, 32.0, 0.25);
        beats.setValue (value.beats, juce::dontSendNotification);
        beats.setSliderStyle (juce::Slider::LinearHorizontal);
        beats.setTextBoxStyle (juce::Slider::TextBoxRight, false, 76, 26);
        beats.setTextValueSuffix (" beats");
        beats.setColour (juce::Slider::thumbColourId, juce::Colour (0xffffd166));
        beats.onValueChange = [this]
        {
            value.beats = beats.getValue();
            if (onChange) onChange (value);
        };
        addAndMakeVisible (beats);
        setSize (310, 112);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (25));
        area.removeFromTop (8);
        beatsLabel.setBounds (area.removeFromTop (18));
        beats.setBounds (area.removeFromTop (34));
    }

private:
    PipeWait value;
    std::function<void(const PipeWait&)> onChange;
    juce::Label title, beatsLabel;
    juce::Slider beats;
};

class StrikeSettingsComponent final : public juce::Component
{
public:
    StrikeSettingsComponent (PipeStrike initial, std::function<void(const PipeStrike&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true); title.setText ("Strike", juce::dontSendNotification); addAndMakeVisible (title);
        styleEditorLabel (countLabel, 12.0f, false); countLabel.setText ("Maximum discs", juce::dontSendNotification); addAndMakeVisible (countLabel);
        count.setRange (1, 4, 1); count.setValue (value.maxDiscs, juce::dontSendNotification);
        count.setSliderStyle (juce::Slider::LinearHorizontal); count.setTextBoxStyle (juce::Slider::TextBoxRight, false, 54, 26);
        count.setColour (juce::Slider::thumbColourId, juce::Colour (0xffff6b8a)); addAndMakeVisible (count);
        configureToggle (left, "NW", value.left); configureToggle (right, "NE", value.right);
        configureToggle (up, "SW", value.up); configureToggle (down, "SE", value.down);
        count.onValueChange = [this] { value.maxDiscs = static_cast<int> (count.getValue()); notify(); };
        left.onClick = [this] { value.left = left.getToggleState(); notify(); };
        right.onClick = [this] { value.right = right.getToggleState(); notify(); };
        up.onClick = [this] { value.up = up.getToggleState(); notify(); };
        down.onClick = [this] { value.down = down.getToggleState(); notify(); };
        setSize (310, 160);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (27)); area.removeFromTop (5);
        auto countRow = area.removeFromTop (38); countLabel.setBounds (countRow.removeFromLeft (110)); count.setBounds (countRow);
        area.removeFromTop (7);
        auto directions = area.removeFromTop (30);
        const auto width = directions.getWidth() / 4;
        left.setBounds (directions.removeFromLeft (width)); right.setBounds (directions.removeFromLeft (width));
        up.setBounds (directions.removeFromLeft (width)); down.setBounds (directions);
    }

private:
    PipeStrike value;
    std::function<void(const PipeStrike&)> onChange;
    juce::Label title, countLabel;
    juce::Slider count;
    juce::ToggleButton left, right, up, down;
    void configureToggle (juce::ToggleButton& button, const juce::String& text, bool enabled)
    {
        button.setButtonText (text); button.setToggleState (enabled, juce::dontSendNotification);
        button.setColour (juce::ToggleButton::textColourId, textPrimary()); addAndMakeVisible (button);
    }
    void notify() { if (onChange) onChange (value); }
};

class TeleportSettingsComponent final : public juce::Component
{
public:
    TeleportSettingsComponent (PipeTeleport initial, juce::StringArray destinationNames,
                               juce::StringArray destinationIds,
                               std::function<void(const PipeTeleport&)> changed)
        : value (std::move (initial)), ids (std::move (destinationIds)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true); title.setText ("Teleport", juce::dontSendNotification); addAndMakeVisible (title);
        styleEditorLabel (destinationLabel, 12.0f, false); destinationLabel.setText ("Destination", juce::dontSendNotification); addAndMakeVisible (destinationLabel);
        destination.addItem ("Any (random)", 1); destination.addItemList (destinationNames, 2);
        auto selected = 1; for (int i = 0; i < ids.size(); ++i) if (ids[i] == value.destinationId) selected = i + 2;
        destination.setSelectedId (selected, juce::dontSendNotification); addAndMakeVisible (destination);
        configureSlider (chanceLabel, chance, "Teleport chance", 0, 100, 1, value.probability * 100.0, "%");
        configureSlider (limitLabel, limit, "Maximum teleports", 1, 64, 1, value.maxPerWindow, "");
        configureSlider (barsLabel, bars, "Per window", 0.25, 16, 0.25, value.windowBars, " bars");
        configureSlider (stopLabel, stop, "Stop after", 0, 1000, 1, value.stopAfter, " drops");
        destination.onChange = [this]
        {
            const auto index = destination.getSelectedId() - 2;
            value.destinationId = juce::isPositiveAndBelow (index, ids.size()) ? ids[index] : juce::String(); notify();
        };
        chance.onValueChange = [this] { value.probability = chance.getValue() / 100.0; notify(); };
        limit.onValueChange = [this] { value.maxPerWindow = static_cast<int> (limit.getValue()); notify(); };
        bars.onValueChange = [this] { value.windowBars = bars.getValue(); notify(); };
        stop.onValueChange = [this] { value.stopAfter = static_cast<int> (stop.getValue()); notify(); };
        setSize (360, 270);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11); title.setBounds (area.removeFromTop (27)); area.removeFromTop (5);
        auto destinationRow = area.removeFromTop (40); destinationLabel.setBounds (destinationRow.removeFromLeft (124)); destination.setBounds (destinationRow.reduced (0, 4));
        layoutRow (area, chanceLabel, chance); layoutRow (area, limitLabel, limit);
        layoutRow (area, barsLabel, bars); layoutRow (area, stopLabel, stop);
    }

private:
    PipeTeleport value; juce::StringArray ids; std::function<void(const PipeTeleport&)> onChange;
    juce::Label title, destinationLabel, chanceLabel, limitLabel, barsLabel, stopLabel;
    juce::ComboBox destination; juce::Slider chance, limit, bars, stop;
    void configureSlider (juce::Label& label, juce::Slider& slider, const juce::String& text,
                          double min, double max, double step, double current, const juce::String& suffix)
    {
        styleEditorLabel (label, 12.0f, false); label.setText (text, juce::dontSendNotification); addAndMakeVisible (label);
        slider.setRange (min, max, step); slider.setValue (current, juce::dontSendNotification);
        slider.setSliderStyle (juce::Slider::LinearHorizontal); slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 82, 26);
        slider.setTextValueSuffix (suffix); slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff53d8fb)); addAndMakeVisible (slider);
    }
    static void layoutRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
    { auto row = area.removeFromTop (42); label.setBounds (row.removeFromLeft (124)); slider.setBounds (row); }
    void notify() { if (onChange) onChange (value); }
};

class FilterSettingsComponent final : public juce::Component
{
public:
    FilterSettingsComponent (PipeFilter initial, std::function<void(const PipeFilter&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true); title.setText ("Speed filter", juce::dontSendNotification); addAndMakeVisible (title);
        styleEditorLabel (modeLabel, 12.0f, false); modeLabel.setText ("Mode", juce::dontSendNotification); addAndMakeVisible (modeLabel);
        mode.addItemList ({ "Highpass", "Lowpass", "Bandpass" }, 1); mode.setSelectedId (static_cast<int> (value.mode) + 1, juce::dontSendNotification); addAndMakeVisible (mode);
        configureSlider (lowLabel, low, "Low cutoff", value.lowSpeed);
        configureSlider (highLabel, high, "High cutoff", value.highSpeed);
        mode.onChange = [this] { value.mode = static_cast<PipeFilter::Mode> (juce::jlimit (0, 2, mode.getSelectedId() - 1)); updateState(); notify(); };
        low.onValueChange = [this] { value.lowSpeed = low.getValue(); if (high.getValue() < low.getValue()) high.setValue (low.getValue(), juce::sendNotificationSync); notify(); };
        high.onValueChange = [this] { value.highSpeed = high.getValue(); if (low.getValue() > high.getValue()) low.setValue (high.getValue(), juce::sendNotificationSync); notify(); };
        updateState(); setSize (330, 184);
    }
    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }
    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11); title.setBounds (area.removeFromTop (27)); area.removeFromTop (5);
        auto row = area.removeFromTop (38); modeLabel.setBounds (row.removeFromLeft (105)); mode.setBounds (row.reduced (0, 3));
        layoutRow (area, lowLabel, low); layoutRow (area, highLabel, high);
    }
private:
    PipeFilter value; std::function<void(const PipeFilter&)> onChange;
    juce::Label title, modeLabel, lowLabel, highLabel; juce::ComboBox mode; juce::Slider low, high;
    void configureSlider (juce::Label& label, juce::Slider& slider, const juce::String& text, double current)
    {
        styleEditorLabel (label, 12.0f, false); label.setText (text, juce::dontSendNotification); addAndMakeVisible (label);
        slider.setRange (0.125, 4.0, 0.125); slider.setValue (current, juce::dontSendNotification);
        slider.setSliderStyle (juce::Slider::LinearHorizontal); slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 26);
        slider.setTextValueSuffix ("x"); slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff7dd3fc)); addAndMakeVisible (slider);
    }
    static void layoutRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
    { auto row = area.removeFromTop (42); label.setBounds (row.removeFromLeft (105)); slider.setBounds (row); }
    void updateState()
    {
        low.setEnabled (value.mode != PipeFilter::Mode::lowpass); lowLabel.setEnabled (low.isEnabled());
        high.setEnabled (value.mode != PipeFilter::Mode::highpass); highLabel.setEnabled (high.isEnabled());
    }
    void notify() { if (onChange) onChange (value); }
};

class LogicSettingsComponent final : public juce::Component
{
public:
    LogicSettingsComponent (PipeLogic initial, std::function<void(const PipeLogic&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText ("Logic", juce::dontSendNotification);
        addAndMakeVisible (title);

        styleEditorLabel (modeLabel, 12.0f, false);
        modeLabel.setText ("Rule", juce::dontSendNotification);
        addAndMakeVisible (modeLabel);
        mode.addItemList ({ "Gate", "Count to open", "Switch", "Speed comparison", "Flip-flop", "Every Nth drop" }, 1);
        mode.setSelectedId (static_cast<int> (value.mode) + 1, juce::dontSendNotification);
        addAndMakeVisible (mode);

        gateOpen.setButtonText ("Gate open");
        gateOpen.setToggleState (value.gateOpen, juce::dontSendNotification);
        gateOpen.setColour (juce::ToggleButton::textColourId, textPrimary());
        addAndMakeVisible (gateOpen);

        styleEditorLabel (targetLabel, 12.0f, false);
        addAndMakeVisible (targetLabel);
        target.setRange (1, 64, 1);
        target.setValue (value.targetCount, juce::dontSendNotification);
        target.setSliderStyle (juce::Slider::LinearHorizontal);
        target.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 26);
        target.setColour (juce::Slider::thumbColourId, juce::Colour (0xffffd166));
        addAndMakeVisible (target);

        styleEditorLabel (branchLabel, 12.0f, false);
        branchLabel.setText ("Send towards", juce::dontSendNotification);
        addAndMakeVisible (branchLabel);
        branch.addItemList ({ "Left", "Straight", "Right", "Random" }, 1);
        branch.setSelectedId (static_cast<int> (value.branch) + 1, juce::dontSendNotification);
        addAndMakeVisible (branch);

        styleEditorLabel (comparisonLabel, 12.0f, false);
        comparisonLabel.setText ("Comparison", juce::dontSendNotification);
        addAndMakeVisible (comparisonLabel);
        comparison.addItemList ({ "Less than", "Less or equal", "Equal", "Greater or equal", "Greater than" }, 1);
        comparison.setSelectedId (static_cast<int> (value.comparison) + 1, juce::dontSendNotification);
        addAndMakeVisible (comparison);

        styleEditorLabel (speedLabel, 12.0f, false);
        speedLabel.setText ("Drop speed", juce::dontSendNotification);
        addAndMakeVisible (speedLabel);
        speed.setRange (0.125, 4.0, 0.125);
        speed.setValue (value.compareSpeed, juce::dontSendNotification);
        speed.setSliderStyle (juce::Slider::LinearHorizontal);
        speed.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 26);
        speed.setTextValueSuffix ("x");
        speed.setColour (juce::Slider::thumbColourId, juce::Colour (0xffffd166));
        addAndMakeVisible (speed);

        styleEditorLabel (explanation, 11.0f, false);
        explanation.setColour (juce::Label::textColourId, textMuted());
        explanation.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (explanation);

        mode.onChange = [this]
        {
            value.mode = static_cast<PipeLogic::Mode> (juce::jlimit (0, 5, mode.getSelectedId() - 1));
            updateState();
            notify();
        };
        gateOpen.onClick = [this] { value.gateOpen = gateOpen.getToggleState(); notify(); };
        target.onValueChange = [this] { value.targetCount = static_cast<int> (target.getValue()); notify(); };
        branch.onChange = [this] { value.branch = static_cast<PipeLogic::Branch> (juce::jlimit (0, 3, branch.getSelectedId() - 1)); notify(); };
        comparison.onChange = [this] { value.comparison = static_cast<PipeLogic::Comparison> (juce::jlimit (0, 4, comparison.getSelectedId() - 1)); notify(); };
        speed.onValueChange = [this] { value.compareSpeed = speed.getValue(); notify(); };

        setSize (350, 170);
        updateState();
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (27));
        area.removeFromTop (5);
        auto modeRow = area.removeFromTop (38);
        modeLabel.setBounds (modeRow.removeFromLeft (105));
        mode.setBounds (modeRow.reduced (0, 3));
        area.removeFromTop (5);

        if (gateOpen.isVisible()) gateOpen.setBounds (area.removeFromTop (34));
        if (target.isVisible())
        {
            auto row = area.removeFromTop (38);
            targetLabel.setBounds (row.removeFromLeft (105));
            target.setBounds (row);
        }
        if (branch.isVisible())
        {
            auto row = area.removeFromTop (38);
            branchLabel.setBounds (row.removeFromLeft (105));
            branch.setBounds (row.reduced (0, 3));
        }
        if (comparison.isVisible())
        {
            auto row = area.removeFromTop (38);
            comparisonLabel.setBounds (row.removeFromLeft (105));
            comparison.setBounds (row.reduced (0, 3));
            row = area.removeFromTop (38);
            speedLabel.setBounds (row.removeFromLeft (105));
            speed.setBounds (row);
        }
        area.removeFromTop (3);
        explanation.setBounds (area.removeFromTop (38));
    }

private:
    PipeLogic value;
    std::function<void(const PipeLogic&)> onChange;
    juce::Label title, modeLabel, targetLabel, branchLabel, comparisonLabel, speedLabel, explanation;
    juce::ComboBox mode, branch, comparison;
    juce::ToggleButton gateOpen;
    juce::Slider target, speed;

    void updateState()
    {
        const auto isGate = value.mode == PipeLogic::Mode::gate;
        const auto isCount = value.mode == PipeLogic::Mode::counter || value.mode == PipeLogic::Mode::everyNth;
        const auto isSwitch = value.mode == PipeLogic::Mode::switcher;
        const auto isComparator = value.mode == PipeLogic::Mode::comparator;

        gateOpen.setVisible (isGate);
        targetLabel.setVisible (isCount); target.setVisible (isCount);
        branchLabel.setVisible (isSwitch); branch.setVisible (isSwitch);
        comparisonLabel.setVisible (isComparator); comparison.setVisible (isComparator);
        speedLabel.setVisible (isComparator); speed.setVisible (isComparator);
        targetLabel.setText (value.mode == PipeLogic::Mode::counter ? "Open after" : "Pass every", juce::dontSendNotification);

        if (isGate) explanation.setText ("Closed gates remove passing drops.", juce::dontSendNotification);
        else if (value.mode == PipeLogic::Mode::counter) explanation.setText ("Blocks drops until the target count, then remains open.", juce::dontSendNotification);
        else if (isSwitch) explanation.setText ("Chooses an exit at a pipe junction.", juce::dontSendNotification);
        else if (isComparator) explanation.setText ("Only drops matching the speed comparison pass.", juce::dontSendNotification);
        else if (value.mode == PipeLogic::Mode::flipFlop) explanation.setText ("Alternates junction exits, or pass and block on a straight pipe.", juce::dontSendNotification);
        else explanation.setText ("Only each selected numbered drop passes.", juce::dontSendNotification);

        setSize (350, isComparator ? 220 : 180);
        resized();
        repaint();
    }

    void notify() { if (onChange) onChange (value); }
};

class ClockSettingsComponent final : public juce::Component
{
public:
    using ClockBank = std::array<SequencingClock, 4>;

    ClockSettingsComponent (ClockBank initial, std::function<void(const ClockBank&)> changed)
        : clocks (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText ("Clocks", juce::dontSendNotification);
        addAndMakeVisible (title);

        styleEditorLabel (clockLabel, 12.0f, false);
        clockLabel.setText ("Auxiliary clock", juce::dontSendNotification);
        addAndMakeVisible (clockLabel);
        addAndMakeVisible (clockChoice);
        rebuildClockChoices();

        enabled.setButtonText ("Enabled");
        enabled.setColour (juce::ToggleButton::textColourId, textPrimary());
        addAndMakeVisible (enabled);

        styleEditorLabel (nameLabel, 12.0f, false);
        nameLabel.setText ("Name", juce::dontSendNotification);
        addAndMakeVisible (nameLabel);
        name.setColour (juce::TextEditor::backgroundColourId, raisedSurface());
        name.setColour (juce::TextEditor::textColourId, textPrimary());
        name.setColour (juce::TextEditor::outlineColourId, subtleStroke());
        name.setColour (juce::TextEditor::focusedOutlineColourId, accentColour());
        name.setIndents (8, 4);
        name.setSelectAllWhenFocused (true);
        addAndMakeVisible (name);

        configureSlider (ratioLabel, ratio, "Rate", 0.125, 8.0, 0.125, "x");
        configureSlider (phaseLabel, phase, "Phase", 0.0, 16.0, 0.125, " beats");
        configureSlider (swingLabel, swing, "Swing", 0.0, 75.0, 1.0, "%");

        clockChoice.onChange = [this] { selected = juce::jlimit (0, 3, clockChoice.getSelectedId() - 1); loadSelected(); };
        enabled.onClick = [this] { if (! updating) { clocks[static_cast<size_t> (selected)].enabled = enabled.getToggleState(); controlsChanged(); } };
        name.onFocusLost = [this] { commitName(); };
        name.onReturnKey = [this] { commitName(); name.giveAwayKeyboardFocus(); };
        ratio.onValueChange = [this] { if (! updating) { clocks[static_cast<size_t> (selected)].ratio = ratio.getValue(); controlsChanged(); } };
        phase.onValueChange = [this] { if (! updating) { clocks[static_cast<size_t> (selected)].phaseBeats = phase.getValue(); controlsChanged(); } };
        swing.onValueChange = [this] { if (! updating) { clocks[static_cast<size_t> (selected)].swing = swing.getValue() / 100.0; controlsChanged(); } };

        clockChoice.setSelectedId (1, juce::dontSendNotification);
        loadSelected();
        setSize (380, 276);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (27));
        area.removeFromTop (5);
        auto row = area.removeFromTop (38);
        clockLabel.setBounds (row.removeFromLeft (112));
        clockChoice.setBounds (row.reduced (0, 3));
        area.removeFromTop (2);
        enabled.setBounds (area.removeFromTop (30).withTrimmedLeft (112));
        row = area.removeFromTop (38);
        nameLabel.setBounds (row.removeFromLeft (112));
        name.setBounds (row.reduced (0, 3));
        layoutSliderRow (area, ratioLabel, ratio);
        layoutSliderRow (area, phaseLabel, phase);
        layoutSliderRow (area, swingLabel, swing);
    }

private:
    ClockBank clocks;
    std::function<void(const ClockBank&)> onChange;
    int selected = 0;
    bool updating = false;
    juce::Label title, clockLabel, nameLabel, ratioLabel, phaseLabel, swingLabel;
    juce::ComboBox clockChoice;
    juce::ToggleButton enabled;
    juce::TextEditor name;
    juce::Slider ratio, phase, swing;

    void configureSlider (juce::Label& label, juce::Slider& slider, const juce::String& text,
                          double min, double max, double step, const juce::String& suffix)
    {
        styleEditorLabel (label, 12.0f, false);
        label.setText (text, juce::dontSendNotification);
        addAndMakeVisible (label);
        slider.setRange (min, max, step);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 26);
        slider.setTextValueSuffix (suffix);
        slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff53d8fb));
        addAndMakeVisible (slider);
    }

    static void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
    {
        auto row = area.removeFromTop (39);
        label.setBounds (row.removeFromLeft (112));
        slider.setBounds (row);
    }

    void rebuildClockChoices()
    {
        const auto oldSelection = juce::jmax (1, clockChoice.getSelectedId());
        clockChoice.clear (juce::dontSendNotification);
        for (int i = 0; i < static_cast<int> (clocks.size()); ++i)
        {
            auto label = juce::String (i + 1) + "  " + clocks[static_cast<size_t> (i)].name;
            clockChoice.addItem (label, i + 1);
        }
        clockChoice.setSelectedId (juce::jlimit (1, 4, oldSelection), juce::dontSendNotification);
    }

    void loadSelected()
    {
        const juce::ScopedValueSetter<bool> guard (updating, true);
        const auto& clock = clocks[static_cast<size_t> (selected)];
        enabled.setToggleState (clock.enabled, juce::dontSendNotification);
        name.setText (clock.name, false);
        ratio.setValue (clock.ratio, juce::dontSendNotification);
        phase.setValue (clock.phaseBeats, juce::dontSendNotification);
        swing.setValue (clock.swing * 100.0, juce::dontSendNotification);
        updateEnabledState();
    }

    void updateEnabledState()
    {
        const auto isEnabled = clocks[static_cast<size_t> (selected)].enabled;
        name.setEnabled (isEnabled);
        ratio.setEnabled (isEnabled); phase.setEnabled (isEnabled); swing.setEnabled (isEnabled);
        nameLabel.setEnabled (isEnabled); ratioLabel.setEnabled (isEnabled);
        phaseLabel.setEnabled (isEnabled); swingLabel.setEnabled (isEnabled);
    }

    void commitName()
    {
        if (updating) return;
        auto entered = name.getText().trim();
        if (entered.isEmpty()) entered = "Clock " + juce::String (selected + 1);
        clocks[static_cast<size_t> (selected)].name = entered;
        name.setText (entered, false);
        rebuildClockChoices();
        controlsChanged();
    }

    void controlsChanged()
    {
        updateEnabledState();
        if (onChange) onChange (clocks);
    }
};

class ModulatorSettingsComponent final : public juce::Component
{
public:
    ModulatorSettingsComponent (Modulator initial, std::function<void(const Modulator&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText ("Modulator", juce::dontSendNotification);
        addAndMakeVisible (title);

        enabled.setButtonText ("Enabled");
        enabled.setToggleState (value.enabled, juce::dontSendNotification);
        enabled.setColour (juce::ToggleButton::textColourId, textPrimary());
        enabled.onClick = [this] { value.enabled = enabled.getToggleState(); notify(); updateEnabledState(); };
        addAndMakeVisible (enabled);

        styleEditorLabel (nameLabel, 12.0f, false);
        nameLabel.setText ("Name", juce::dontSendNotification);
        addAndMakeVisible (nameLabel);
        name.setText (value.name, false);
        name.setSelectAllWhenFocused (true);
        name.setColour (juce::TextEditor::backgroundColourId, raisedSurface());
        name.setColour (juce::TextEditor::textColourId, textPrimary());
        name.setColour (juce::TextEditor::outlineColourId, subtleStroke());
        name.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xffd884ff));
        name.onFocusLost = [this] { commitName(); };
        name.onReturnKey = [this] { commitName(); name.giveAwayKeyboardFocus(); };
        addAndMakeVisible (name);

        styleEditorLabel (shapeLabel, 12.0f, false);
        shapeLabel.setText ("Shape", juce::dontSendNotification);
        addAndMakeVisible (shapeLabel);
        shape.addItemList ({ "Sine", "Triangle", "Square", "Random" }, 1);
        shape.setSelectedId (static_cast<int> (value.shape) + 1, juce::dontSendNotification);
        shape.onChange = [this]
        {
            value.shape = static_cast<Modulator::Shape> (juce::jlimit (0, 3, shape.getSelectedId() - 1));
            notify();
        };
        addAndMakeVisible (shape);

        configureSlider (cycleLabel, cycle, "Cycle", 0.125, 64.0, 0.125, value.cycleBeats, " beats");
        configureSlider (phaseLabel, phase, "Phase", 0.0, 360.0, 1.0, value.phase * 360.0, " deg");
        cycle.onValueChange = [this] { value.cycleBeats = cycle.getValue(); notify(); };
        phase.onValueChange = [this] { value.phase = phase.getValue() / 360.0; notify(); };
        setSize (350, 245);
        updateEnabledState();
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (27));
        enabled.setBounds (area.removeFromTop (28));
        layoutEditorRow (area, nameLabel, name);
        auto row = area.removeFromTop (39);
        shapeLabel.setBounds (row.removeFromLeft (96));
        shape.setBounds (row.reduced (0, 4));
        layoutSliderRow (area, cycleLabel, cycle);
        layoutSliderRow (area, phaseLabel, phase);
    }

private:
    Modulator value;
    std::function<void(const Modulator&)> onChange;
    juce::Label title, nameLabel, shapeLabel, cycleLabel, phaseLabel;
    juce::ToggleButton enabled;
    juce::TextEditor name;
    juce::ComboBox shape;
    juce::Slider cycle, phase;

    void configureSlider (juce::Label& label, juce::Slider& slider, const juce::String& text,
                          double min, double max, double step, double current, const juce::String& suffix)
    {
        styleEditorLabel (label, 12.0f, false);
        label.setText (text, juce::dontSendNotification);
        addAndMakeVisible (label);
        slider.setRange (min, max, step);
        slider.setValue (current, juce::dontSendNotification);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 82, 26);
        slider.setTextValueSuffix (suffix);
        slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffd884ff));
        addAndMakeVisible (slider);
    }

    static void layoutEditorRow (juce::Rectangle<int>& area, juce::Label& label, juce::TextEditor& editor)
    {
        auto row = area.removeFromTop (39);
        label.setBounds (row.removeFromLeft (96));
        editor.setBounds (row.reduced (0, 4));
    }

    static void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
    {
        auto row = area.removeFromTop (39);
        label.setBounds (row.removeFromLeft (96));
        slider.setBounds (row);
    }

    void commitName()
    {
        auto entered = name.getText().trim();
        if (entered.isEmpty()) entered = "Modulator";
        value.name = entered;
        name.setText (entered, false);
        notify();
    }

    void updateEnabledState()
    {
        for (auto* component : std::array<juce::Component*, 4> { &name, &shape, &cycle, &phase })
            component->setEnabled (value.enabled);
        nameLabel.setEnabled (value.enabled); shapeLabel.setEnabled (value.enabled);
        cycleLabel.setEnabled (value.enabled); phaseLabel.setEnabled (value.enabled);
    }

    void notify() { if (onChange) onChange (value); }
};

class ModulationConnectionSettingsComponent final : public juce::Component
{
public:
    ModulationConnectionSettingsComponent (ModulationConnection initial,
                                           std::function<void(const ModulationConnection&)> changed)
        : value (std::move (initial)), onChange (std::move (changed))
    {
        styleEditorLabel (title, 15.0f, true);
        title.setText (modulationTargetName (value.targetKind), juce::dontSendNotification);
        addAndMakeVisible (title);
        styleEditorLabel (parameterLabel, 12.0f, false);
        parameterLabel.setText ("Parameter", juce::dontSendNotification);
        addAndMakeVisible (parameterLabel);
        parameter.addItemList (modulationParameterNames (value.targetKind), 1);
        parameter.setSelectedId (juce::jlimit (1, parameter.getNumItems(), value.parameter + 1), juce::dontSendNotification);
        parameter.onChange = [this] { value.parameter = juce::jmax (0, parameter.getSelectedId() - 1); notify(); };
        addAndMakeVisible (parameter);

        styleEditorLabel (depthLabel, 12.0f, false);
        depthLabel.setText ("Depth", juce::dontSendNotification);
        addAndMakeVisible (depthLabel);
        depth.setRange (0.0, 100.0, 1.0);
        depth.setValue (value.depth * 100.0, juce::dontSendNotification);
        depth.setSliderStyle (juce::Slider::LinearHorizontal);
        depth.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 26);
        depth.setTextValueSuffix ("%");
        depth.setColour (juce::Slider::thumbColourId, juce::Colour (0xffd884ff));
        depth.onValueChange = [this] { value.depth = depth.getValue() / 100.0; notify(); };
        addAndMakeVisible (depth);

        styleEditorLabel (offsetLabel, 12.0f, false);
        offsetLabel.setText ("Offset", juce::dontSendNotification);
        addAndMakeVisible (offsetLabel);
        offset.setRange (-100.0, 100.0, 1.0);
        offset.setValue (value.offset * 100.0, juce::dontSendNotification);
        offset.setSliderStyle (juce::Slider::LinearHorizontal);
        offset.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 26);
        offset.setTextValueSuffix ("%");
        offset.setColour (juce::Slider::thumbColourId, juce::Colour (0xff65e6d4));
        offset.onValueChange = [this] { value.offset = offset.getValue() / 100.0; notify(); };
        addAndMakeVisible (offset);

        inverted.setButtonText ("Invert modulation");
        inverted.setToggleState (value.inverted, juce::dontSendNotification);
        inverted.setColour (juce::ToggleButton::textColourId, textPrimary());
        inverted.onClick = [this] { value.inverted = inverted.getToggleState(); notify(); };
        addAndMakeVisible (inverted);
        setSize (330, 204);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surfaceColour()); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 11);
        title.setBounds (area.removeFromTop (28));
        auto row = area.removeFromTop (39);
        parameterLabel.setBounds (row.removeFromLeft (92));
        parameter.setBounds (row.reduced (0, 4));
        row = area.removeFromTop (39);
        depthLabel.setBounds (row.removeFromLeft (92));
        depth.setBounds (row);
        row = area.removeFromTop (39);
        offsetLabel.setBounds (row.removeFromLeft (92));
        offset.setBounds (row);
        inverted.setBounds (area.removeFromTop (30).withTrimmedLeft (92));
    }

private:
    ModulationConnection value;
    std::function<void(const ModulationConnection&)> onChange;
    juce::Label title, parameterLabel, depthLabel, offsetLabel;
    juce::ComboBox parameter;
    juce::Slider depth, offset;
    juce::ToggleButton inverted;
    void notify() { if (onChange) onChange (value); }
};

class RoadCanvas final : public juce::Component,
                         private juce::Timer
{
public:
    enum class Tool
    {
        select,
        draw,
        pipe,
        warpPipe,
        tap,
        drain,
        clone,
        speedLimit,
        wait,
        strike,
        teleport,
        filter,
        logic,
        modulator,
        modConnect,
        edit,
        disc,
        erase
    };

    enum class RouteLayer
    {
        shadow,
        glow,
        core,
        shine
    };

    struct RoutePaintItem
    {
        juce::Path path;
        juce::Rectangle<float> bounds;
        bool selected = false;
        bool pipe = false;
        bool warpPipe = false;
        juce::Colour colour;
        float alpha = 1.0f;
    };

    struct DiscHandle
    {
        std::vector<int> worldPath;
        int discIndex = -1;

        bool isValid() const noexcept { return discIndex >= 0; }
    };

    struct DiscInfo
    {
        bool valid = false;
        int soundElementCount = 0;
        int nestedWorldCount = 0;
        int scCodeCount = 0;
        int pdPatchCount = 0;
        int scSheetCount = 0;
        int orcaGridCount = 0;
        bool hasNestedWorld = false;
        bool hasScCode = false;
        juce::String scCode;
        float scDurationSeconds = -1.0f;
        bool hasPdPatch = false;
        juce::String pdPatch;
        float pdDurationSeconds = -1.0f;
        bool hasScSheet = false;
        int scSheetRowCount = 0;
        int scSheetSectionCount = 0;
        bool hasOrcaGrid = false;
        int orcaGridWidth = 0;
        int orcaGridHeight = 0;
        int nestedRouteCount = 0;
        int nestedDiscCount = 0;
        int carouselCount = 0;
        bool hasCarousel = false;
        int carouselItemCount = 0;
        int pipeWorldCount = 0;
        bool hasPipeWorld = false;
        int triggerMode = 0;
        int elementMode = 0;
        double elementProbability = 0.5;
        bool holdDropsUntilFinished = false;
        float level = 1.0f;
        float pan = 0.0f;
        bool muted = false;
        bool solo = false;
    };

    struct MixerChannel
    {
        int discIndex = -1;
        juce::String name;
        float level = 1.0f;
        float pan = 0.0f;
        bool muted = false;
        bool solo = false;
        bool selected = false;
    };

    struct ScCodeInfo
    {
        bool valid = false;
        juce::String code;
        float durationSeconds = -1.0f;
        int index = 0;
        int count = 0;
    };

    struct PdPatchInfo
    {
        bool valid = false;
        juce::String patch;
        juce::String searchPath;
        float durationSeconds = -1.0f;
        int index = 0;
        int count = 0;
    };

    std::function<void()> onRoutesChanged;
    std::function<void()> onDiscPanelRequested;
    std::function<void()> onSelectionChanged;
    std::function<void(int)> onDiscFlowTriggered;
    std::function<bool(const juce::String&)> onDiscPlaybackActive;

    RoadCanvas()
    {
        setWantsKeyboardFocus (true);
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        startTimerHz (60);
        lastCommittedState = createProjectState();
    }

    void setFlowTiming (double bpm, double beatsPerGridUnit)
    {
        flowBpm = juce::jlimit (20.0, 400.0, bpm);
        flowBeatsPerGridUnit = juce::jmax (1.0 / 16.0, beatsPerGridUnit);
    }

    using ClockBank = std::array<SequencingClock, 4>;

    const ClockBank& getSequencingClocks() const noexcept { return sequencingClocks; }

    void setSequencingClocks (const ClockBank& clocks)
    {
        sequencingClocks = clocks;
        for (auto& clock : sequencingClocks)
        {
            clock.name = clock.name.trim().isNotEmpty() ? clock.name.trim() : "Clock";
            clock.ratio = juce::jlimit (0.125, 8.0, clock.ratio);
            clock.phaseBeats = juce::jlimit (0.0, 16.0, clock.phaseBeats);
            clock.swing = juce::jlimit (0.0, 0.75, clock.swing);
        }
        resetFlowPulses();
        notifyChanged();
        repaint();
    }

    void setFlowRunning (bool shouldRun)
    {
        flowRunning = shouldRun;
        lastFlowTimeMs = juce::Time::getMillisecondCounterHiRes();
        if (flowRunning && flowPulses.empty()) resetFlowPulses();
        repaint();
    }

    void resetFlowToStart()
    {
        flowRunning = false;
        resetFlowPulses();
        lastFlowTimeMs = juce::Time::getMillisecondCounterHiRes();
        repaint();
    }

    bool isFlowRunning() const noexcept { return flowRunning; }

    void setTool (Tool newTool)
    {
        if (modulationLayerVisible && newTool != Tool::select && newTool != Tool::modulator
            && newTool != Tool::modConnect && newTool != Tool::erase)
            newTool = Tool::select;
        tool = newTool;
        if (tool != Tool::tap) selectedTap = -1;
        if (tool != Tool::drain) selectedDrain = -1;
        if (tool != Tool::clone) selectedCloner = -1;
        if (tool != Tool::speedLimit) selectedSpeedLimit = -1;
        if (tool != Tool::wait) selectedWait = -1;
        if (tool != Tool::strike) selectedStrike = -1;
        if (tool != Tool::teleport) selectedTeleport = -1;
        if (tool != Tool::filter) selectedFilter = -1;
        if (tool != Tool::logic) selectedLogic = -1;
        selectedRoute = -1;
        selectedNode = -1;
        draggingNode = false;
        connectingModulator = -1;
        repaint();
    }

    Tool getTool() const noexcept { return tool; }

    bool isModulationLayerVisible() const noexcept { return modulationLayerVisible; }

    bool enterModulationLayer()
    {
        if (modulationLayerVisible) return false;
        worldPath.clear();
        modulationLayerVisible = true;
        resetSelection();
        selectedModulator = selectedModulationConnection = -1;
        setTool (Tool::select);
        resetView();
        notifySelectionChanged();
        repaint();
        return true;
    }

    bool exitModulationLayer()
    {
        if (! modulationLayerVisible) return false;
        modulationLayerVisible = false;
        selectedModulator = selectedModulationConnection = -1;
        setTool (Tool::select);
        resetView();
        notifySelectionChanged();
        repaint();
        return true;
    }

    int getModulatorCount() const noexcept { return static_cast<int> (modulators.size()); }
    int getModulationConnectionCount() const noexcept { return static_cast<int> (modulationConnections.size()); }

    void setSnapEnabled (bool shouldSnap)
    {
        snapEnabled = shouldSnap;
        repaint();
    }

    bool isSnapEnabled() const noexcept { return snapEnabled; }

    void setOrbitElementsDimmed (bool shouldDim)
    {
        orbitElementsDimmed = shouldDim;
        repaint();
    }

    void setCompactDiscs (bool shouldUseCompactDiscs)
    {
        compactDiscs = shouldUseCompactDiscs;
        repaint();
    }

    void setFlowDebugVisible (bool shouldShow)
    {
        flowDebugVisible = shouldShow;
        repaint();
    }

    int getRouteCount() const noexcept { return static_cast<int> (routes().size()); }
    int getDiscCount() const noexcept { return static_cast<int> (discs().size()); }

    bool selectDiscForFlow (int index)
    {
        if (! juce::isPositiveAndBelow (index, static_cast<int> (discs().size()))) return false;
        selectedDisc = index;
        selectedRoute = -1;
        selectedNode = -1;
        notifySelectionChanged();
        repaint();
        return true;
    }

    void flashDiscForFlow (int index)
    {
        if (! juce::isPositiveAndBelow (index, static_cast<int> (discs().size()))) return;
        discFlashUntil[discKeyForCurrentWorld (index)] = juce::Time::getMillisecondCounterHiRes() + 420.0;
        repaint();
    }

    int getElementCount() const
    {
        return countElementsRecursive (rootDiscs);
    }

    DiscHandle getSelectedDiscHandle() const
    {
        return { worldPath, selectedDisc };
    }

    int getSelectedDiscTriggerMode() const
    {
        return selectedDisc >= 0 && selectedDisc < static_cast<int> (discs().size())
                   ? static_cast<int> (discs()[static_cast<size_t> (selectedDisc)].triggerMode)
                   : 0;
    }

    void setSelectedDiscTriggerMode (int mode)
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size())) return;
        discs()[static_cast<size_t> (selectedDisc)].triggerMode = static_cast<Disc::TriggerMode> (juce::jlimit (0, 2, mode));
        notifyChanged();
    }

    void setSelectedDiscElementMode (int mode)
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size())) return;
        discs()[static_cast<size_t> (selectedDisc)].elementMode = static_cast<Disc::ElementMode> (juce::jlimit (0, 4, mode));
        notifyChanged();
    }

    void setSelectedDiscElementProbability (double probability)
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size())) return;
        discs()[static_cast<size_t> (selectedDisc)].elementProbability = juce::jlimit (0.0, 1.0, probability);
        notifyChanged();
    }

    void setSelectedDiscHoldDrops (bool shouldHold)
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size())) return;
        discs()[static_cast<size_t> (selectedDisc)].holdDropsUntilFinished = shouldHold;
        notifyChanged();
    }

    void setSelectedDiscMix (float level, float pan, bool muted, bool solo)
    {
        if (! juce::isPositiveAndBelow (selectedDisc, static_cast<int> (discs().size()))) return;
        auto& disc = discs()[static_cast<size_t> (selectedDisc)];
        disc.level = juce::jlimit (0.0f, 1.5f, level);
        disc.pan = juce::jlimit (-1.0f, 1.0f, pan);
        disc.muted = muted; disc.solo = solo;
        notifyChanged();
    }

    bool canEnterSelectedDiscWorld() const
    {
        if (modulationLayerVisible) return false;
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        return discs()[static_cast<size_t> (selectedDisc)].hasNestedWorld();
    }

    juce::String getLayerPathText() const
    {
        if (modulationLayerVisible) return "Modulation / Root";
        juce::String text ("Root");

        for (const auto discIndex : worldPath)
            text << " / D" << juce::String (discIndex + 1);

        return text;
    }

    bool addElementToSelectedDisc()
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        discs()[static_cast<size_t> (selectedDisc)].soundElementCount++;
        notifyChanged();
        repaint();
        return true;
    }

    bool removeElementFromSelectedDisc()
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        auto& disc = discs()[static_cast<size_t> (selectedDisc)];

        if (disc.soundElementCount <= 0)
            return false;

        disc.soundElementCount--;
        notifyChanged();
        repaint();
        return true;
    }

    int getLastNestedWorldIndex() const
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return -1;

        return discs()[static_cast<size_t> (selectedDisc)].nestedWorldCount - 1;
    }

    int getLastScCodeIndex() const
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return -1;

        return static_cast<int> (discs()[static_cast<size_t> (selectedDisc)].scCodeElements.size()) - 1;
    }

    int getLastPdPatchIndex() const
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return -1;

        return static_cast<int> (discs()[static_cast<size_t> (selectedDisc)].pdPatches.size()) - 1;
    }

    int getLastScSheetIndex() const
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return -1;

        return static_cast<int> (discs()[static_cast<size_t> (selectedDisc)].scSheets.size()) - 1;
    }

    int getLastOrcaGridIndex() const
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return -1;

        return static_cast<int> (discs()[static_cast<size_t> (selectedDisc)].orcaGrids.size()) - 1;
    }

    int getLastCarouselIndex() const
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size())) return -1;
        return static_cast<int> (discs()[static_cast<size_t> (selectedDisc)].carousels.size()) - 1;
    }

    bool addNestedWorldToSelectedDisc()
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        discs()[static_cast<size_t> (selectedDisc)].nestedWorldCount++;
        notifyChanged();
        repaint();
        return true;
    }

    bool removeNestedWorldFromSelectedDisc()
    {
        return removeNestedWorldFromSelectedDisc (getLastNestedWorldIndex());
    }

    bool removeNestedWorldFromSelectedDisc (int index)
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        auto& disc = discs()[static_cast<size_t> (selectedDisc)];

        if (! juce::isPositiveAndBelow (index, disc.nestedWorldCount))
            return false;

        disc.nestedWorldCount--;
        notifyChanged();
        repaint();
        return true;
    }

    bool addScCodeToSelectedDisc()
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        auto& disc = discs()[static_cast<size_t> (selectedDisc)];

        disc.scCodeElements.push_back ({});

        notifyChanged();
        repaint();
        return true;
    }

    bool removeScCodeFromSelectedDisc()
    {
        return removeScCodeFromSelectedDisc (getLastScCodeIndex());
    }

    bool removeScCodeFromSelectedDisc (int index)
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        auto& disc = discs()[static_cast<size_t> (selectedDisc)];

        if (! juce::isPositiveAndBelow (index, static_cast<int> (disc.scCodeElements.size())))
            return false;

        disc.scCodeElements.erase (disc.scCodeElements.begin() + index);
        notifyChanged();
        repaint();
        return true;
    }

    bool addPdPatchToSelectedDisc()
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        discs()[static_cast<size_t> (selectedDisc)].pdPatches.push_back ({});
        notifyChanged();
        repaint();
        return true;
    }

    bool removePdPatchFromSelectedDisc()
    {
        return removePdPatchFromSelectedDisc (getLastPdPatchIndex());
    }

    bool removePdPatchFromSelectedDisc (int index)
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        auto& disc = discs()[static_cast<size_t> (selectedDisc)];

        if (! juce::isPositiveAndBelow (index, static_cast<int> (disc.pdPatches.size())))
            return false;

        disc.pdPatches.erase (disc.pdPatches.begin() + index);
        notifyChanged();
        repaint();
        return true;
    }

    bool setSelectedDiscScCode (const juce::String& code)
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        auto& disc = discs()[static_cast<size_t> (selectedDisc)];
        if (disc.scCodeElements.empty())
            disc.scCodeElements.push_back ({});

        const auto hadScCode = disc.hasScCodeElement();
        disc.scCodeElements.front().code = code;

        if (hadScCode != disc.hasScCodeElement())
            notifyChanged();

        repaint();
        return true;
    }

    bool setSelectedDiscScDuration (float seconds)
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        auto& disc = discs()[static_cast<size_t> (selectedDisc)];
        if (disc.scCodeElements.empty())
            disc.scCodeElements.push_back ({});

        disc.scCodeElements.front().durationSeconds = seconds < 0.0f
                                                          ? -1.0f
                                                          : juce::jlimit (0.25f, 9999.0f, seconds);
        repaint();
        return true;
    }

    bool addScSheetToSelectedDisc()
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        auto& disc = discs()[static_cast<size_t> (selectedDisc)];
        disc.scSheets.push_back (defaultScSheet());

        notifyChanged();
        repaint();
        return true;
    }

    bool removeScSheetFromSelectedDisc()
    {
        return removeScSheetFromSelectedDisc (getLastScSheetIndex());
    }

    bool removeScSheetFromSelectedDisc (int index)
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        auto& disc = discs()[static_cast<size_t> (selectedDisc)];
        if (! juce::isPositiveAndBelow (index, static_cast<int> (disc.scSheets.size())))
            return false;

        disc.scSheets.erase (disc.scSheets.begin() + index);
        notifyChanged();
        repaint();
        return true;
    }

    ScoreDocument* getSelectedDiscScSheetDocument()
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return nullptr;

        auto& disc = discs()[static_cast<size_t> (selectedDisc)];
        return ! disc.scSheets.empty() ? &disc.scSheets.front() : nullptr;
    }

    bool addScSheetRowToSelectedDisc()
    {
        if (auto* document = getSelectedDiscScSheetDocument())
        {
            document->addDefaultRow();
            notifyChanged();
            repaint();
            return true;
        }

        return false;
    }

    bool removeScSheetRowFromSelectedDisc (int row)
    {
        if (auto* document = getSelectedDiscScSheetDocument())
        {
            if (! juce::isPositiveAndBelow (row, document->getNumRows()))
                return false;

            document->removeRow (row);
            notifyChanged();
            repaint();
            return true;
        }

        return false;
    }

    bool duplicateScSheetRowFromSelectedDisc (int row)
    {
        if (auto* document = getSelectedDiscScSheetDocument())
        {
            if (! juce::isPositiveAndBelow (row, document->getNumRows()))
                return false;

            document->duplicateRow (row);
            notifyChanged();
            repaint();
            return true;
        }

        return false;
    }

    void notifySelectedDiscScSheetChanged()
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return;

        repaint();
    }

    bool addOrcaGridToSelectedDisc()
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        auto& disc = discs()[static_cast<size_t> (selectedDisc)];
        disc.orcaGrids.push_back (defaultOrcaGrid());

        notifyChanged();
        repaint();
        return true;
    }

    bool addCarouselToSelectedDisc()
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size())) return false;
        discs()[static_cast<size_t> (selectedDisc)].carousels.push_back (CarouselDocument::createDefault());
        notifyChanged(); repaint(); return true;
    }

    bool removeCarouselFromSelectedDisc (int index = -1)
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size())) return false;
        auto& values = discs()[static_cast<size_t> (selectedDisc)].carousels;
        if (index < 0) index = static_cast<int> (values.size()) - 1;
        if (! juce::isPositiveAndBelow (index, static_cast<int> (values.size()))) return false;
        values.erase (values.begin() + index); notifyChanged(); repaint(); return true;
    }

    bool addPipeWorldToSelectedDisc()
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size())) return false;
        discs()[static_cast<size_t> (selectedDisc)].pipeWorlds.emplace_back();
        notifyChanged(); repaint(); return true;
    }

    bool removePipeWorldFromSelectedDisc (int index = -1)
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size())) return false;
        auto& values = discs()[static_cast<size_t> (selectedDisc)].pipeWorlds;
        if (index < 0) index = static_cast<int> (values.size()) - 1;
        if (! juce::isPositiveAndBelow (index, static_cast<int> (values.size()))) return false;
        values.erase (values.begin() + index); notifyChanged(); repaint(); return true;
    }

    bool removeOrcaGridFromSelectedDisc()
    {
        return removeOrcaGridFromSelectedDisc (getLastOrcaGridIndex());
    }

    bool removeOrcaGridFromSelectedDisc (int index)
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        auto& disc = discs()[static_cast<size_t> (selectedDisc)];
        if (! juce::isPositiveAndBelow (index, static_cast<int> (disc.orcaGrids.size())))
            return false;

        disc.orcaGrids.erase (disc.orcaGrids.begin() + index);
        notifyChanged();
        repaint();
        return true;
    }

    bool clearSelectedDiscOrcaGrid()
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        auto& disc = discs()[static_cast<size_t> (selectedDisc)];
        if (disc.orcaGrids.empty())
            return false;

        disc.orcaGrids.front() = gridcollider::GridModel (16, 10).createSnapshot();
        notifyChanged();
        repaint();
        return true;
    }

    gridcollider::GridModel::Snapshot getSelectedDiscOrcaGrid() const
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return {};

        const auto& disc = discs()[static_cast<size_t> (selectedDisc)];
        return ! disc.orcaGrids.empty() ? disc.orcaGrids.front() : gridcollider::GridModel::Snapshot {};
    }

    bool setSelectedDiscOrcaGrid (const gridcollider::GridModel::Snapshot& snapshot)
    {
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        auto& disc = discs()[static_cast<size_t> (selectedDisc)];
        if (disc.orcaGrids.empty())
            return false;

        disc.orcaGrids.front() = snapshot;
        repaint();
        return true;
    }

    DiscInfo getSelectedDiscInfo() const
    {
        return getDiscInfo (getSelectedDiscHandle());
    }

    std::vector<MixerChannel> getMixerChannels() const
    {
        std::vector<MixerChannel> channels;
        channels.reserve (discs().size());
        for (int i = 0; i < static_cast<int> (discs().size()); ++i)
        {
            const auto& disc = discs()[static_cast<size_t> (i)];
            channels.push_back ({ i, "Disc " + juce::String (i + 1), disc.level, disc.pan, disc.muted, disc.solo, i == selectedDisc });
        }
        return channels;
    }

    void setMixerChannel (int index, float level, float pan, bool muted, bool solo)
    {
        if (! juce::isPositiveAndBelow (index, static_cast<int> (discs().size()))) return;
        auto& disc = discs()[static_cast<size_t> (index)];
        disc.level = juce::jlimit (0.0f, 1.5f, level); disc.pan = juce::jlimit (-1.0f, 1.0f, pan);
        disc.muted = muted; disc.solo = solo; notifyChanged(); repaint();
    }

    void selectMixerChannel (int index)
    {
        if (! juce::isPositiveAndBelow (index, static_cast<int> (discs().size()))) return;
        selectedDisc = index; selectedRoute = selectedNode = -1; notifySelectionChanged(); repaint(); requestDiscPanel();
    }

    DiscInfo getDiscInfo (const DiscHandle& handle) const
    {
        if (const auto* disc = findDisc (handle))
            return infoForDisc (*disc);

        return {};
    }

    bool setDiscScCode (const DiscHandle& handle, const juce::String& code)
    {
        return setDiscScCode (handle, 0, code);
    }

    bool setDiscScCode (const DiscHandle& handle, int codeIndex, const juce::String& code)
    {
        if (auto* disc = findDisc (handle))
        {
            if (codeIndex < 0)
                return false;

            while (static_cast<int> (disc->scCodeElements.size()) <= codeIndex)
                disc->scCodeElements.push_back ({});

            const auto hadScCode = disc->hasScCodeElement();
            disc->scCodeElements[static_cast<size_t> (codeIndex)].code = code;

            if (hadScCode != disc->hasScCodeElement())
                notifyChanged();

            repaint();
            return true;
        }

        return false;
    }

    bool setDiscScDuration (const DiscHandle& handle, float seconds)
    {
        return setDiscScDuration (handle, 0, seconds);
    }

    bool setDiscScDuration (const DiscHandle& handle, int codeIndex, float seconds)
    {
        if (auto* disc = findDisc (handle))
        {
            if (codeIndex < 0)
                return false;

            while (static_cast<int> (disc->scCodeElements.size()) <= codeIndex)
                disc->scCodeElements.push_back ({});

            disc->scCodeElements[static_cast<size_t> (codeIndex)].durationSeconds = seconds < 0.0f ? -1.0f : juce::jlimit (0.25f, 9999.0f, seconds);
            repaint();
            return true;
        }

        return false;
    }

    ScCodeInfo getDiscScCodeInfo (const DiscHandle& handle, int codeIndex) const
    {
        if (const auto* disc = findDisc (handle))
        {
            if (! juce::isPositiveAndBelow (codeIndex, static_cast<int> (disc->scCodeElements.size())))
                return {};

            const auto& element = disc->scCodeElements[static_cast<size_t> (codeIndex)];
            return { true, element.code, element.durationSeconds, codeIndex, static_cast<int> (disc->scCodeElements.size()) };
        }

        return {};
    }

    std::vector<DiscAudioTrigger> getDiscScCodeTrigger (const DiscHandle& handle, int codeIndex) const
    {
        std::vector<DiscAudioTrigger> triggers;

        const auto info = getDiscScCodeInfo (handle, codeIndex);
        if (! info.valid || info.code.trim().isEmpty())
            return triggers;

        DiscAudioTrigger trigger;
        trigger.scCode = info.code;
        trigger.scDurationSeconds = info.durationSeconds;
        trigger.depth = static_cast<int> (handle.worldPath.size());
        trigger.branchIndex = juce::jmax (0, handle.discIndex) * 7 + codeIndex + 1;
        triggers.push_back (std::move (trigger));
        return triggers;
    }

    bool setDiscPdPatch (const DiscHandle& handle, int patchIndex, const juce::String& patch)
    {
        return setDiscPdPatch (handle, patchIndex, patch, {}, false);
    }

    bool setDiscPdPatch (const DiscHandle& handle, int patchIndex, const juce::String& patch, const juce::String& searchPath, bool replaceSearchPath = true)
    {
        if (auto* disc = findDisc (handle))
        {
            if (patchIndex < 0)
                return false;

            while (static_cast<int> (disc->pdPatches.size()) <= patchIndex)
                disc->pdPatches.push_back ({});

            const auto hadPatch = disc->hasPdPatch();
            auto& pdPatch = disc->pdPatches[static_cast<size_t> (patchIndex)];
            pdPatch.patch = patch;
            if (replaceSearchPath)
                pdPatch.searchPath = searchPath;

            if (hadPatch != disc->hasPdPatch())
                notifyChanged();

            repaint();
            return true;
        }

        return false;
    }

    bool setDiscPdDuration (const DiscHandle& handle, int patchIndex, float seconds)
    {
        if (auto* disc = findDisc (handle))
        {
            if (patchIndex < 0)
                return false;

            while (static_cast<int> (disc->pdPatches.size()) <= patchIndex)
                disc->pdPatches.push_back ({});

            disc->pdPatches[static_cast<size_t> (patchIndex)].durationSeconds = seconds < 0.0f ? -1.0f : juce::jlimit (0.25f, 9999.0f, seconds);
            repaint();
            return true;
        }

        return false;
    }

    PdPatchInfo getDiscPdPatchInfo (const DiscHandle& handle, int patchIndex) const
    {
        if (const auto* disc = findDisc (handle))
        {
            if (! juce::isPositiveAndBelow (patchIndex, static_cast<int> (disc->pdPatches.size())))
                return {};

            const auto& element = disc->pdPatches[static_cast<size_t> (patchIndex)];
            return { true, element.patch, element.searchPath, element.durationSeconds, patchIndex, static_cast<int> (disc->pdPatches.size()) };
        }

        return {};
    }

    std::vector<DiscAudioTrigger> getDiscPdPatchTrigger (const DiscHandle& handle, int patchIndex) const
    {
        std::vector<DiscAudioTrigger> triggers;

        const auto info = getDiscPdPatchInfo (handle, patchIndex);
        if (! info.valid || info.patch.trim().isEmpty())
            return triggers;

        DiscAudioTrigger trigger;
        trigger.pdPatch = info.patch;
        trigger.pdSearchPath = info.searchPath;
        trigger.pdDurationSeconds = info.durationSeconds;
        trigger.depth = static_cast<int> (handle.worldPath.size());
        trigger.branchIndex = juce::jmax (0, handle.discIndex) * 11 + patchIndex + 1;
        triggers.push_back (std::move (trigger));
        return triggers;
    }

    ScoreDocument* getDiscScSheetDocument (const DiscHandle& handle)
    {
        auto* disc = findDisc (handle);
        return disc != nullptr && ! disc->scSheets.empty() ? &disc->scSheets.front() : nullptr;
    }

    bool addScSheetRowToDisc (const DiscHandle& handle)
    {
        if (auto* document = getDiscScSheetDocument (handle))
        {
            document->addDefaultRow();
            notifyChanged();
            repaint();
            return true;
        }

        return false;
    }

    bool removeScSheetRowFromDisc (const DiscHandle& handle, int row)
    {
        if (auto* document = getDiscScSheetDocument (handle))
        {
            if (! juce::isPositiveAndBelow (row, document->getNumRows()))
                return false;

            document->removeRow (row);
            notifyChanged();
            repaint();
            return true;
        }

        return false;
    }

    bool duplicateScSheetRowFromDisc (const DiscHandle& handle, int row)
    {
        if (auto* document = getDiscScSheetDocument (handle))
        {
            if (! juce::isPositiveAndBelow (row, document->getNumRows()))
                return false;

            document->duplicateRow (row);
            notifyChanged();
            repaint();
            return true;
        }

        return false;
    }

    void notifyDiscScSheetChanged (const DiscHandle& handle)
    {
        if (findDisc (handle) != nullptr)
            repaint();
    }

    bool clearDiscOrcaGrid (const DiscHandle& handle)
    {
        if (auto* disc = findDisc (handle))
        {
            if (disc->orcaGrids.empty())
                return false;

            disc->orcaGrids.front() = gridcollider::GridModel (16, 10).createSnapshot();
            notifyChanged();
            repaint();
            return true;
        }

        return false;
    }

    gridcollider::GridModel::Snapshot getDiscOrcaGrid (const DiscHandle& handle) const
    {
        if (const auto* disc = findDisc (handle))
            return ! disc->orcaGrids.empty() ? disc->orcaGrids.front() : gridcollider::GridModel::Snapshot {};

        return {};
    }

    bool setDiscOrcaGrid (const DiscHandle& handle, const gridcollider::GridModel::Snapshot& snapshot)
    {
        if (auto* disc = findDisc (handle))
        {
            if (disc->orcaGrids.empty())
                return false;

            disc->orcaGrids.front() = snapshot;
            repaint();
            return true;
        }

        return false;
    }

    CarouselDocument getDiscCarousel (const DiscHandle& handle, int index) const
    {
        if (const auto* disc = findDisc (handle))
            if (juce::isPositiveAndBelow (index, static_cast<int> (disc->carousels.size())))
                return disc->carousels[static_cast<size_t> (index)];
        return {};
    }

    std::vector<CarouselDocument> getSelectedDiscCarousels() const
    {
        if (selectedDisc >= 0 && selectedDisc < static_cast<int> (discs().size()))
            return discs()[static_cast<size_t> (selectedDisc)].carousels;
        return {};
    }

    std::vector<juce::String> getSelectedDiscPipeWorlds() const
    {
        if (selectedDisc >= 0 && selectedDisc < static_cast<int> (discs().size())) return discs()[static_cast<size_t> (selectedDisc)].pipeWorlds;
        return {};
    }

    bool setDiscCarousel (const DiscHandle& handle, int index, const CarouselDocument& document)
    {
        if (auto* disc = findDisc (handle))
            if (juce::isPositiveAndBelow (index, static_cast<int> (disc->carousels.size())))
            { disc->carousels[static_cast<size_t> (index)] = document; notifyChanged(); repaint(); return true; }
        return false;
    }

    juce::String getDiscPipeWorld (const DiscHandle& handle, int index) const
    {
        if (const auto* disc = findDisc (handle))
            if (juce::isPositiveAndBelow (index, static_cast<int> (disc->pipeWorlds.size()))) return disc->pipeWorlds[static_cast<size_t> (index)];
        return {};
    }

    bool setDiscPipeWorld (const DiscHandle& handle, int index, const juce::String& state)
    {
        if (auto* disc = findDisc (handle))
            if (juce::isPositiveAndBelow (index, static_cast<int> (disc->pipeWorlds.size())))
            { disc->pipeWorlds[static_cast<size_t> (index)] = state; notifyChanged(); repaint(); return true; }
        return false;
    }

    std::vector<DiscAudioTrigger> getSelectedDiscTriggers() const
    {
        std::vector<DiscAudioTrigger> triggers;

        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return triggers;

        appendDiscAudioTriggers (discs()[static_cast<size_t> (selectedDisc)],
                                 getWorldDepth(),
                                 selectedDisc,
                                 triggers);
        return triggers;
    }

    bool isSelectedDiscAudible() const
    {
        if (! juce::isPositiveAndBelow (selectedDisc, static_cast<int> (discs().size()))) return false;
        const auto& selected = discs()[static_cast<size_t> (selectedDisc)];
        const auto anySolo = std::any_of (discs().begin(), discs().end(), [] (const auto& disc) { return disc.solo; });
        return ! selected.muted && (! anySolo || selected.solo);
    }

    bool enterNestedWorldForSelectedDisc()
    {
        if (modulationLayerVisible) return false;
        if (selectedDisc < 0 || selectedDisc >= static_cast<int> (discs().size()))
            return false;

        auto& disc = discs()[static_cast<size_t> (selectedDisc)];
        if (disc.nestedWorldCount <= 0)
            disc.nestedWorldCount = 1;
        worldPath.push_back (selectedDisc);
        resetSelection();
        resetView();
        notifyChanged();
        repaint();
        return true;
    }

    bool exitNestedWorld()
    {
        if (worldPath.empty())
            return false;

        worldPath.pop_back();
        resetSelection();
        resetView();
        repaint();
        return true;
    }

    bool exitToRootWorld()
    {
        if (worldPath.empty())
            return false;

        worldPath.clear();
        resetSelection();
        resetView();
        repaint();
        return true;
    }

    bool isInsideNestedWorld() const noexcept { return ! worldPath.empty(); }
    int getWorldDepth() const noexcept { return static_cast<int> (worldPath.size()); }

    float getTotalLength() const
    {
        return std::accumulate (routes().begin(), routes().end(), 0.0f,
                                [] (float total, const RoadRoute& route) { return total + route.getLength(); });
    }

    void undo()
    {
        if (undoStates.empty()) return;
        const auto restoreModulationLayer = modulationLayerVisible;
        const auto state = undoStates.back();
        undoStates.pop_back();
        restoringHistory = true;
        applyProjectState (state);
        restoringHistory = false;
        modulationLayerVisible = restoreModulationLayer;
        if (modulationLayerVisible) worldPath.clear();
        lastCommittedState = createProjectState();
        resetSelection();
        if (onRoutesChanged != nullptr) onRoutesChanged();
        repaint();
    }

    void clear()
    {
        if (modulationLayerVisible)
        {
            modulators.clear();
            modulationConnections.clear();
            selectedModulator = selectedModulationConnection = -1;
            notifyChanged();
            repaint();
            return;
        }
        routes().clear();
        pipeTaps().clear();
        pipeDrains().clear();
        pipeCloners().clear();
        pipeSpeedLimits().clear();
        pipeWaits().clear();
        pipeStrikes().clear();
        pipeTeleports().clear();
        pipeFilters().clear();
        pipeLogics().clear();
        currentRoute.points.clear();
        selectedRoute = -1;
        selectedNode = -1;
        notifyChanged();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        drawBackground (g);

        juce::Graphics::ScopedSaveState state (g);
        g.addTransform (getViewTransform());
        if (modulationLayerVisible)
            g.beginTransparencyLayer (0.22f);

        std::vector<RoutePaintItem> routePaintItems;
        routePaintItems.reserve (routes().size() + (currentRoute.isDrawable() ? 1u : 0u));

        for (int i = 0; i < static_cast<int> (routes().size()); ++i)
            addRoutePaintItem (routePaintItems, routes()[static_cast<size_t> (i)], i == selectedRoute, 1.0f);

        if (currentRoute.isDrawable())
            addRoutePaintItem (routePaintItems, currentRoute, false, 0.68f);

        for (const auto layer : { RouteLayer::shadow, RouteLayer::glow, RouteLayer::core, RouteLayer::shine })
            for (const auto& item : routePaintItems)
                drawRouteLayer (g, item, layer);

        drawFlowPulses (g);
        drawFlowDebugEvents (g);

        for (const auto& tap : pipeTaps())
        {
            const auto colour = tap.enabled ? juce::Colour (0xff56d9ff) : textMuted();
            g.setColour (colour.withAlpha (0.20f));
            g.fillEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre (tap.position));
            g.setColour (colour);
            g.drawEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (tap.position), 2.0f);
            g.drawLine ({ tap.position.x - 3.5f, tap.position.y, tap.position.x + 3.5f, tap.position.y }, 1.8f);
            g.drawLine ({ tap.position.x, tap.position.y - 3.5f, tap.position.x, tap.position.y + 3.5f }, 1.8f);
            if (tap.clockIndex > 0)
            {
                static const std::array<juce::Colour, 4> clockColours {
                    juce::Colour (0xff53d8fb), juce::Colour (0xffff6f91),
                    juce::Colour (0xffffc857), juce::Colour (0xffad8cff)
                };
                const auto clock = juce::jlimit (1, 4, tap.clockIndex);
                const auto enabled = sequencingClocks[static_cast<size_t> (clock - 1)].enabled;
                const auto badge = juce::Rectangle<float> (9.0f, 9.0f)
                                       .withCentre (tap.position.translated (7.0f, -7.0f));
                g.setColour (juce::Colour (0xff08110e).withAlpha (0.92f));
                g.fillEllipse (badge.expanded (1.2f));
                g.setColour (clockColours[static_cast<size_t> (clock - 1)].withMultipliedAlpha (enabled ? 1.0f : 0.32f));
                g.fillEllipse (badge);
                g.setColour (juce::Colour (0xff08110e));
                g.setFont (juce::FontOptions (6.5f, juce::Font::bold));
                g.drawText (juce::String (clock), badge, juce::Justification::centred);
            }
        }

        for (const auto& drain : pipeDrains())
        {
            const auto colour = pdPatchElementColour().withMultipliedAlpha (drain.enabled ? 1.0f : 0.28f);
            g.setColour (colour.withAlpha (0.18f));
            g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (drain.position));
            g.setColour (colour);
            g.drawEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (drain.position), 1.8f);
            g.drawLine ({ drain.position.x - 3.0f, drain.position.y - 2.0f,
                          drain.position.x, drain.position.y + 3.0f }, 1.6f);
            g.drawLine ({ drain.position.x, drain.position.y + 3.0f,
                          drain.position.x + 3.0f, drain.position.y - 2.0f }, 1.6f);
        }

        for (const auto& cloner : pipeCloners())
        {
            const auto colour = juce::Colour (0xffb888ff).withMultipliedAlpha (cloner.enabled ? 1.0f : 0.28f);
            g.setColour (colour.withAlpha (0.18f));
            g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (cloner.position));
            g.setColour (colour);
            g.drawEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (cloner.position), 1.8f);
            g.drawLine ({ cloner.position.x - 3.5f, cloner.position.y,
                          cloner.position.x + 3.5f, cloner.position.y - 3.5f }, 1.6f);
            g.drawLine ({ cloner.position.x - 3.5f, cloner.position.y,
                          cloner.position.x + 3.5f, cloner.position.y + 3.5f }, 1.6f);
        }

        for (const auto& limit : pipeSpeedLimits())
        {
            const auto colour = juce::Colour (0xffffb84d).withMultipliedAlpha (limit.enabled ? 1.0f : 0.28f);
            const auto sign = juce::Rectangle<float> (14.0f, 14.0f).withCentre (limit.position);
            g.setColour (colour.withAlpha (0.18f));
            g.fillEllipse (sign.expanded (4.0f));
            g.setColour (colour);
            g.drawEllipse (sign, 1.8f);
            g.setFont (juce::FontOptions (7.5f, juce::Font::bold));
            g.drawText (juce::String (limit.bpmMultiplier, 2) + "x", sign, juce::Justification::centred, false);
        }

        for (const auto& wait : pipeWaits())
        {
            const auto colour = juce::Colour (0xffffd166).withMultipliedAlpha (wait.enabled ? 1.0f : 0.28f);
            const auto marker = juce::Rectangle<float> (14.0f, 14.0f).withCentre (wait.position);
            g.setColour (colour.withAlpha (0.18f));
            g.fillEllipse (marker.expanded (4.0f));
            g.setColour (colour);
            g.drawEllipse (marker, 1.6f);
            g.drawLine ({ marker.getX() + 4.0f, marker.getY() + 3.0f,
                          marker.getRight() - 4.0f, marker.getBottom() - 3.0f }, 1.4f);
            g.drawLine ({ marker.getRight() - 4.0f, marker.getY() + 3.0f,
                          marker.getX() + 4.0f, marker.getBottom() - 3.0f }, 1.4f);
        }

        for (const auto& strike : pipeStrikes())
        {
            const auto colour = juce::Colour (0xffff6b8a).withMultipliedAlpha (strike.enabled ? 1.0f : 0.28f);
            const std::array<juce::Point<float>, 4> endpoints {{
                strike.position + juce::Point<float> (-gridSize, -gridSize),
                strike.position + juce::Point<float> ( gridSize, -gridSize),
                strike.position + juce::Point<float> (-gridSize,  gridSize),
                strike.position + juce::Point<float> ( gridSize,  gridSize)
            }};
            g.setColour (colour.withAlpha (0.28f));
            for (const auto endpoint : endpoints)
                g.drawLine ({ strike.position, endpoint }, 2.2f);
            g.setColour (colour.withAlpha (0.18f));
            g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (strike.position));
            g.setColour (colour);
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (strike.position));
            for (const auto endpoint : endpoints)
            {
                g.setColour (colour.withAlpha (0.78f));
                g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (endpoint));
            }
        }

        for (int i = 0; i < static_cast<int> (pipeTeleports().size()); ++i)
        {
            const auto& teleport = pipeTeleports()[static_cast<size_t> (i)];
            const auto colour = juce::Colour (0xff53d8fb).withMultipliedAlpha (teleport.enabled ? 1.0f : 0.28f);
            if (flowDebugVisible && teleport.destinationId.isNotEmpty())
                for (const auto& destination : pipeTeleports())
                    if (destination.id == teleport.destinationId)
                    {
                        g.setColour (colour.withAlpha (0.24f));
                        g.drawDashedLine ({ teleport.position, destination.position }, std::array<float, 2> { 5.0f, 4.0f }.data(), 2, 1.1f);
                        break;
                    }
            g.setColour (colour.withAlpha (0.18f)); g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (teleport.position));
            g.setColour (colour); g.drawEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (teleport.position), 1.8f);
            g.setFont (juce::FontOptions (7.0f, juce::Font::bold));
            g.drawText (juce::String (i + 1), juce::Rectangle<float> (12.0f, 12.0f).withCentre (teleport.position), juce::Justification::centred, false);
        }
        for (const auto& filter : pipeFilters())
        {
            const auto colour = juce::Colour (0xff7dd3fc).withMultipliedAlpha (filter.enabled ? 1.0f : 0.28f);
            const auto marker = juce::Rectangle<float> (24.0f, 20.0f).withCentre (filter.position);
            g.setColour (colour.withAlpha (0.14f));
            g.fillRoundedRectangle (marker.expanded (5.0f), 6.0f);
            const auto box = juce::Rectangle<float> (16.0f, 18.0f).withCentre (filter.position);
            const auto y = filter.position.y;
            g.setColour (colour.withAlpha (0.96f));
            g.drawLine ({ marker.getX(), y, box.getX(), y }, 2.0f);
            g.drawLine ({ box.getRight(), y, marker.getRight(), y }, 2.0f);
            g.drawRoundedRectangle (box, 2.0f, 1.8f);
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawText ("F", box, juce::Justification::centred, false);
        }

        for (const auto& logic : pipeLogics())
        {
            const auto colour = juce::Colour (0xffffd166).withMultipliedAlpha (logic.enabled ? 1.0f : 0.28f);
            const auto marker = juce::Rectangle<float> (27.0f, 21.0f).withCentre (logic.position);
            const auto box = marker.reduced (4.0f, 1.5f);
            const auto abbreviation = logic.mode == PipeLogic::Mode::gate ? "G"
                                      : logic.mode == PipeLogic::Mode::counter ? "#"
                                      : logic.mode == PipeLogic::Mode::switcher ? "S"
                                      : logic.mode == PipeLogic::Mode::comparator ? "<"
                                      : logic.mode == PipeLogic::Mode::flipFlop ? "T"
                                      : "N";
            g.setColour (colour.withAlpha (0.14f));
            g.fillRoundedRectangle (marker.expanded (4.0f), 6.0f);
            g.setColour (colour.withAlpha (0.96f));
            g.drawLine ({ marker.getX(), logic.position.y, box.getX(), logic.position.y }, 2.0f);
            g.drawLine ({ box.getRight(), logic.position.y, marker.getRight(), logic.position.y }, 2.0f);
            g.drawRoundedRectangle (box, 2.5f, 1.8f);
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawText (abbreviation, box, juce::Justification::centred, false);
        }

        const auto now = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < static_cast<int> (discs().size()); ++i)
        {
            const auto flash = discFlashUntil.find (discKeyForCurrentWorld (i));
            drawDisc (g, discs()[static_cast<size_t> (i)], i == selectedDisc,
                      flash != discFlashUntil.end() && flash->second > now);
        }

        if (modulationLayerVisible)
        {
            g.endTransparencyLayer();
            drawModulationLayer (g);
            return;
        }

        drawSelectionOverlay (g);

        if (tool == Tool::edit)
            drawNodes (g);
    }

    void resized() override {}

    void mouseDown (const juce::MouseEvent& event) override
    {
        grabKeyboardFocus();

        if (event.mods.isPopupMenu())
        {
            undo();
            return;
        }

        if (event.mods.isMiddleButtonDown() || event.mods.isCommandDown())
        {
            panning = true;
            panStartScreen = event.position;
            panStartOffset = viewOffset;
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            return;
        }

        const auto worldPosition = screenToWorld (event.position);

        if (modulationLayerVisible)
        {
            selectedFlowPulse = -1;
            const auto sourceHit = findModulatorAt (worldPosition, screenToleranceToWorld (18.0f));
            if (tool == Tool::select)
            {
                selectedModulator = sourceHit;
                selectedModulationConnection = sourceHit >= 0 ? -1
                    : findModulationConnectionAt (worldPosition, screenToleranceToWorld (10.0f));
                draggingModulator = selectedModulator >= 0;
                if (draggingModulator)
                    modulatorDragOffset = modulators[static_cast<size_t> (selectedModulator)].position - worldPosition;
                repaint();
                if (selectedModulator >= 0) showModulatorSettings (selectedModulator);
                else if (selectedModulationConnection >= 0) showModulationConnectionSettings (selectedModulationConnection);
                return;
            }
            if (tool == Tool::modulator)
            {
                Modulator source;
                source.position = snapEnabled ? snapToGrid (worldPosition) : worldPosition;
                source.name = "Modulator " + juce::String (modulators.size() + 1);
                modulators.push_back (source);
                selectedModulator = static_cast<int> (modulators.size()) - 1;
                selectedModulationConnection = -1;
                notifyChanged (false);
                repaint();
                showModulatorSettings (selectedModulator);
                return;
            }
            if (tool == Tool::modConnect)
            {
                connectingModulator = sourceHit;
                modulationDragPoint = worldPosition;
                selectedModulator = sourceHit;
                selectedModulationConnection = -1;
                repaint();
                return;
            }
            if (tool == Tool::erase)
            {
                if (sourceHit >= 0)
                {
                    const auto id = modulators[static_cast<size_t> (sourceHit)].id;
                    modulators.erase (modulators.begin() + sourceHit);
                    modulationConnections.erase (std::remove_if (modulationConnections.begin(), modulationConnections.end(), [&] (const auto& connection)
                    { return connection.sourceId == id; }), modulationConnections.end());
                    selectedModulator = selectedModulationConnection = -1;
                    notifyChanged (false);
                    repaint();
                    return;
                }
                const auto connection = findModulationConnectionAt (worldPosition, screenToleranceToWorld (10.0f));
                if (connection >= 0)
                {
                    modulationConnections.erase (modulationConnections.begin() + connection);
                    selectedModulationConnection = -1;
                    notifyChanged (false);
                    repaint();
                }
                return;
            }
        }

        if (tool == Tool::select)
        {
            selectedFlowPulse = -1;
            auto nearestPulseDistance = screenToleranceToWorld (14.0f);
            for (int i = 0; i < static_cast<int> (flowPulses.size()); ++i)
            {
                const auto& pulse = flowPulses[static_cast<size_t> (i)];
                if (! juce::isPositiveAndBelow (pulse.routeIndex, static_cast<int> (routes().size()))) continue;
                const auto distance = pointAlongRoute (routes()[static_cast<size_t> (pulse.routeIndex)], pulse.distance)
                                         .getDistanceFrom (worldPosition);
                if (distance <= nearestPulseDistance)
                {
                    nearestPulseDistance = distance;
                    selectedFlowPulse = i;
                }
            }

            if (selectedFlowPulse >= 0)
            {
                selectedDisc = selectedRoute = selectedNode = -1;
                selectedTap = selectedDrain = selectedCloner = selectedSpeedLimit = selectedWait = selectedStrike = selectedTeleport = selectedFilter = selectedLogic = -1;
                notifySelectionChanged();
                repaint();
                return;
            }

            selectedTap = selectedDrain = selectedCloner = selectedSpeedLimit = selectedWait = selectedStrike = selectedTeleport = selectedFilter = selectedLogic = -1;
            const auto deviceTolerance = screenToleranceToWorld (12.0f);
            const auto findDevice = [worldPosition, deviceTolerance] (const auto& devices)
            {
                for (int i = static_cast<int> (devices.size()) - 1; i >= 0; --i)
                    if (devices[static_cast<size_t> (i)].position.getDistanceFrom (worldPosition) <= deviceTolerance) return i;
                return -1;
            };
            selectedTap = findDevice (pipeTaps());
            if (selectedTap < 0) selectedDrain = findDevice (pipeDrains());
            if (selectedTap < 0 && selectedDrain < 0) selectedCloner = findDevice (pipeCloners());
            if (selectedTap < 0 && selectedDrain < 0 && selectedCloner < 0) selectedSpeedLimit = findDevice (pipeSpeedLimits());
            if (selectedTap < 0 && selectedDrain < 0 && selectedCloner < 0 && selectedSpeedLimit < 0) selectedWait = findDevice (pipeWaits());
            if (selectedTap < 0 && selectedDrain < 0 && selectedCloner < 0 && selectedSpeedLimit < 0 && selectedWait < 0) selectedStrike = findDevice (pipeStrikes());
            if (selectedTap < 0 && selectedDrain < 0 && selectedCloner < 0 && selectedSpeedLimit < 0 && selectedWait < 0 && selectedStrike < 0) selectedTeleport = findDevice (pipeTeleports());
            if (selectedTap < 0 && selectedDrain < 0 && selectedCloner < 0 && selectedSpeedLimit < 0 && selectedWait < 0 && selectedStrike < 0 && selectedTeleport < 0) selectedFilter = findDevice (pipeFilters());
            if (selectedTap < 0 && selectedDrain < 0 && selectedCloner < 0 && selectedSpeedLimit < 0 && selectedWait < 0 && selectedStrike < 0 && selectedTeleport < 0 && selectedFilter < 0) selectedLogic = findDevice (pipeLogics());
            const auto selectedDevice = selectedTap >= 0 || selectedDrain >= 0 || selectedCloner >= 0 || selectedSpeedLimit >= 0
                                     || selectedWait >= 0 || selectedStrike >= 0 || selectedTeleport >= 0 || selectedFilter >= 0 || selectedLogic >= 0;
            const auto discDiameter = compactDiscs ? gridSize * 0.55f : gridSize;
            selectedDisc = selectedDevice ? -1 : findDiscAt (worldPosition, screenToleranceToWorld (discDiameter * 0.55f));
            selectedRoute = selectedDevice || selectedDisc >= 0 ? -1 : findNearestRoute (worldPosition, screenToleranceToWorld (12.0f));
            selectedNode = -1;

            auto itemKey = -1;
            if (selectedRoute >= 0) itemKey = selectionKey (selectionRoute, selectedRoute);
            else if (selectedDisc >= 0) itemKey = selectionKey (selectionDisc, selectedDisc);
            else if (selectedTap >= 0) itemKey = selectionKey (selectionTap, selectedTap);
            else if (selectedDrain >= 0) itemKey = selectionKey (selectionDrain, selectedDrain);
            else if (selectedCloner >= 0) itemKey = selectionKey (selectionCloner, selectedCloner);
            else if (selectedSpeedLimit >= 0) itemKey = selectionKey (selectionSpeedLimit, selectedSpeedLimit);
            else if (selectedWait >= 0) itemKey = selectionKey (selectionWait, selectedWait);
            else if (selectedStrike >= 0) itemKey = selectionKey (selectionStrike, selectedStrike);
            else if (selectedTeleport >= 0) itemKey = selectionKey (selectionTeleport, selectedTeleport);
            else if (selectedFilter >= 0) itemKey = selectionKey (selectionFilter, selectedFilter);
            else if (selectedLogic >= 0) itemKey = selectionKey (selectionLogic, selectedLogic);

            if (itemKey >= 0)
            {
                if (event.mods.isShiftDown())
                {
                    if (selectedItems.count (itemKey) != 0) selectedItems.erase (itemKey);
                    else selectedItems.insert (itemKey);
                }
                else if (selectedItems.count (itemKey) == 0)
                {
                    selectedItems.clear();
                    selectedItems.insert (itemKey);
                }
                draggingSelection = selectedItems.count (itemKey) != 0;
                selectionDragLast = snapToGrid (worldPosition);
            }
            else
            {
                if (! event.mods.isShiftDown()) selectedItems.clear();
                marqueeSelecting = true;
                marqueeStart = marqueeCurrent = worldPosition;
            }
            notifySelectionChanged();
            repaint();
            if (selectedTap >= 0) showTapSettings (selectedTap);
            else if (selectedDrain >= 0) showDrainSettings (selectedDrain);
            else if (selectedCloner >= 0) showClonerSettings (selectedCloner);
            else if (selectedSpeedLimit >= 0) showSpeedLimitSettings (selectedSpeedLimit);
            else if (selectedWait >= 0) showWaitSettings (selectedWait);
            else if (selectedStrike >= 0) showStrikeSettings (selectedStrike);
            else if (selectedTeleport >= 0) showTeleportSettings (selectedTeleport);
            else if (selectedFilter >= 0) showFilterSettings (selectedFilter);
            else if (selectedLogic >= 0) showLogicSettings (selectedLogic);
            return;
        }

        if (tool == Tool::draw || tool == Tool::pipe || tool == Tool::warpPipe)
        {
            const auto pipeHit = tool == Tool::pipe || tool == Tool::warpPipe ? findNearestSegment (worldPosition, snapRadius, -1, 1) : SegmentHit {};
            const auto point = pipeHit.route >= 0 ? pipeHit.point : snappedPoint (worldPosition);
            selectedRoute = -1;
            selectedNode = -1;
            currentRoute = {};
            currentRoute.isPipe = tool == Tool::pipe || tool == Tool::warpPipe;
            currentRoute.isWarpPipe = tool == Tool::warpPipe;
            if (currentRoute.isPipe) currentRoute.colour = pipeHit.route >= 0 ? routes()[static_cast<size_t> (pipeHit.route)].colour
                                                                              : pipeColourForIndex (static_cast<int> (routes().size()));
            currentRoute.points.push_back (point);
            drawing = true;
            repaint();
            return;
        }

        if (tool == Tool::disc)
        {
            selectedTap = -1;
            const auto segmentHit = findNearestSegment (worldPosition, screenToleranceToWorld (discLineSnapRadius), -1, 1);
            const auto point = snapToGrid (segmentHit.route >= 0 ? segmentHit.point : worldPosition);
            const auto existingDisc = findDiscAt (point);

            if (existingDisc >= 0)
            {
                selectedDisc = existingDisc;
            }
            else
            {
                Disc newDisc;
                newDisc.centre = point;
                discs().push_back (std::move (newDisc));
                selectedDisc = static_cast<int> (discs().size()) - 1;
                notifyChanged();
            }

            selectedRoute = -1;
            selectedNode = -1;
            notifySelectionChanged();
            repaint();
            requestDiscPanel();
            return;
        }

        if (tool == Tool::tap)
        {
            {
                const auto point = snapToGrid (worldPosition);
                auto& taps = pipeTaps();
                const auto existing = std::find_if (taps.begin(), taps.end(), [point] (const auto& tap) { return tap.position.getDistanceFrom (point) < 2.0f; });
                if (existing == taps.end()) { taps.push_back ({ point }); selectedTap = static_cast<int> (taps.size()) - 1; }
                else selectedTap = static_cast<int> (std::distance (taps.begin(), existing));
                selectedDisc = -1;
                selectedRoute = -1;
                showTapSettings (selectedTap);
                notifyChanged();
                repaint();
            }
            return;
        }

        if (tool == Tool::drain)
        {
            {
                const auto point = snapToGrid (worldPosition);
                auto& drains = pipeDrains();
                const auto existing = std::find_if (drains.begin(), drains.end(), [point] (const auto& drain)
                {
                    return drain.position.getDistanceFrom (point) < 2.0f;
                });
                if (existing == drains.end())
                {
                    drains.push_back ({ point });
                    selectedDrain = static_cast<int> (drains.size()) - 1;
                }
                else
                    selectedDrain = static_cast<int> (std::distance (drains.begin(), existing));
                selectedDisc = -1;
                selectedRoute = -1;
                showDrainSettings (selectedDrain);
                notifyChanged();
                repaint();
            }
            return;
        }

        if (tool == Tool::clone)
        {
            {
                const auto point = snapToGrid (worldPosition);
                auto& cloners = pipeCloners();
                const auto existing = std::find_if (cloners.begin(), cloners.end(), [point] (const auto& cloner)
                {
                    return cloner.position.getDistanceFrom (point) < 2.0f;
                });
                if (existing == cloners.end())
                {
                    PipeCloner cloner;
                    cloner.position = point;
                    cloners.push_back (cloner);
                    selectedCloner = static_cast<int> (cloners.size()) - 1;
                }
                else
                    selectedCloner = static_cast<int> (std::distance (cloners.begin(), existing));
                selectedDisc = -1;
                selectedRoute = -1;
                showClonerSettings (selectedCloner);
                notifyChanged();
                repaint();
            }
            return;
        }

        if (tool == Tool::speedLimit)
        {
            {
                const auto point = snapToGrid (worldPosition);
                auto& limits = pipeSpeedLimits();
                const auto existing = std::find_if (limits.begin(), limits.end(), [point] (const auto& limit)
                {
                    return limit.position.getDistanceFrom (point) < 2.0f;
                });
                if (existing == limits.end())
                {
                    limits.push_back ({ point, 1.0 });
                    selectedSpeedLimit = static_cast<int> (limits.size()) - 1;
                }
                else
                    selectedSpeedLimit = static_cast<int> (std::distance (limits.begin(), existing));
                selectedDisc = -1;
                selectedRoute = -1;
                showSpeedLimitSettings (selectedSpeedLimit);
                notifyChanged();
                repaint();
            }
            return;
        }

        if (tool == Tool::wait)
        {
            {
                const auto point = snapToGrid (worldPosition);
                auto& waits = pipeWaits();
                const auto existing = std::find_if (waits.begin(), waits.end(), [point] (const auto& wait)
                {
                    return wait.position.getDistanceFrom (point) < 2.0f;
                });
                if (existing == waits.end())
                {
                    waits.push_back ({ point, 1.0 });
                    selectedWait = static_cast<int> (waits.size()) - 1;
                }
                else
                    selectedWait = static_cast<int> (std::distance (waits.begin(), existing));
                selectedDisc = -1;
                selectedRoute = -1;
                showWaitSettings (selectedWait);
                notifyChanged();
                repaint();
            }
            return;
        }

        if (tool == Tool::strike)
        {
            {
                const auto point = snapToGrid (worldPosition);
                auto& strikes = pipeStrikes();
                const auto existing = std::find_if (strikes.begin(), strikes.end(), [point] (const auto& strike)
                { return strike.position.getDistanceFrom (point) < 2.0f; });
                if (existing == strikes.end())
                {
                    strikes.push_back ({ point });
                    selectedStrike = static_cast<int> (strikes.size()) - 1;
                }
                else selectedStrike = static_cast<int> (std::distance (strikes.begin(), existing));
                selectedDisc = -1; selectedRoute = -1;
                showStrikeSettings (selectedStrike);
                notifyChanged(); repaint();
            }
            return;
        }

        if (tool == Tool::teleport)
        {
            {
                const auto point = snapToGrid (worldPosition); auto& teleports = pipeTeleports();
                const auto existing = std::find_if (teleports.begin(), teleports.end(), [point] (const auto& teleport)
                { return teleport.position.getDistanceFrom (point) < 2.0f; });
                if (existing == teleports.end()) { PipeTeleport teleport; teleport.position = point; teleports.push_back (teleport); selectedTeleport = static_cast<int> (teleports.size()) - 1; }
                else selectedTeleport = static_cast<int> (std::distance (teleports.begin(), existing));
                selectedDisc = -1; selectedRoute = -1; showTeleportSettings (selectedTeleport); notifyChanged(); repaint();
            }
            return;
        }
        if (tool == Tool::filter)
        {
            {
                const auto point = snapToGrid (worldPosition); auto& filters = pipeFilters();
                const auto existing = std::find_if (filters.begin(), filters.end(), [point] (const auto& filter) { return filter.position.getDistanceFrom (point) < 2.0f; });
                if (existing == filters.end()) { filters.push_back ({ point }); selectedFilter = static_cast<int> (filters.size()) - 1; }
                else selectedFilter = static_cast<int> (std::distance (filters.begin(), existing));
                selectedDisc = -1; selectedRoute = -1; showFilterSettings (selectedFilter); notifyChanged(); repaint();
            }
            return;
        }
        if (tool == Tool::logic)
        {
            const auto point = snapToGrid (worldPosition);
            auto& logics = pipeLogics();
            const auto existing = std::find_if (logics.begin(), logics.end(), [point] (const auto& logic)
            { return logic.position.getDistanceFrom (point) < 2.0f; });
            if (existing == logics.end())
            {
                PipeLogic logic;
                logic.position = point;
                logics.push_back (logic);
                selectedLogic = static_cast<int> (logics.size()) - 1;
            }
            else selectedLogic = static_cast<int> (std::distance (logics.begin(), existing));
            selectedDisc = -1;
            selectedRoute = -1;
            showLogicSettings (selectedLogic);
            notifyChanged();
            repaint();
            return;
        }

        if (tool == Tool::edit)
        {
            const auto hit = findNearestNode (worldPosition, screenToleranceToWorld (12.0f));

            if (hit.first >= 0)
            {
                selectedRoute = hit.first;
                selectedNode = hit.second;
                draggingNode = true;
                repaint();
                return;
            }

            const auto segmentHit = findNearestSegment (worldPosition, screenToleranceToWorld (14.0f));

            if (segmentHit.route >= 0)
            {
                auto& route = routes()[static_cast<size_t> (segmentHit.route)];
                const auto insertAt = static_cast<size_t> (segmentHit.insertIndex);
                route.points.insert (route.points.begin() + static_cast<std::ptrdiff_t> (insertAt),
                                     segmentHit.point);
                selectedRoute = segmentHit.route;
                selectedNode = segmentHit.insertIndex;
                draggingNode = true;
                notifyChanged();
                repaint();
                return;
            }

            selectedRoute = -1;
            selectedNode = -1;
            repaint();
            return;
        }

        if (tool == Tool::erase)
        {
            const auto routeIndex = findNearestRoute (worldPosition, screenToleranceToWorld (18.0f));

            if (routeIndex >= 0)
            {
                eraseRouteAndPulses (routeIndex);
                selectedRoute = -1;
                selectedNode = -1;
                selectedDisc = -1;
                notifyChanged();
                notifySelectionChanged();
                repaint();
            }
        }
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (modulationLayerVisible)
            return;
        if (tool != Tool::select)
            return;

        const auto worldPosition = screenToWorld (event.position);

        const auto discDiameter = compactDiscs ? gridSize * 0.55f : gridSize;
        selectedDisc = findDiscAt (worldPosition, screenToleranceToWorld (discDiameter * 0.55f));
        selectedRoute = -1;
        selectedNode = -1;

        if (selectedDisc >= 0)
            requestDiscPanel();

        notifySelectionChanged();
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (panning)
        {
            viewOffset = panStartOffset + (event.position - panStartScreen);
            repaint();
            return;
        }

        const auto worldPosition = screenToWorld (event.position);

        if (modulationLayerVisible)
        {
            if (connectingModulator >= 0)
            {
                modulationDragPoint = worldPosition;
                repaint();
            }
            else if (draggingModulator && juce::isPositiveAndBelow (selectedModulator, static_cast<int> (modulators.size())))
            {
                const auto next = worldPosition + modulatorDragOffset;
                modulators[static_cast<size_t> (selectedModulator)].position = snapEnabled ? snapToGrid (next) : next;
                repaint();
            }
            return;
        }

        if (tool == Tool::select && marqueeSelecting)
        {
            marqueeCurrent = worldPosition;
            repaint();
            return;
        }

        if (tool == Tool::select && draggingSelection)
        {
            const auto current = snapToGrid (worldPosition);
            const auto delta = current - selectionDragLast;
            if (delta.x != 0.0f || delta.y != 0.0f)
            {
                moveSelectedItems (delta);
                selectionDragLast = current;
                selectionWasMoved = true;
                repaint();
            }
            return;
        }

        if ((tool == Tool::draw || tool == Tool::pipe || tool == Tool::warpPipe) && drawing)
        {
            const auto point = snappedPoint (worldPosition);
            appendPoint (point);
            repaint();
            return;
        }

        if (tool == Tool::edit && draggingNode && selectedRoute >= 0 && selectedNode >= 0)
        {
            const auto point = snappedPoint (worldPosition, selectedRoute, selectedNode);
            auto& route = routes()[static_cast<size_t> (selectedRoute)];
            route.points[static_cast<size_t> (selectedNode)] = point;
            notifyChanged();
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (panning)
        {
            panning = false;
            setMouseCursor (juce::MouseCursor::CrosshairCursor);
            return;
        }

        if (modulationLayerVisible)
        {
            if (connectingModulator >= 0 && juce::isPositiveAndBelow (connectingModulator, static_cast<int> (modulators.size())))
            {
                const auto target = findModulationTarget (screenToWorld (event.position), screenToleranceToWorld (20.0f));
                if (target.isValid())
                {
                    const auto sourceId = modulators[static_cast<size_t> (connectingModulator)].id;
                    const auto existing = std::find_if (modulationConnections.begin(), modulationConnections.end(), [&] (const auto& connection)
                    { return connection.sourceId == sourceId && connection.targetKind == target.kind && connection.targetId == target.id; });
                    if (existing == modulationConnections.end())
                    {
                        modulationConnections.push_back ({ sourceId, target.kind, target.id });
                        selectedModulationConnection = static_cast<int> (modulationConnections.size()) - 1;
                        notifyChanged (false);
                    }
                    else
                        selectedModulationConnection = static_cast<int> (std::distance (modulationConnections.begin(), existing));
                    showModulationConnectionSettings (selectedModulationConnection);
                }
                connectingModulator = -1;
                repaint();
                return;
            }
            if (draggingModulator)
            {
                draggingModulator = false;
                notifyChanged (false);
                return;
            }
            return;
        }

        if ((tool == Tool::draw || tool == Tool::pipe || tool == Tool::warpPipe) && drawing)
        {
            drawing = false;
            finishCurrentRoute();
            repaint();
            return;
        }

        if (marqueeSelecting)
        {
            marqueeSelecting = false;
            selectItemsInMarquee();
            repaint();
            return;
        }

        if (draggingSelection)
        {
            draggingSelection = false;
            if (selectionWasMoved) notifyChanged();
            selectionWasMoved = false;
            return;
        }

        draggingNode = false;
    }

    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        if (wheel.isInertial)
            return;

        if (std::abs (wheel.deltaX) > std::abs (wheel.deltaY))
        {
            panBy ({ wheel.deltaX * 180.0f, 0.0f });
            return;
        }

        const auto zoomFactor = std::pow (1.18f, wheel.deltaY * 7.0f);
        zoomAt (event.position, zoomFactor);
    }

    bool toggleSelectedDeviceBypass()
    {
        bool* enabled = nullptr;
        if (juce::isPositiveAndBelow (selectedTap, static_cast<int> (pipeTaps().size()))) enabled = &pipeTaps()[static_cast<size_t> (selectedTap)].enabled;
        else if (juce::isPositiveAndBelow (selectedDrain, static_cast<int> (pipeDrains().size()))) enabled = &pipeDrains()[static_cast<size_t> (selectedDrain)].enabled;
        else if (juce::isPositiveAndBelow (selectedCloner, static_cast<int> (pipeCloners().size()))) enabled = &pipeCloners()[static_cast<size_t> (selectedCloner)].enabled;
        else if (juce::isPositiveAndBelow (selectedSpeedLimit, static_cast<int> (pipeSpeedLimits().size()))) enabled = &pipeSpeedLimits()[static_cast<size_t> (selectedSpeedLimit)].enabled;
        else if (juce::isPositiveAndBelow (selectedWait, static_cast<int> (pipeWaits().size()))) enabled = &pipeWaits()[static_cast<size_t> (selectedWait)].enabled;
        else if (juce::isPositiveAndBelow (selectedStrike, static_cast<int> (pipeStrikes().size()))) enabled = &pipeStrikes()[static_cast<size_t> (selectedStrike)].enabled;
        else if (juce::isPositiveAndBelow (selectedTeleport, static_cast<int> (pipeTeleports().size()))) enabled = &pipeTeleports()[static_cast<size_t> (selectedTeleport)].enabled;
        else if (juce::isPositiveAndBelow (selectedFilter, static_cast<int> (pipeFilters().size()))) enabled = &pipeFilters()[static_cast<size_t> (selectedFilter)].enabled;
        else if (juce::isPositiveAndBelow (selectedLogic, static_cast<int> (pipeLogics().size()))) enabled = &pipeLogics()[static_cast<size_t> (selectedLogic)].enabled;
        if (enabled == nullptr) return false;
        *enabled = ! *enabled;
        notifyChanged(); repaint();
        return true;
    }

    void copySelectedItems() { copySelection(); }
    void pasteSelectedItems() { pasteSelection(); }
    void duplicateSelectedItems() { copySelection(); pasteSelection(); }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
        {
            undo();
            return true;
        }

        if (key == juce::KeyPress ('c', juce::ModifierKeys::commandModifier, 0)) { copySelection(); return true; }
        if (key == juce::KeyPress ('v', juce::ModifierKeys::commandModifier, 0)) { pasteSelection(); return true; }
        if (key == juce::KeyPress ('d', juce::ModifierKeys::commandModifier, 0)) { copySelection(); pasteSelection(); return true; }

        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            if (modulationLayerVisible)
            {
                if (juce::isPositiveAndBelow (selectedModulator, static_cast<int> (modulators.size())))
                {
                    const auto id = modulators[static_cast<size_t> (selectedModulator)].id;
                    modulators.erase (modulators.begin() + selectedModulator);
                    modulationConnections.erase (std::remove_if (modulationConnections.begin(), modulationConnections.end(), [&] (const auto& connection)
                    { return connection.sourceId == id; }), modulationConnections.end());
                    selectedModulator = selectedModulationConnection = -1;
                    notifyChanged (false); repaint(); return true;
                }
                if (juce::isPositiveAndBelow (selectedModulationConnection, static_cast<int> (modulationConnections.size())))
                {
                    modulationConnections.erase (modulationConnections.begin() + selectedModulationConnection);
                    selectedModulationConnection = -1;
                    notifyChanged (false); repaint(); return true;
                }
                return false;
            }
            if (! selectedItems.empty())
            {
                deleteSelectedItems();
                notifyChanged(); notifySelectionChanged(); repaint();
                return true;
            }
            if (juce::isPositiveAndBelow (selectedTap, static_cast<int> (pipeTaps().size())))
            {
                pipeTaps().erase (pipeTaps().begin() + selectedTap);
                selectedTap = -1;
                notifyChanged();
                repaint();
                return true;
            }

            if (juce::isPositiveAndBelow (selectedDrain, static_cast<int> (pipeDrains().size())))
            {
                pipeDrains().erase (pipeDrains().begin() + selectedDrain);
                selectedDrain = -1;
                notifyChanged();
                repaint();
                return true;
            }

            if (juce::isPositiveAndBelow (selectedCloner, static_cast<int> (pipeCloners().size())))
            {
                pipeCloners().erase (pipeCloners().begin() + selectedCloner);
                selectedCloner = -1;
                notifyChanged();
                repaint();
                return true;
            }

            if (juce::isPositiveAndBelow (selectedSpeedLimit, static_cast<int> (pipeSpeedLimits().size())))
            {
                pipeSpeedLimits().erase (pipeSpeedLimits().begin() + selectedSpeedLimit);
                selectedSpeedLimit = -1;
                notifyChanged();
                repaint();
                return true;
            }

            if (juce::isPositiveAndBelow (selectedWait, static_cast<int> (pipeWaits().size())))
            {
                pipeWaits().erase (pipeWaits().begin() + selectedWait);
                selectedWait = -1;
                notifyChanged();
                repaint();
                return true;
            }

            if (juce::isPositiveAndBelow (selectedStrike, static_cast<int> (pipeStrikes().size())))
            {
                pipeStrikes().erase (pipeStrikes().begin() + selectedStrike);
                selectedStrike = -1; notifyChanged(); repaint(); return true;
            }
            if (juce::isPositiveAndBelow (selectedTeleport, static_cast<int> (pipeTeleports().size())))
            {
                const auto removedId = pipeTeleports()[static_cast<size_t> (selectedTeleport)].id;
                pipeTeleports().erase (pipeTeleports().begin() + selectedTeleport);
                for (auto& teleport : pipeTeleports()) if (teleport.destinationId == removedId) teleport.destinationId.clear();
                selectedTeleport = -1; notifyChanged(); repaint(); return true;
            }
            if (juce::isPositiveAndBelow (selectedFilter, static_cast<int> (pipeFilters().size())))
            { pipeFilters().erase (pipeFilters().begin() + selectedFilter); selectedFilter = -1; notifyChanged(); repaint(); return true; }
            if (juce::isPositiveAndBelow (selectedLogic, static_cast<int> (pipeLogics().size())))
            { pipeLogics().erase (pipeLogics().begin() + selectedLogic); selectedLogic = -1; notifyChanged(); repaint(); return true; }

            if (selectedRoute >= 0 && selectedRoute < static_cast<int> (routes().size()))
            {
                eraseRouteAndPulses (selectedRoute);
                selectedRoute = -1;
                selectedNode = -1;
                notifyChanged();
                repaint();
                return true;
            }

            if (selectedDisc >= 0 && selectedDisc < static_cast<int> (discs().size()))
            {
                discs().erase (discs().begin() + selectedDisc);
                selectedDisc = -1;
                notifyChanged();
                notifySelectionChanged();
                repaint();
                return true;
            }
        }

        return false;
    }

    juce::ValueTree createProjectState() const
    {
        juce::ValueTree project ("otherwareProject");
        project.setProperty ("version", 1, nullptr);
        project.addChild (routesToValueTree (rootRoutes), -1, nullptr);
        project.addChild (tapsToValueTree (rootPipeTaps), -1, nullptr);
        project.addChild (drainsToValueTree (rootPipeDrains), -1, nullptr);
        project.addChild (clonersToValueTree (rootPipeCloners), -1, nullptr);
        project.addChild (speedLimitsToValueTree (rootPipeSpeedLimits), -1, nullptr);
        project.addChild (waitsToValueTree (rootPipeWaits), -1, nullptr);
        project.addChild (strikesToValueTree (rootPipeStrikes), -1, nullptr);
        project.addChild (teleportsToValueTree (rootPipeTeleports), -1, nullptr);
        project.addChild (filtersToValueTree (rootPipeFilters), -1, nullptr);
        project.addChild (logicsToValueTree (rootPipeLogics), -1, nullptr);
        project.addChild (clocksToValueTree (sequencingClocks), -1, nullptr);
        project.addChild (modulationToValueTree (modulators, modulationConnections), -1, nullptr);
        project.addChild (discsToValueTree (rootDiscs), -1, nullptr);
        return project;
    }

    bool applyProjectState (const juce::ValueTree& project)
    {
        if (! project.isValid() || ! project.hasType ("otherwareProject"))
            return false;
        std::vector<RoadRoute> newRoutes;
        std::vector<PipeTap> newTaps;
        std::vector<PipeDrain> newDrains;
        std::vector<PipeCloner> newCloners;
        std::vector<PipeSpeedLimit> newSpeedLimits;
        std::vector<PipeWait> newWaits;
        std::vector<PipeStrike> newStrikes;
        std::vector<PipeTeleport> newTeleports;
        std::vector<PipeFilter> newFilters;
        std::vector<PipeLogic> newLogics;
        auto newClocks = defaultSequencingClocks();
        std::vector<Modulator> newModulators;
        std::vector<ModulationConnection> newModulationConnections;
        std::vector<Disc> newDiscs;
        if (! routesFromValueTree (project.getChildWithName ("routes"), newRoutes)
            || ! tapsFromValueTree (project.getChildWithName ("pipeTaps"), newTaps)
            || ! drainsFromValueTree (project.getChildWithName ("pipeDrains"), newDrains)
            || ! clonersFromValueTree (project.getChildWithName ("pipeCloners"), newCloners)
            || ! speedLimitsFromValueTree (project.getChildWithName ("pipeSpeedLimits"), newSpeedLimits)
            || ! waitsFromValueTree (project.getChildWithName ("pipeWaits"), newWaits)
            || ! strikesFromValueTree (project.getChildWithName ("pipeStrikes"), newStrikes)
            || ! teleportsFromValueTree (project.getChildWithName ("pipeTeleports"), newTeleports)
            || ! filtersFromValueTree (project.getChildWithName ("pipeFilters"), newFilters)
            || ! logicsFromValueTree (project.getChildWithName ("pipeLogics"), newLogics)
            || ! clocksFromValueTree (project.getChildWithName ("sequencingClocks"), newClocks)
            || ! modulationFromValueTree (project.getChildWithName ("modulation"), newModulators, newModulationConnections)
            || ! discsFromValueTree (project.getChildWithName ("discs"), newDiscs))
            return false;
        rootRoutes = std::move (newRoutes);
        rootPipeTaps = std::move (newTaps);
        rootPipeDrains = std::move (newDrains);
        rootPipeCloners = std::move (newCloners);
        rootPipeSpeedLimits = std::move (newSpeedLimits);
        rootPipeWaits = std::move (newWaits);
        rootPipeStrikes = std::move (newStrikes);
        rootPipeTeleports = std::move (newTeleports);
        rootPipeFilters = std::move (newFilters);
        rootPipeLogics = std::move (newLogics);
        sequencingClocks = std::move (newClocks);
        modulators = std::move (newModulators);
        modulationConnections = std::move (newModulationConnections);
        rootDiscs = std::move (newDiscs);
        normalisePipeJunctions (rootRoutes);
        normaliseNestedPipeJunctions (rootDiscs);
        worldPath.clear();
        modulationLayerVisible = false;
        resetSelection();
        resetView();
        notifyChanged();
        repaint();
        return true;
    }

private:
    std::vector<RoadRoute> rootRoutes;
    std::vector<PipeTap> rootPipeTaps;
    std::vector<PipeDrain> rootPipeDrains;
    std::vector<PipeCloner> rootPipeCloners;
    std::vector<PipeSpeedLimit> rootPipeSpeedLimits;
    std::vector<PipeWait> rootPipeWaits;
    std::vector<PipeStrike> rootPipeStrikes;
    std::vector<PipeTeleport> rootPipeTeleports;
    std::vector<PipeFilter> rootPipeFilters;
    std::vector<PipeLogic> rootPipeLogics;
    std::vector<Modulator> modulators;
    std::vector<ModulationConnection> modulationConnections;
    ClockBank sequencingClocks { defaultSequencingClocks() };
    std::vector<Disc> rootDiscs;
    static constexpr int selectionStride = 100000;
    static constexpr int selectionRoute = 0, selectionDisc = 1, selectionTap = 2, selectionDrain = 3,
                         selectionCloner = 4, selectionSpeedLimit = 5, selectionWait = 6, selectionStrike = 7,
                         selectionTeleport = 8, selectionFilter = 9, selectionLogic = 10;
    static int selectionKey (int kind, int index) { return kind * selectionStride + index; }

    juce::Point<float>* selectionPosition (int kind, int index)
    {
        if (kind == selectionDisc && juce::isPositiveAndBelow (index, static_cast<int> (discs().size()))) return &discs()[static_cast<size_t> (index)].centre;
        if (kind == selectionTap && juce::isPositiveAndBelow (index, static_cast<int> (pipeTaps().size()))) return &pipeTaps()[static_cast<size_t> (index)].position;
        if (kind == selectionDrain && juce::isPositiveAndBelow (index, static_cast<int> (pipeDrains().size()))) return &pipeDrains()[static_cast<size_t> (index)].position;
        if (kind == selectionCloner && juce::isPositiveAndBelow (index, static_cast<int> (pipeCloners().size()))) return &pipeCloners()[static_cast<size_t> (index)].position;
        if (kind == selectionSpeedLimit && juce::isPositiveAndBelow (index, static_cast<int> (pipeSpeedLimits().size()))) return &pipeSpeedLimits()[static_cast<size_t> (index)].position;
        if (kind == selectionWait && juce::isPositiveAndBelow (index, static_cast<int> (pipeWaits().size()))) return &pipeWaits()[static_cast<size_t> (index)].position;
        if (kind == selectionStrike && juce::isPositiveAndBelow (index, static_cast<int> (pipeStrikes().size()))) return &pipeStrikes()[static_cast<size_t> (index)].position;
        if (kind == selectionTeleport && juce::isPositiveAndBelow (index, static_cast<int> (pipeTeleports().size()))) return &pipeTeleports()[static_cast<size_t> (index)].position;
        if (kind == selectionFilter && juce::isPositiveAndBelow (index, static_cast<int> (pipeFilters().size()))) return &pipeFilters()[static_cast<size_t> (index)].position;
        if (kind == selectionLogic && juce::isPositiveAndBelow (index, static_cast<int> (pipeLogics().size()))) return &pipeLogics()[static_cast<size_t> (index)].position;
        return nullptr;
    }

    void moveSelectedItems (juce::Point<float> delta)
    {
        for (const auto key : selectedItems)
        {
            const auto kind = key / selectionStride, index = key % selectionStride;
            if (kind == selectionRoute && juce::isPositiveAndBelow (index, static_cast<int> (routes().size())))
                for (auto& point : routes()[static_cast<size_t> (index)].points) point += delta;
            else if (auto* position = selectionPosition (kind, index)) *position += delta;
        }
    }

    void deleteSelectedItems()
    {
        const auto eraseKind = [this] (int kind, auto& items)
        {
            std::vector<int> indices;
            for (const auto key : selectedItems) if (key / selectionStride == kind) indices.push_back (key % selectionStride);
            std::sort (indices.rbegin(), indices.rend());
            for (const auto index : indices) if (juce::isPositiveAndBelow (index, static_cast<int> (items.size()))) items.erase (items.begin() + index);
        };
        std::vector<int> routeIndices;
        for (const auto key : selectedItems) if (key / selectionStride == selectionRoute) routeIndices.push_back (key % selectionStride);
        std::sort (routeIndices.rbegin(), routeIndices.rend());
        for (const auto index : routeIndices) eraseRouteAndPulses (index);
        eraseKind (selectionDisc, discs()); eraseKind (selectionTap, pipeTaps());
        eraseKind (selectionDrain, pipeDrains()); eraseKind (selectionCloner, pipeCloners()); eraseKind (selectionSpeedLimit, pipeSpeedLimits());
        eraseKind (selectionWait, pipeWaits()); eraseKind (selectionStrike, pipeStrikes()); eraseKind (selectionTeleport, pipeTeleports());
        eraseKind (selectionFilter, pipeFilters());
        eraseKind (selectionLogic, pipeLogics());
        selectedItems.clear();
        selectedRoute = selectedDisc = selectedTap = selectedDrain = selectedCloner = selectedSpeedLimit = selectedWait = selectedStrike = selectedTeleport = selectedFilter = selectedLogic = -1;
    }

    void copySelection()
    {
        clipboardRoutes.clear(); clipboardDiscs.clear(); clipboardTaps.clear(); clipboardDrains.clear(); clipboardCloners.clear();
        clipboardSpeedLimits.clear(); clipboardWaits.clear(); clipboardStrikes.clear(); clipboardTeleports.clear(); clipboardFilters.clear(); clipboardLogics.clear();
        for (const auto key : selectedItems)
        {
            const auto kind = key / selectionStride, index = key % selectionStride;
            if (kind == selectionRoute && juce::isPositiveAndBelow (index, static_cast<int> (routes().size()))) clipboardRoutes.push_back (routes()[static_cast<size_t> (index)]);
            else if (kind == selectionDisc && juce::isPositiveAndBelow (index, static_cast<int> (discs().size()))) clipboardDiscs.push_back (discs()[static_cast<size_t> (index)]);
            else if (kind == selectionTap && juce::isPositiveAndBelow (index, static_cast<int> (pipeTaps().size()))) clipboardTaps.push_back (pipeTaps()[static_cast<size_t> (index)]);
            else if (kind == selectionDrain && juce::isPositiveAndBelow (index, static_cast<int> (pipeDrains().size()))) clipboardDrains.push_back (pipeDrains()[static_cast<size_t> (index)]);
            else if (kind == selectionCloner && juce::isPositiveAndBelow (index, static_cast<int> (pipeCloners().size()))) clipboardCloners.push_back (pipeCloners()[static_cast<size_t> (index)]);
            else if (kind == selectionSpeedLimit && juce::isPositiveAndBelow (index, static_cast<int> (pipeSpeedLimits().size()))) clipboardSpeedLimits.push_back (pipeSpeedLimits()[static_cast<size_t> (index)]);
            else if (kind == selectionWait && juce::isPositiveAndBelow (index, static_cast<int> (pipeWaits().size()))) clipboardWaits.push_back (pipeWaits()[static_cast<size_t> (index)]);
            else if (kind == selectionStrike && juce::isPositiveAndBelow (index, static_cast<int> (pipeStrikes().size()))) clipboardStrikes.push_back (pipeStrikes()[static_cast<size_t> (index)]);
            else if (kind == selectionTeleport && juce::isPositiveAndBelow (index, static_cast<int> (pipeTeleports().size()))) clipboardTeleports.push_back (pipeTeleports()[static_cast<size_t> (index)]);
            else if (kind == selectionFilter && juce::isPositiveAndBelow (index, static_cast<int> (pipeFilters().size()))) clipboardFilters.push_back (pipeFilters()[static_cast<size_t> (index)]);
            else if (kind == selectionLogic && juce::isPositiveAndBelow (index, static_cast<int> (pipeLogics().size()))) clipboardLogics.push_back (pipeLogics()[static_cast<size_t> (index)]);
        }
    }

    void pasteSelection()
    {
        selectedItems.clear();
        const juce::Point<float> offset { gridSize, gridSize };
        for (auto item : clipboardRoutes) { for (auto& point : item.points) point += offset; routes().push_back (item); selectedItems.insert (selectionKey (selectionRoute, static_cast<int> (routes().size()) - 1)); }
        const auto pastePositioned = [this, offset] (auto& clipboard, auto& destination, int kind)
        {
            for (auto item : clipboard) { item.position += offset; item.id = juce::Uuid().toString(); destination.push_back (item); selectedItems.insert (selectionKey (kind, static_cast<int> (destination.size()) - 1)); }
        };
        for (auto item : clipboardDiscs) { item.centre += offset; item.id = juce::Uuid().toString(); discs().push_back (item); selectedItems.insert (selectionKey (selectionDisc, static_cast<int> (discs().size()) - 1)); }
        pastePositioned (clipboardTaps, pipeTaps(), selectionTap); pastePositioned (clipboardDrains, pipeDrains(), selectionDrain);
        pastePositioned (clipboardCloners, pipeCloners(), selectionCloner); pastePositioned (clipboardSpeedLimits, pipeSpeedLimits(), selectionSpeedLimit);
        pastePositioned (clipboardWaits, pipeWaits(), selectionWait); pastePositioned (clipboardStrikes, pipeStrikes(), selectionStrike);
        for (auto item : clipboardTeleports) { item.position += offset; item.id = juce::Uuid().toString(); item.destinationId.clear(); pipeTeleports().push_back (item); selectedItems.insert (selectionKey (selectionTeleport, static_cast<int> (pipeTeleports().size()) - 1)); }
        pastePositioned (clipboardFilters, pipeFilters(), selectionFilter);
        for (auto item : clipboardLogics)
        {
            item.position += offset;
            item.id = juce::Uuid().toString();
            item.count = 0;
            item.flipState = false;
            pipeLogics().push_back (item);
            selectedItems.insert (selectionKey (selectionLogic, static_cast<int> (pipeLogics().size()) - 1));
        }
        if (! selectedItems.empty()) { notifyChanged(); repaint(); }
    }

    void selectItemsInMarquee()
    {
        auto area = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (marqueeStart.x, marqueeCurrent.x),
                                                                juce::jmin (marqueeStart.y, marqueeCurrent.y),
                                                                juce::jmax (marqueeStart.x, marqueeCurrent.x),
                                                                juce::jmax (marqueeStart.y, marqueeCurrent.y));
        for (int i = 0; i < static_cast<int> (routes().size()); ++i)
            if (std::any_of (routes()[static_cast<size_t> (i)].points.begin(), routes()[static_cast<size_t> (i)].points.end(),
                             [&area] (auto point) { return area.contains (point); })) selectedItems.insert (selectionKey (selectionRoute, i));
        const auto addPositions = [this, &area] (int kind, int count)
        {
            for (int i = 0; i < count; ++i) if (const auto* position = selectionPosition (kind, i); position != nullptr && area.contains (*position))
                selectedItems.insert (selectionKey (kind, i));
        };
        addPositions (selectionDisc, static_cast<int> (discs().size()));
        addPositions (selectionTap, static_cast<int> (pipeTaps().size())); addPositions (selectionDrain, static_cast<int> (pipeDrains().size()));
        addPositions (selectionCloner, static_cast<int> (pipeCloners().size())); addPositions (selectionSpeedLimit, static_cast<int> (pipeSpeedLimits().size()));
        addPositions (selectionWait, static_cast<int> (pipeWaits().size())); addPositions (selectionStrike, static_cast<int> (pipeStrikes().size()));
        addPositions (selectionTeleport, static_cast<int> (pipeTeleports().size())); addPositions (selectionFilter, static_cast<int> (pipeFilters().size()));
        addPositions (selectionLogic, static_cast<int> (pipeLogics().size()));
    }

    void drawSelectionOverlay (juce::Graphics& g) const
    {
        g.setColour (juce::Colour (0xff2997ff).withAlpha (0.92f));
        for (const auto key : selectedItems)
        {
            const auto kind = key / selectionStride, index = key % selectionStride;
            if (kind == selectionRoute && juce::isPositiveAndBelow (index, static_cast<int> (routes().size())))
            {
                juce::Path path; const auto& points = routes()[static_cast<size_t> (index)].points;
                if (! points.empty()) { path.startNewSubPath (points.front()); for (size_t i = 1; i < points.size(); ++i) path.lineTo (points[i]); g.strokePath (path, juce::PathStrokeType (2.0f)); }
            }
            else
            {
                auto* self = const_cast<RoadCanvas*> (this);
                if (const auto* position = self->selectionPosition (kind, index)) g.drawEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (*position), 1.5f);
            }
        }
        if (marqueeSelecting)
        {
            const auto area = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (marqueeStart.x, marqueeCurrent.x), juce::jmin (marqueeStart.y, marqueeCurrent.y),
                                                                          juce::jmax (marqueeStart.x, marqueeCurrent.x), juce::jmax (marqueeStart.y, marqueeCurrent.y));
            g.setColour (juce::Colour (0xff2997ff).withAlpha (0.12f)); g.fillRect (area);
            g.setColour (juce::Colour (0xff2997ff).withAlpha (0.85f)); g.drawRect (area, 1.0f);
        }
    }

    std::vector<int> worldPath;
    RoadRoute currentRoute;
    Tool tool = Tool::select;
    bool snapEnabled = true;
    bool orbitElementsDimmed = false;
    bool compactDiscs = false;
    bool flowDebugVisible = false;
    bool modulationLayerVisible = false;
    bool restoringHistory = false;
    juce::ValueTree lastCommittedState;
    std::vector<juce::ValueTree> undoStates;
    bool drawing = false;
    bool draggingNode = false;
    bool panning = false;
    int selectedRoute = -1;
    int selectedNode = -1;
    int selectedDisc = -1;
    int selectedTap = -1;
    int selectedDrain = -1;
    int selectedCloner = -1;
    int selectedSpeedLimit = -1;
    int selectedWait = -1;
    int selectedStrike = -1;
    int selectedTeleport = -1;
    int selectedFilter = -1;
    int selectedLogic = -1;
    int selectedModulator = -1;
    int selectedModulationConnection = -1;
    int connectingModulator = -1;
    juce::Point<float> modulationDragPoint;
    bool draggingModulator = false;
    juce::Point<float> modulatorDragOffset;
    int selectedFlowPulse = -1;
    std::set<int> selectedItems;
    bool marqueeSelecting = false, draggingSelection = false, selectionWasMoved = false;
    juce::Point<float> marqueeStart, marqueeCurrent, selectionDragLast;
    std::vector<RoadRoute> clipboardRoutes; std::vector<Disc> clipboardDiscs; std::vector<PipeTap> clipboardTaps;
    std::vector<PipeDrain> clipboardDrains; std::vector<PipeCloner> clipboardCloners; std::vector<PipeSpeedLimit> clipboardSpeedLimits;
    std::vector<PipeWait> clipboardWaits; std::vector<PipeStrike> clipboardStrikes; std::vector<PipeTeleport> clipboardTeleports;
    std::vector<PipeFilter> clipboardFilters;
    std::vector<PipeLogic> clipboardLogics;
    float viewScale = 1.0f;
    juce::Point<float> viewOffset { 0.0f, 0.0f };
    juce::Point<float> panStartScreen;
    juce::Point<float> panStartOffset;
    struct FlowPulse { int routeIndex = -1; float distance = 0.0f; int lastDisc = -1; double speed = 1.0; double probability = 1.0; bool reverse = false; int lastDrain = -1; int lastCloner = -1; int lastSpeedLimit = -1; int lastWait = -1; double waitBeatsRemaining = 0.0; int lastStrike = -1; int lastTeleport = -1; int lastFilter = -1; int lastLogic = -1; int heldByDisc = -1; };
    std::vector<FlowPulse> flowPulses;
    struct FlowDebugEvent { juce::Point<float> position; juce::String text; double expiresMs = 0.0; };
    std::vector<FlowDebugEvent> flowDebugEvents;
    std::map<juce::String, double> discFlashUntil;
    bool flowRunning = false;
    double flowBpm = 120.0;
    double flowBeatsPerGridUnit = 1.0;
    double lastFlowTimeMs = 0.0;
    double flowBeatPosition = 0.0;

    juce::String discKeyForCurrentWorld (int discIndex) const
    {
        juce::String key;
        for (const auto index : worldPath) key << index << "/";
        key << discIndex;
        return key;
    }

    static juce::ValueTree routesToValueTree (const std::vector<RoadRoute>& source)
    {
        juce::ValueTree routesTree ("routes");
        for (const auto& route : source)
        {
            juce::ValueTree routeTree ("route");
            routeTree.setProperty ("colour", route.colour.toString(), nullptr);
            routeTree.setProperty ("pipe", route.isPipe, nullptr);
            routeTree.setProperty ("warpPipe", route.isWarpPipe, nullptr);
            for (const auto& point : route.points)
            {
                juce::ValueTree pointTree ("point");
                pointTree.setProperty ("x", point.x, nullptr);
                pointTree.setProperty ("y", point.y, nullptr);
                routeTree.addChild (pointTree, -1, nullptr);
            }
            routesTree.addChild (routeTree, -1, nullptr);
        }
        return routesTree;
    }

    static juce::ValueTree tapsToValueTree (const std::vector<PipeTap>& source)
    {
        juce::ValueTree tree ("pipeTaps");
        for (const auto& tap : source)
        {
            juce::ValueTree child ("tap");
            child.setProperty ("x", tap.position.x, nullptr); child.setProperty ("y", tap.position.y, nullptr);
            child.setProperty ("interval", tap.intervalBeats, nullptr); child.setProperty ("speed", tap.speed, nullptr);
            child.setProperty ("randomSpeed", tap.randomSpeed, nullptr); child.setProperty ("speedLow", tap.speedLow, nullptr);
            child.setProperty ("speedHigh", tap.speedHigh, nullptr); child.setProperty ("probability", tap.probability, nullptr);
            child.setProperty ("reverse", tap.reverse, nullptr); child.setProperty ("enabled", tap.enabled, nullptr);
            child.setProperty ("totalDrops", tap.totalDrops, nullptr); child.setProperty ("randomInterval", tap.randomInterval, nullptr);
            child.setProperty ("intervalLow", tap.intervalLowBeats, nullptr); child.setProperty ("intervalHigh", tap.intervalHighBeats, nullptr);
            child.setProperty ("clock", tap.clockIndex, nullptr);
            child.setProperty ("id", tap.id, nullptr);
            tree.addChild (child, -1, nullptr);
        }
        return tree;
    }

    static bool tapsFromValueTree (const juce::ValueTree& tree, std::vector<PipeTap>& destination)
    {
        if (! tree.isValid()) return true;
        if (! tree.hasType ("pipeTaps")) return false;
        for (const auto& child : tree) if (child.hasType ("tap"))
        {
            PipeTap tap;
            tap.position = { static_cast<float> (child["x"]), static_cast<float> (child["y"]) };
            tap.intervalBeats = static_cast<double> (child.getProperty ("interval", 1.0));
            tap.speed = static_cast<double> (child.getProperty ("speed", 1.0));
            tap.randomSpeed = static_cast<bool> (child.getProperty ("randomSpeed", false));
            tap.speedLow = juce::jlimit (0.25, 4.0, static_cast<double> (child.getProperty ("speedLow", 0.5)));
            tap.speedHigh = juce::jlimit (tap.speedLow, 4.0, static_cast<double> (child.getProperty ("speedHigh", 1.5)));
            tap.probability = static_cast<double> (child.getProperty ("probability", 1.0));
            tap.reverse = static_cast<bool> (child.getProperty ("reverse", false));
            tap.enabled = static_cast<bool> (child.getProperty ("enabled", true));
            tap.totalDrops = juce::jlimit (0, 99, static_cast<int> (child.getProperty ("totalDrops", 0)));
            tap.randomInterval = static_cast<bool> (child.getProperty ("randomInterval", false));
            tap.intervalLowBeats = juce::jlimit (0.25, 32.0, static_cast<double> (child.getProperty ("intervalLow", 0.5)));
            tap.intervalHighBeats = juce::jlimit (tap.intervalLowBeats, 32.0, static_cast<double> (child.getProperty ("intervalHigh", 2.0)));
            tap.clockIndex = juce::jlimit (0, 4, static_cast<int> (child.getProperty ("clock", 0)));
            tap.id = child.getProperty ("id", tap.id).toString();
            destination.push_back (tap);
        }
        return true;
    }

    static juce::ValueTree drainsToValueTree (const std::vector<PipeDrain>& source)
    {
        juce::ValueTree tree ("pipeDrains");
        for (const auto& drain : source)
        {
            juce::ValueTree child ("drain");
            child.setProperty ("x", drain.position.x, nullptr);
            child.setProperty ("y", drain.position.y, nullptr);
            child.setProperty ("probability", drain.destructionProbability, nullptr);
            child.setProperty ("enabled", drain.enabled, nullptr);
            child.setProperty ("id", drain.id, nullptr);
            tree.addChild (child, -1, nullptr);
        }
        return tree;
    }

    static bool drainsFromValueTree (const juce::ValueTree& tree, std::vector<PipeDrain>& destination)
    {
        if (! tree.isValid()) return true;
        if (! tree.hasType ("pipeDrains")) return false;
        for (const auto& child : tree)
            if (child.hasType ("drain"))
            { PipeDrain value; value.position = { static_cast<float> (child["x"]), static_cast<float> (child["y"]) }; value.destructionProbability = static_cast<double> (child.getProperty ("probability", 0.5)); value.enabled = static_cast<bool> (child.getProperty ("enabled", true)); value.id = child.getProperty ("id", value.id).toString(); destination.push_back (value); }
        return true;
    }

    static juce::ValueTree clonersToValueTree (const std::vector<PipeCloner>& source)
    {
        juce::ValueTree tree ("pipeCloners");
        for (const auto& cloner : source)
        {
            juce::ValueTree child ("cloner");
            child.setProperty ("x", cloner.position.x, nullptr);
            child.setProperty ("y", cloner.position.y, nullptr);
            child.setProperty ("straight", static_cast<int> (cloner.straightMode), nullptr);
            child.setProperty ("two", static_cast<int> (cloner.twoWayMode), nullptr);
            child.setProperty ("three", static_cast<int> (cloner.threeWayMode), nullptr);
            child.setProperty ("four", static_cast<int> (cloner.fourWayMode), nullptr);
            child.setProperty ("maxClones", cloner.maxClones, nullptr);
            child.setProperty ("cloneProbability", cloner.cloneProbability, nullptr);
            child.setProperty ("enabled", cloner.enabled, nullptr);
            child.setProperty ("id", cloner.id, nullptr);
            tree.addChild (child, -1, nullptr);
        }
        return tree;
    }

    static bool clonersFromValueTree (const juce::ValueTree& tree, std::vector<PipeCloner>& destination)
    {
        if (! tree.isValid()) return true;
        if (! tree.hasType ("pipeCloners")) return false;
        for (const auto& child : tree)
            if (child.hasType ("cloner"))
            {
                PipeCloner cloner;
                cloner.position = { static_cast<float> (child["x"]), static_cast<float> (child["y"]) };
                const auto mode = [&child] (const char* name)
                {
                    return static_cast<PipeCloner::DirectionMode> (juce::jlimit (0, 4, static_cast<int> (child.getProperty (name, 0))));
                };
                cloner.straightMode = mode ("straight");
                cloner.twoWayMode = mode ("two");
                cloner.threeWayMode = mode ("three");
                cloner.fourWayMode = mode ("four");
                cloner.maxClones = juce::jlimit (1, 16, static_cast<int> (child.getProperty ("maxClones", 4)));
                cloner.cloneProbability = juce::jlimit (0.0, 1.0, static_cast<double> (child.getProperty ("cloneProbability", 1.0)));
                cloner.enabled = static_cast<bool> (child.getProperty ("enabled", true));
                cloner.id = child.getProperty ("id", cloner.id).toString();
                destination.push_back (cloner);
            }
        return true;
    }

    static juce::ValueTree speedLimitsToValueTree (const std::vector<PipeSpeedLimit>& source)
    {
        juce::ValueTree tree ("pipeSpeedLimits");
        for (const auto& limit : source)
        {
            juce::ValueTree child ("speedLimit");
            child.setProperty ("x", limit.position.x, nullptr);
            child.setProperty ("y", limit.position.y, nullptr);
            child.setProperty ("multiplier", limit.bpmMultiplier, nullptr);
            child.setProperty ("affectProbability", limit.affectProbability, nullptr);
            child.setProperty ("enabled", limit.enabled, nullptr);
            child.setProperty ("id", limit.id, nullptr);
            tree.addChild (child, -1, nullptr);
        }
        return tree;
    }

    static bool speedLimitsFromValueTree (const juce::ValueTree& tree, std::vector<PipeSpeedLimit>& destination)
    {
        if (! tree.isValid()) return true;
        if (! tree.hasType ("pipeSpeedLimits")) return false;
        for (const auto& child : tree)
        {
            if (child.hasType ("speedLimit"))
            {
                destination.push_back ({ { static_cast<float> (child["x"]), static_cast<float> (child["y"]) },
                                         juce::jlimit (0.125, 4.0, static_cast<double> (child.getProperty ("multiplier", 1.0))),
                                         juce::jlimit (0.0, 1.0, static_cast<double> (child.getProperty ("affectProbability", 1.0))),
                                         static_cast<bool> (child.getProperty ("enabled", true)) });
                destination.back().id = child.getProperty ("id", destination.back().id).toString();
            }
        }
        return true;
    }

    static juce::ValueTree waitsToValueTree (const std::vector<PipeWait>& source)
    {
        juce::ValueTree tree ("pipeWaits");
        for (const auto& wait : source)
        {
            juce::ValueTree child ("wait");
            child.setProperty ("x", wait.position.x, nullptr);
            child.setProperty ("y", wait.position.y, nullptr);
            child.setProperty ("beats", wait.beats, nullptr);
            child.setProperty ("enabled", wait.enabled, nullptr);
            child.setProperty ("id", wait.id, nullptr);
            tree.addChild (child, -1, nullptr);
        }
        return tree;
    }

    static bool waitsFromValueTree (const juce::ValueTree& tree, std::vector<PipeWait>& destination)
    {
        if (! tree.isValid()) return true;
        if (! tree.hasType ("pipeWaits")) return false;
        for (const auto& child : tree)
        {
            if (child.hasType ("wait"))
            {
                destination.push_back ({ { static_cast<float> (child["x"]), static_cast<float> (child["y"]) },
                                         juce::jlimit (0.25, 32.0, static_cast<double> (child.getProperty ("beats", 1.0))),
                                         static_cast<bool> (child.getProperty ("enabled", true)) });
                destination.back().id = child.getProperty ("id", destination.back().id).toString();
            }
        }
        return true;
    }

    static juce::ValueTree strikesToValueTree (const std::vector<PipeStrike>& source)
    {
        juce::ValueTree tree ("pipeStrikes");
        for (const auto& strike : source)
        {
            juce::ValueTree child ("strike");
            child.setProperty ("x", strike.position.x, nullptr); child.setProperty ("y", strike.position.y, nullptr);
            child.setProperty ("max", strike.maxDiscs, nullptr); child.setProperty ("left", strike.left, nullptr);
            child.setProperty ("right", strike.right, nullptr); child.setProperty ("up", strike.up, nullptr);
            child.setProperty ("down", strike.down, nullptr);
            child.setProperty ("enabled", strike.enabled, nullptr);
            child.setProperty ("id", strike.id, nullptr);
            tree.addChild (child, -1, nullptr);
        }
        return tree;
    }

    static bool strikesFromValueTree (const juce::ValueTree& tree, std::vector<PipeStrike>& destination)
    {
        if (! tree.isValid()) return true;
        if (! tree.hasType ("pipeStrikes")) return false;
        for (const auto& child : tree)
        {
            if (! child.hasType ("strike")) continue;
            destination.push_back ({ { static_cast<float> (child["x"]), static_cast<float> (child["y"]) },
                                     juce::jlimit (1, 4, static_cast<int> (child.getProperty ("max", 4))),
                                     static_cast<bool> (child.getProperty ("left", true)),
                                     static_cast<bool> (child.getProperty ("right", true)),
                                     static_cast<bool> (child.getProperty ("up", true)),
                                     static_cast<bool> (child.getProperty ("down", true)),
                                     static_cast<bool> (child.getProperty ("enabled", true)) });
            destination.back().id = child.getProperty ("id", destination.back().id).toString();
        }
        return true;
    }

    static juce::ValueTree teleportsToValueTree (const std::vector<PipeTeleport>& source)
    {
        juce::ValueTree tree ("pipeTeleports");
        for (const auto& teleport : source)
        {
            juce::ValueTree child ("teleport"); child.setProperty ("x", teleport.position.x, nullptr); child.setProperty ("y", teleport.position.y, nullptr);
            child.setProperty ("id", teleport.id, nullptr); child.setProperty ("destination", teleport.destinationId, nullptr);
            child.setProperty ("probability", teleport.probability, nullptr); child.setProperty ("maxPerWindow", teleport.maxPerWindow, nullptr);
            child.setProperty ("windowBars", teleport.windowBars, nullptr); child.setProperty ("stopAfter", teleport.stopAfter, nullptr);
            child.setProperty ("enabled", teleport.enabled, nullptr);
            tree.addChild (child, -1, nullptr);
        }
        return tree;
    }

    static bool teleportsFromValueTree (const juce::ValueTree& tree, std::vector<PipeTeleport>& destination)
    {
        if (! tree.isValid()) return true; if (! tree.hasType ("pipeTeleports")) return false;
        for (const auto& child : tree) if (child.hasType ("teleport"))
        {
            PipeTeleport teleport; teleport.position = { static_cast<float> (child["x"]), static_cast<float> (child["y"]) };
            teleport.id = child.getProperty ("id", juce::Uuid().toString()).toString(); teleport.destinationId = child["destination"].toString();
            teleport.probability = juce::jlimit (0.0, 1.0, static_cast<double> (child.getProperty ("probability", 1.0)));
            teleport.maxPerWindow = juce::jlimit (1, 64, static_cast<int> (child.getProperty ("maxPerWindow", 4)));
            teleport.windowBars = juce::jlimit (0.25, 16.0, static_cast<double> (child.getProperty ("windowBars", 1.0)));
            teleport.stopAfter = juce::jlimit (0, 1000, static_cast<int> (child.getProperty ("stopAfter", 0))); destination.push_back (teleport);
            destination.back().enabled = static_cast<bool> (child.getProperty ("enabled", true));
        }
        return true;
    }

    static juce::ValueTree filtersToValueTree (const std::vector<PipeFilter>& source)
    {
        juce::ValueTree tree ("pipeFilters");
        for (const auto& filter : source) { juce::ValueTree child ("filter"); child.setProperty ("x", filter.position.x, nullptr); child.setProperty ("y", filter.position.y, nullptr); child.setProperty ("mode", static_cast<int> (filter.mode), nullptr); child.setProperty ("low", filter.lowSpeed, nullptr); child.setProperty ("high", filter.highSpeed, nullptr); child.setProperty ("enabled", filter.enabled, nullptr); child.setProperty ("id", filter.id, nullptr); tree.addChild (child, -1, nullptr); }
        return tree;
    }
    static bool filtersFromValueTree (const juce::ValueTree& tree, std::vector<PipeFilter>& destination)
    {
        if (! tree.isValid()) return true; if (! tree.hasType ("pipeFilters")) return false;
        for (const auto& child : tree) if (child.hasType ("filter")) { PipeFilter filter; filter.position = { static_cast<float> (child["x"]), static_cast<float> (child["y"]) }; filter.mode = static_cast<PipeFilter::Mode> (juce::jlimit (0, 2, static_cast<int> (child.getProperty ("mode", 1)))); filter.lowSpeed = juce::jlimit (0.125, 4.0, static_cast<double> (child.getProperty ("low", 0.5))); filter.highSpeed = juce::jlimit (filter.lowSpeed, 4.0, static_cast<double> (child.getProperty ("high", 1.5))); filter.enabled = static_cast<bool> (child.getProperty ("enabled", true)); filter.id = child.getProperty ("id", filter.id).toString(); destination.push_back (filter); }
        return true;
    }

    static juce::ValueTree logicsToValueTree (const std::vector<PipeLogic>& source)
    {
        juce::ValueTree tree ("pipeLogics");
        for (const auto& logic : source)
        {
            juce::ValueTree child ("logic");
            child.setProperty ("x", logic.position.x, nullptr);
            child.setProperty ("y", logic.position.y, nullptr);
            child.setProperty ("mode", static_cast<int> (logic.mode), nullptr);
            child.setProperty ("comparison", static_cast<int> (logic.comparison), nullptr);
            child.setProperty ("branch", static_cast<int> (logic.branch), nullptr);
            child.setProperty ("target", logic.targetCount, nullptr);
            child.setProperty ("speed", logic.compareSpeed, nullptr);
            child.setProperty ("open", logic.gateOpen, nullptr);
            child.setProperty ("enabled", logic.enabled, nullptr);
            child.setProperty ("id", logic.id, nullptr);
            tree.addChild (child, -1, nullptr);
        }
        return tree;
    }

    static bool logicsFromValueTree (const juce::ValueTree& tree, std::vector<PipeLogic>& destination)
    {
        if (! tree.isValid()) return true;
        if (! tree.hasType ("pipeLogics")) return false;
        for (const auto& child : tree)
        {
            if (! child.hasType ("logic")) continue;
            PipeLogic logic;
            logic.position = { static_cast<float> (child["x"]), static_cast<float> (child["y"]) };
            logic.mode = static_cast<PipeLogic::Mode> (juce::jlimit (0, 5, static_cast<int> (child.getProperty ("mode", 0))));
            logic.comparison = static_cast<PipeLogic::Comparison> (juce::jlimit (0, 4, static_cast<int> (child.getProperty ("comparison", 3))));
            logic.branch = static_cast<PipeLogic::Branch> (juce::jlimit (0, 3, static_cast<int> (child.getProperty ("branch", 1))));
            logic.targetCount = juce::jlimit (1, 64, static_cast<int> (child.getProperty ("target", 4)));
            logic.compareSpeed = juce::jlimit (0.125, 4.0, static_cast<double> (child.getProperty ("speed", 1.0)));
            logic.gateOpen = static_cast<bool> (child.getProperty ("open", true));
            logic.enabled = static_cast<bool> (child.getProperty ("enabled", true));
            logic.id = child.getProperty ("id", logic.id).toString();
            destination.push_back (logic);
        }
        return true;
    }

    static juce::ValueTree clocksToValueTree (const ClockBank& source)
    {
        juce::ValueTree tree ("sequencingClocks");
        for (int i = 0; i < static_cast<int> (source.size()); ++i)
        {
            const auto& clock = source[static_cast<size_t> (i)];
            juce::ValueTree child ("clock");
            child.setProperty ("index", i, nullptr);
            child.setProperty ("name", clock.name, nullptr);
            child.setProperty ("ratio", clock.ratio, nullptr);
            child.setProperty ("phase", clock.phaseBeats, nullptr);
            child.setProperty ("swing", clock.swing, nullptr);
            child.setProperty ("enabled", clock.enabled, nullptr);
            tree.addChild (child, -1, nullptr);
        }
        return tree;
    }

    static bool clocksFromValueTree (const juce::ValueTree& tree, ClockBank& destination)
    {
        if (! tree.isValid()) return true;
        if (! tree.hasType ("sequencingClocks")) return false;
        for (const auto& child : tree)
        {
            if (! child.hasType ("clock")) continue;
            const auto index = juce::jlimit (0, 3, static_cast<int> (child.getProperty ("index", 0)));
            auto& clock = destination[static_cast<size_t> (index)];
            clock.name = child.getProperty ("name", clock.name).toString();
            clock.ratio = juce::jlimit (0.125, 8.0, static_cast<double> (child.getProperty ("ratio", clock.ratio)));
            clock.phaseBeats = juce::jlimit (0.0, 16.0, static_cast<double> (child.getProperty ("phase", clock.phaseBeats)));
            clock.swing = juce::jlimit (0.0, 0.75, static_cast<double> (child.getProperty ("swing", clock.swing)));
            clock.enabled = static_cast<bool> (child.getProperty ("enabled", clock.enabled));
        }
        return true;
    }

    static juce::ValueTree modulationToValueTree (const std::vector<Modulator>& sources,
                                                  const std::vector<ModulationConnection>& connections)
    {
        juce::ValueTree tree ("modulation");
        for (const auto& source : sources)
        {
            juce::ValueTree child ("source");
            child.setProperty ("id", source.id, nullptr);
            child.setProperty ("x", source.position.x, nullptr);
            child.setProperty ("y", source.position.y, nullptr);
            child.setProperty ("name", source.name, nullptr);
            child.setProperty ("shape", static_cast<int> (source.shape), nullptr);
            child.setProperty ("cycle", source.cycleBeats, nullptr);
            child.setProperty ("phase", source.phase, nullptr);
            child.setProperty ("enabled", source.enabled, nullptr);
            tree.addChild (child, -1, nullptr);
        }
        for (const auto& connection : connections)
        {
            juce::ValueTree child ("connection");
            child.setProperty ("source", connection.sourceId, nullptr);
            child.setProperty ("targetKind", static_cast<int> (connection.targetKind), nullptr);
            child.setProperty ("target", connection.targetId, nullptr);
            child.setProperty ("parameter", connection.parameter, nullptr);
            child.setProperty ("depth", connection.depth, nullptr);
            child.setProperty ("offset", connection.offset, nullptr);
            child.setProperty ("inverted", connection.inverted, nullptr);
            tree.addChild (child, -1, nullptr);
        }
        return tree;
    }

    static bool modulationFromValueTree (const juce::ValueTree& tree,
                                         std::vector<Modulator>& sources,
                                         std::vector<ModulationConnection>& connections)
    {
        if (! tree.isValid()) return true;
        if (! tree.hasType ("modulation")) return false;
        for (const auto& child : tree)
        {
            if (child.hasType ("source"))
            {
                Modulator source;
                source.id = child.getProperty ("id", source.id).toString();
                source.position = { static_cast<float> (child["x"]), static_cast<float> (child["y"]) };
                source.name = child.getProperty ("name", "Modulator").toString();
                source.shape = static_cast<Modulator::Shape> (juce::jlimit (0, 3, static_cast<int> (child.getProperty ("shape", 0))));
                source.cycleBeats = juce::jlimit (0.125, 64.0, static_cast<double> (child.getProperty ("cycle", 4.0)));
                source.phase = juce::jlimit (0.0, 1.0, static_cast<double> (child.getProperty ("phase", 0.0)));
                source.enabled = static_cast<bool> (child.getProperty ("enabled", true));
                sources.push_back (std::move (source));
            }
            else if (child.hasType ("connection"))
            {
                ModulationConnection connection;
                connection.sourceId = child["source"].toString();
                connection.targetKind = static_cast<ModulationTargetKind> (
                    juce::jlimit (0, 9, static_cast<int> (child.getProperty ("targetKind", 0))));
                connection.targetId = child["target"].toString();
                connection.parameter = juce::jmax (0, static_cast<int> (child.getProperty ("parameter", 0)));
                connection.depth = juce::jlimit (0.0, 1.0, static_cast<double> (child.getProperty ("depth", 0.5)));
                connection.offset = juce::jlimit (-1.0, 1.0, static_cast<double> (child.getProperty ("offset", 0.0)));
                connection.inverted = static_cast<bool> (child.getProperty ("inverted", false));
                if (connection.sourceId.isNotEmpty() && connection.targetId.isNotEmpty())
                    connections.push_back (std::move (connection));
            }
        }
        return true;
    }

    static bool routesFromValueTree (const juce::ValueTree& tree, std::vector<RoadRoute>& destination)
    {
        if (! tree.isValid() || ! tree.hasType ("routes")) return false;
        for (const auto& routeTree : tree)
        {
            if (! routeTree.hasType ("route")) continue;
            RoadRoute route;
            route.colour = juce::Colour::fromString (routeTree["colour"].toString());
            route.isPipe = static_cast<bool> (routeTree.getProperty ("pipe", false));
            route.isWarpPipe = static_cast<bool> (routeTree.getProperty ("warpPipe", false));
            if (route.isPipe && route.colour == lineColour()) route.colour = pipeColourForIndex (static_cast<int> (destination.size()));
            for (const auto& pointTree : routeTree)
                if (pointTree.hasType ("point")) route.points.push_back ({ static_cast<float> (pointTree["x"]), static_cast<float> (pointTree["y"]) });
            if (! route.points.empty()) destination.push_back (std::move (route));
        }
        return true;
    }

    static juce::ValueTree discToValueTree (const Disc& disc)
    {
        juce::ValueTree tree ("disc");
        tree.setProperty ("x", disc.centre.x, nullptr); tree.setProperty ("y", disc.centre.y, nullptr);
        tree.setProperty ("sounds", disc.soundElementCount, nullptr); tree.setProperty ("worlds", disc.nestedWorldCount, nullptr);
        tree.setProperty ("triggerMode", static_cast<int> (disc.triggerMode), nullptr);
        tree.setProperty ("elementMode", static_cast<int> (disc.elementMode), nullptr);
        tree.setProperty ("elementProbability", disc.elementProbability, nullptr);
        tree.setProperty ("holdDropsUntilFinished", disc.holdDropsUntilFinished, nullptr);
        tree.setProperty ("level", disc.level, nullptr); tree.setProperty ("pan", disc.pan, nullptr);
        tree.setProperty ("muted", disc.muted, nullptr); tree.setProperty ("solo", disc.solo, nullptr);
        tree.setProperty ("id", disc.id, nullptr);
        for (const auto& code : disc.scCodeElements) { juce::ValueTree child ("scCode"); child.setProperty ("code", code.code, nullptr); child.setProperty ("duration", code.durationSeconds, nullptr); tree.addChild (child, -1, nullptr); }
        for (const auto& patch : disc.pdPatches) { juce::ValueTree child ("pdPatch"); child.setProperty ("patch", patch.patch, nullptr); child.setProperty ("path", patch.searchPath, nullptr); child.setProperty ("duration", patch.durationSeconds, nullptr); tree.addChild (child, -1, nullptr); }
        for (const auto& sheet : disc.scSheets) { juce::ValueTree child ("scSheet"); child.addChild (sheet.toValueTree(), -1, nullptr); tree.addChild (child, -1, nullptr); }
        for (const auto& gridSnapshot : disc.orcaGrids)
        {
            juce::ValueTree child ("orcaGrid"); child.setProperty ("width", gridSnapshot.width, nullptr); child.setProperty ("height", gridSnapshot.height, nullptr);
            juce::String cells; for (const auto glyph : gridSnapshot.cells) cells << juce::String::charToString (static_cast<juce_wchar> (static_cast<unsigned char> (glyph)));
            child.setProperty ("cells", cells, nullptr); tree.addChild (child, -1, nullptr);
        }
        for (const auto& carousel : disc.carousels) tree.addChild (carousel.toValueTree(), -1, nullptr);
        for (const auto& state : disc.pipeWorlds) { juce::ValueTree child ("pipeWorld"); child.setProperty ("state", state, nullptr); tree.addChild (child, -1, nullptr); }
        tree.addChild (routesToValueTree (disc.nestedRoutes), -1, nullptr);
        tree.addChild (tapsToValueTree (disc.nestedPipeTaps), -1, nullptr);
        tree.addChild (drainsToValueTree (disc.nestedPipeDrains), -1, nullptr);
        tree.addChild (clonersToValueTree (disc.nestedPipeCloners), -1, nullptr);
        tree.addChild (speedLimitsToValueTree (disc.nestedPipeSpeedLimits), -1, nullptr);
        tree.addChild (waitsToValueTree (disc.nestedPipeWaits), -1, nullptr);
        tree.addChild (strikesToValueTree (disc.nestedPipeStrikes), -1, nullptr);
        tree.addChild (teleportsToValueTree (disc.nestedPipeTeleports), -1, nullptr);
        tree.addChild (filtersToValueTree (disc.nestedPipeFilters), -1, nullptr);
        tree.addChild (logicsToValueTree (disc.nestedPipeLogics), -1, nullptr);
        tree.addChild (discsToValueTree (disc.nestedDiscs), -1, nullptr);
        return tree;
    }

    static juce::ValueTree discsToValueTree (const std::vector<Disc>& source)
    {
        juce::ValueTree tree ("discs");
        for (const auto& disc : source) tree.addChild (discToValueTree (disc), -1, nullptr);
        return tree;
    }

    static bool discFromValueTree (const juce::ValueTree& tree, Disc& disc)
    {
        if (! tree.hasType ("disc")) return false;
        disc.centre = { static_cast<float> (tree["x"]), static_cast<float> (tree["y"]) };
        disc.triggerMode = static_cast<Disc::TriggerMode> (juce::jlimit (0, 2, static_cast<int> (tree.getProperty ("triggerMode", 0))));
        disc.elementMode = static_cast<Disc::ElementMode> (juce::jlimit (0, 4, static_cast<int> (tree.getProperty ("elementMode", 0))));
        disc.elementProbability = juce::jlimit (0.0, 1.0, static_cast<double> (tree.getProperty ("elementProbability", 0.5)));
        disc.holdDropsUntilFinished = static_cast<bool> (tree.getProperty ("holdDropsUntilFinished", false));
        disc.level = juce::jlimit (0.0f, 1.5f, static_cast<float> (tree.getProperty ("level", 1.0f)));
        disc.pan = juce::jlimit (-1.0f, 1.0f, static_cast<float> (tree.getProperty ("pan", 0.0f)));
        disc.muted = static_cast<bool> (tree.getProperty ("muted", false)); disc.solo = static_cast<bool> (tree.getProperty ("solo", false));
        disc.id = tree.getProperty ("id", disc.id).toString();
        disc.soundElementCount = juce::jmax (0, static_cast<int> (tree["sounds"]));
        disc.nestedWorldCount = juce::jmax (0, static_cast<int> (tree["worlds"]));
        for (const auto& child : tree)
        {
            if (child.hasType ("scCode")) disc.scCodeElements.push_back ({ child["code"].toString(), static_cast<float> (child["duration"]) });
            else if (child.hasType ("pdPatch")) disc.pdPatches.push_back ({ child["patch"].toString(), child["path"].toString(), static_cast<float> (child["duration"]) });
            else if (child.hasType ("scSheet") && child.getNumChildren() > 0) disc.scSheets.push_back (ScoreDocument::fromValueTree (child.getChild (0)));
            else if (child.hasType ("orcaGrid"))
            {
                gridcollider::GridModel::Snapshot snapshot; snapshot.width = juce::jmax (1, static_cast<int> (child["width"])); snapshot.height = juce::jmax (1, static_cast<int> (child["height"]));
                const auto cells = child["cells"].toString(); snapshot.cells.resize (static_cast<size_t> (snapshot.width * snapshot.height), '.');
                for (int i = 0; i < juce::jmin (cells.length(), static_cast<int> (snapshot.cells.size())); ++i) snapshot.cells[static_cast<size_t> (i)] = static_cast<char> (cells[i]);
                disc.orcaGrids.push_back (std::move (snapshot));
            }
            else if (child.hasType ("carousel")) disc.carousels.push_back (CarouselDocument::fromValueTree (child));
            else if (child.hasType ("pipeWorld")) disc.pipeWorlds.push_back (child["state"].toString());
            else if (child.hasType ("routes")) { if (! routesFromValueTree (child, disc.nestedRoutes)) return false; }
            else if (child.hasType ("pipeTaps")) { if (! tapsFromValueTree (child, disc.nestedPipeTaps)) return false; }
            else if (child.hasType ("pipeDrains")) { if (! drainsFromValueTree (child, disc.nestedPipeDrains)) return false; }
            else if (child.hasType ("pipeCloners")) { if (! clonersFromValueTree (child, disc.nestedPipeCloners)) return false; }
            else if (child.hasType ("pipeSpeedLimits")) { if (! speedLimitsFromValueTree (child, disc.nestedPipeSpeedLimits)) return false; }
            else if (child.hasType ("pipeWaits")) { if (! waitsFromValueTree (child, disc.nestedPipeWaits)) return false; }
            else if (child.hasType ("pipeStrikes")) { if (! strikesFromValueTree (child, disc.nestedPipeStrikes)) return false; }
            else if (child.hasType ("pipeTeleports")) { if (! teleportsFromValueTree (child, disc.nestedPipeTeleports)) return false; }
            else if (child.hasType ("pipeFilters")) { if (! filtersFromValueTree (child, disc.nestedPipeFilters)) return false; }
            else if (child.hasType ("pipeLogics")) { if (! logicsFromValueTree (child, disc.nestedPipeLogics)) return false; }
            else if (child.hasType ("discs")) { if (! discsFromValueTree (child, disc.nestedDiscs)) return false; }
        }
        return true;
    }

    static bool discsFromValueTree (const juce::ValueTree& tree, std::vector<Disc>& destination)
    {
        if (! tree.isValid() || ! tree.hasType ("discs")) return false;
        for (const auto& discTree : tree) { Disc disc; if (discFromValueTree (discTree, disc)) destination.push_back (std::move (disc)); }
        return true;
    }

    struct SegmentHit
    {
        int route = -1;
        int insertIndex = -1;
        juce::Point<float> point;
    };

    std::vector<RoadRoute>& routes()
    {
        auto* currentRoutes = &rootRoutes;
        auto* currentDiscs = &rootDiscs;

        for (auto index : worldPath)
        {
            auto& disc = (*currentDiscs)[static_cast<size_t> (index)];
            currentRoutes = &disc.nestedRoutes;
            currentDiscs = &disc.nestedDiscs;
        }

        return *currentRoutes;
    }

    const std::vector<RoadRoute>& routes() const
    {
        auto* currentRoutes = &rootRoutes;
        auto* currentDiscs = &rootDiscs;

        for (auto index : worldPath)
        {
            const auto& disc = (*currentDiscs)[static_cast<size_t> (index)];
            currentRoutes = &disc.nestedRoutes;
            currentDiscs = &disc.nestedDiscs;
        }

        return *currentRoutes;
    }

    std::vector<PipeTap>& pipeTaps()
    {
        auto* currentTaps = &rootPipeTaps;
        auto* currentDiscs = &rootDiscs;
        for (auto index : worldPath)
        {
            auto& disc = (*currentDiscs)[static_cast<size_t> (index)];
            currentTaps = &disc.nestedPipeTaps;
            currentDiscs = &disc.nestedDiscs;
        }
        return *currentTaps;
    }

    const std::vector<PipeTap>& pipeTaps() const
    {
        auto* currentTaps = &rootPipeTaps;
        auto* currentDiscs = &rootDiscs;
        for (auto index : worldPath)
        {
            const auto& disc = (*currentDiscs)[static_cast<size_t> (index)];
            currentTaps = &disc.nestedPipeTaps;
            currentDiscs = &disc.nestedDiscs;
        }
        return *currentTaps;
    }

    std::vector<PipeDrain>& pipeDrains()
    {
        auto* currentDrains = &rootPipeDrains;
        auto* currentDiscs = &rootDiscs;
        for (auto index : worldPath)
        {
            auto& disc = (*currentDiscs)[static_cast<size_t> (index)];
            currentDrains = &disc.nestedPipeDrains;
            currentDiscs = &disc.nestedDiscs;
        }
        return *currentDrains;
    }

    const std::vector<PipeDrain>& pipeDrains() const
    {
        auto* currentDrains = &rootPipeDrains;
        auto* currentDiscs = &rootDiscs;
        for (auto index : worldPath)
        {
            const auto& disc = (*currentDiscs)[static_cast<size_t> (index)];
            currentDrains = &disc.nestedPipeDrains;
            currentDiscs = &disc.nestedDiscs;
        }
        return *currentDrains;
    }

    std::vector<PipeCloner>& pipeCloners()
    {
        auto* currentCloners = &rootPipeCloners;
        auto* currentDiscs = &rootDiscs;
        for (auto index : worldPath)
        {
            auto& disc = (*currentDiscs)[static_cast<size_t> (index)];
            currentCloners = &disc.nestedPipeCloners;
            currentDiscs = &disc.nestedDiscs;
        }
        return *currentCloners;
    }

    const std::vector<PipeCloner>& pipeCloners() const
    {
        auto* currentCloners = &rootPipeCloners;
        auto* currentDiscs = &rootDiscs;
        for (auto index : worldPath)
        {
            const auto& disc = (*currentDiscs)[static_cast<size_t> (index)];
            currentCloners = &disc.nestedPipeCloners;
            currentDiscs = &disc.nestedDiscs;
        }
        return *currentCloners;
    }

    std::vector<PipeSpeedLimit>& pipeSpeedLimits()
    {
        auto* currentLimits = &rootPipeSpeedLimits;
        auto* currentDiscs = &rootDiscs;
        for (auto index : worldPath)
        {
            auto& disc = (*currentDiscs)[static_cast<size_t> (index)];
            currentLimits = &disc.nestedPipeSpeedLimits;
            currentDiscs = &disc.nestedDiscs;
        }
        return *currentLimits;
    }

    const std::vector<PipeSpeedLimit>& pipeSpeedLimits() const
    {
        auto* currentLimits = &rootPipeSpeedLimits;
        auto* currentDiscs = &rootDiscs;
        for (auto index : worldPath)
        {
            const auto& disc = (*currentDiscs)[static_cast<size_t> (index)];
            currentLimits = &disc.nestedPipeSpeedLimits;
            currentDiscs = &disc.nestedDiscs;
        }
        return *currentLimits;
    }

    std::vector<PipeWait>& pipeWaits()
    {
        auto* currentWaits = &rootPipeWaits;
        auto* currentDiscs = &rootDiscs;
        for (auto index : worldPath)
        {
            auto& disc = (*currentDiscs)[static_cast<size_t> (index)];
            currentWaits = &disc.nestedPipeWaits;
            currentDiscs = &disc.nestedDiscs;
        }
        return *currentWaits;
    }

    const std::vector<PipeWait>& pipeWaits() const
    {
        auto* currentWaits = &rootPipeWaits;
        auto* currentDiscs = &rootDiscs;
        for (auto index : worldPath)
        {
            const auto& disc = (*currentDiscs)[static_cast<size_t> (index)];
            currentWaits = &disc.nestedPipeWaits;
            currentDiscs = &disc.nestedDiscs;
        }
        return *currentWaits;
    }

    std::vector<PipeStrike>& pipeStrikes()
    {
        auto* current = &rootPipeStrikes; auto* currentDiscs = &rootDiscs;
        for (auto index : worldPath) { auto& disc = (*currentDiscs)[static_cast<size_t> (index)]; current = &disc.nestedPipeStrikes; currentDiscs = &disc.nestedDiscs; }
        return *current;
    }

    const std::vector<PipeStrike>& pipeStrikes() const
    {
        auto* current = &rootPipeStrikes; auto* currentDiscs = &rootDiscs;
        for (auto index : worldPath) { const auto& disc = (*currentDiscs)[static_cast<size_t> (index)]; current = &disc.nestedPipeStrikes; currentDiscs = &disc.nestedDiscs; }
        return *current;
    }

    std::vector<PipeTeleport>& pipeTeleports()
    {
        auto* current = &rootPipeTeleports; auto* currentDiscs = &rootDiscs;
        for (auto index : worldPath) { auto& disc = (*currentDiscs)[static_cast<size_t> (index)]; current = &disc.nestedPipeTeleports; currentDiscs = &disc.nestedDiscs; }
        return *current;
    }

    const std::vector<PipeTeleport>& pipeTeleports() const
    {
        auto* current = &rootPipeTeleports; auto* currentDiscs = &rootDiscs;
        for (auto index : worldPath) { const auto& disc = (*currentDiscs)[static_cast<size_t> (index)]; current = &disc.nestedPipeTeleports; currentDiscs = &disc.nestedDiscs; }
        return *current;
    }
    std::vector<PipeFilter>& pipeFilters()
    { auto* current = &rootPipeFilters; auto* currentDiscs = &rootDiscs; for (auto index : worldPath) { auto& disc = (*currentDiscs)[static_cast<size_t> (index)]; current = &disc.nestedPipeFilters; currentDiscs = &disc.nestedDiscs; } return *current; }
    const std::vector<PipeFilter>& pipeFilters() const
    { auto* current = &rootPipeFilters; auto* currentDiscs = &rootDiscs; for (auto index : worldPath) { const auto& disc = (*currentDiscs)[static_cast<size_t> (index)]; current = &disc.nestedPipeFilters; currentDiscs = &disc.nestedDiscs; } return *current; }
    std::vector<PipeLogic>& pipeLogics()
    { auto* current = &rootPipeLogics; auto* currentDiscs = &rootDiscs; for (auto index : worldPath) { auto& disc = (*currentDiscs)[static_cast<size_t> (index)]; current = &disc.nestedPipeLogics; currentDiscs = &disc.nestedDiscs; } return *current; }
    const std::vector<PipeLogic>& pipeLogics() const
    { auto* current = &rootPipeLogics; auto* currentDiscs = &rootDiscs; for (auto index : worldPath) { const auto& disc = (*currentDiscs)[static_cast<size_t> (index)]; current = &disc.nestedPipeLogics; currentDiscs = &disc.nestedDiscs; } return *current; }

    std::vector<Disc>& discs()
    {
        auto* currentDiscs = &rootDiscs;

        for (auto index : worldPath)
            currentDiscs = &(*currentDiscs)[static_cast<size_t> (index)].nestedDiscs;

        return *currentDiscs;
    }

    const std::vector<Disc>& discs() const
    {
        auto* currentDiscs = &rootDiscs;

        for (auto index : worldPath)
            currentDiscs = &(*currentDiscs)[static_cast<size_t> (index)].nestedDiscs;

        return *currentDiscs;
    }

    Disc* findDisc (const DiscHandle& handle)
    {
        auto* currentDiscs = &rootDiscs;

        for (auto index : handle.worldPath)
        {
            if (! juce::isPositiveAndBelow (index, static_cast<int> (currentDiscs->size())))
                return nullptr;

            currentDiscs = &(*currentDiscs)[static_cast<size_t> (index)].nestedDiscs;
        }

        if (! juce::isPositiveAndBelow (handle.discIndex, static_cast<int> (currentDiscs->size())))
            return nullptr;

        return &(*currentDiscs)[static_cast<size_t> (handle.discIndex)];
    }

    const Disc* findDisc (const DiscHandle& handle) const
    {
        auto* currentDiscs = &rootDiscs;

        for (auto index : handle.worldPath)
        {
            if (! juce::isPositiveAndBelow (index, static_cast<int> (currentDiscs->size())))
                return nullptr;

            currentDiscs = &(*currentDiscs)[static_cast<size_t> (index)].nestedDiscs;
        }

        if (! juce::isPositiveAndBelow (handle.discIndex, static_cast<int> (currentDiscs->size())))
            return nullptr;

        return &(*currentDiscs)[static_cast<size_t> (handle.discIndex)];
    }

    DiscInfo infoForDisc (const Disc& disc) const
    {
        const auto hasScCode = disc.hasScCodeElement();
        const auto hasPdPatch = disc.hasPdPatch();
        const auto hasScSheet = disc.hasScSheet();
        const auto hasOrcaGrid = disc.hasOrcaGrid();
        const auto& firstCode = hasScCode ? disc.scCodeElements.front() : ScCodeElement {};
        const auto& firstPdPatch = hasPdPatch ? disc.pdPatches.front() : PdPatchElement {};

        return { true,
                 disc.soundElementCount,
                 disc.nestedWorldCount,
                 static_cast<int> (disc.scCodeElements.size()),
                 static_cast<int> (disc.pdPatches.size()),
                 static_cast<int> (disc.scSheets.size()),
                 static_cast<int> (disc.orcaGrids.size()),
                 disc.hasNestedWorld(),
                 hasScCode,
                 hasScCode ? firstCode.code : juce::String(),
                 hasScCode ? firstCode.durationSeconds : -1.0f,
                 hasPdPatch,
                 hasPdPatch ? firstPdPatch.patch : juce::String(),
                 hasPdPatch ? firstPdPatch.durationSeconds : -1.0f,
                 hasScSheet,
                 hasScSheet ? disc.scSheets.front().getNumRows() : 0,
                 hasScSheet ? disc.scSheets.front().getNumSections() : 0,
                 hasOrcaGrid,
                 hasOrcaGrid ? disc.orcaGrids.front().width : 0,
                 hasOrcaGrid ? disc.orcaGrids.front().height : 0,
                 static_cast<int> (disc.nestedRoutes.size()),
                 static_cast<int> (disc.nestedDiscs.size()),
                 static_cast<int> (disc.carousels.size()),
                 disc.hasCarousel(),
                 disc.hasCarousel() ? static_cast<int> (disc.carousels.front().items.size()) : 0,
                 static_cast<int> (disc.pipeWorlds.size()),
                 disc.hasPipeWorld(),
                 static_cast<int> (disc.triggerMode),
                 static_cast<int> (disc.elementMode),
                 modulatedUnitValue (disc.elementProbability, ModulationTargetKind::disc, disc.id, 2),
                 disc.holdDropsUntilFinished,
                 juce::jlimit (0.0f, 1.5f, disc.level * static_cast<float> (1.0 + modulationSignal (ModulationTargetKind::disc, disc.id, 0))),
                 juce::jlimit (-1.0f, 1.0f, disc.pan + static_cast<float> (modulationSignal (ModulationTargetKind::disc, disc.id, 1))),
                 disc.muted, disc.solo };
    }

    void resetSelection()
    {
        selectedRoute = -1;
        selectedNode = -1;
        selectedDisc = -1;
        selectedTap = -1;
        selectedDrain = -1;
        selectedCloner = -1;
        selectedSpeedLimit = -1;
        selectedWait = -1;
        selectedStrike = -1;
        selectedTeleport = -1;
        selectedFilter = -1;
        selectedLogic = -1;
        selectedModulator = -1;
        selectedModulationConnection = -1;
        connectingModulator = -1;
        draggingModulator = false;
        draggingNode = false;
        drawing = false;
        currentRoute.points.clear();
        notifySelectionChanged();
    }

    void notifySelectionChanged()
    {
        if (onSelectionChanged != nullptr)
            onSelectionChanged();
    }

    void resetView()
    {
        viewScale = 1.0f;
        viewOffset = { 0.0f, 0.0f };
    }

    static int countElementsRecursive (const std::vector<Disc>& discsToCount)
    {
        int total = 0;

        for (const auto& disc : discsToCount)
        {
            total += disc.getElementCount();
            total += countElementsRecursive (disc.nestedDiscs);
        }

        return total;
    }

    void appendDiscAudioTriggers (const Disc& disc,
                                  int depth,
                                  int branchIndex,
                                  std::vector<DiscAudioTrigger>& triggers) const
    {
        if (disc.muted) return;
        const auto applyMix = [this, &disc] (DiscAudioTrigger& trigger)
        {
            const auto levelSignal = modulationSignal (ModulationTargetKind::disc, disc.id, 0);
            trigger.gain = juce::jlimit (0.0f, 1.5f, disc.level * static_cast<float> (1.0 + levelSignal));
            trigger.pan = juce::jlimit (-1.0f, 1.0f,
                                       disc.pan + static_cast<float> (modulationSignal (ModulationTargetKind::disc, disc.id, 1)));
        };
        if (disc.soundElementCount > 0)
        {
            DiscAudioTrigger trigger;
            trigger.soundElementCount = disc.soundElementCount;
            trigger.depth = depth;
            trigger.branchIndex = branchIndex;
            applyMix (trigger);
            triggers.push_back (std::move (trigger));
        }

        for (const auto& scCodeElement : disc.scCodeElements)
        {
            DiscAudioTrigger trigger;
            trigger.scCode = scCodeElement.code;
            trigger.scDurationSeconds = scCodeElement.durationSeconds;
            trigger.depth = depth;
            trigger.branchIndex = branchIndex;
            applyMix (trigger);
            triggers.push_back (std::move (trigger));
        }

        for (const auto& pdPatch : disc.pdPatches)
        {
            DiscAudioTrigger trigger;
            trigger.pdPatch = pdPatch.patch;
            trigger.pdSearchPath = pdPatch.searchPath;
            trigger.pdDurationSeconds = pdPatch.durationSeconds;
            trigger.depth = depth;
            trigger.branchIndex = branchIndex;
            applyMix (trigger);
            triggers.push_back (std::move (trigger));
        }

        for (const auto& scSheet : disc.scSheets)
        {
            DiscAudioTrigger trigger;
            trigger.hasScSheet = true;
            trigger.scSheet = scSheet.toValueTree();
            trigger.depth = depth;
            trigger.branchIndex = branchIndex;
            applyMix (trigger);
            triggers.push_back (std::move (trigger));
        }

        for (const auto& orcaGrid : disc.orcaGrids)
        {
            DiscAudioTrigger trigger;
            trigger.hasOrcaGrid = true;
            trigger.orcaGrid = orcaGrid;
            trigger.depth = depth;
            trigger.branchIndex = branchIndex;
            applyMix (trigger);
            triggers.push_back (std::move (trigger));
        }

        if (! disc.hasNestedWorld())
            return;

        for (int i = 0; i < disc.nestedWorldCount; ++i)
        {
            DiscAudioTrigger nestedTrigger;
            nestedTrigger.soundElementCount = 1;
            nestedTrigger.nestedWorldPulse = true;
            nestedTrigger.depth = depth;
            nestedTrigger.branchIndex = branchIndex;
            applyMix (nestedTrigger);
            triggers.push_back (std::move (nestedTrigger));
        }

        for (int i = 0; i < static_cast<int> (disc.nestedDiscs.size()); ++i)
            appendDiscAudioTriggers (disc.nestedDiscs[static_cast<size_t> (i)],
                                     depth + 1,
                                     branchIndex * 7 + i + 1,
                                     triggers);
    }

    juce::AffineTransform getViewTransform() const
    {
        return juce::AffineTransform::scale (viewScale).translated (viewOffset.x, viewOffset.y);
    }

    juce::Point<float> screenToWorld (juce::Point<float> screenPoint) const
    {
        return { (screenPoint.x - viewOffset.x) / viewScale,
                 (screenPoint.y - viewOffset.y) / viewScale };
    }

    struct ModulationTargetHit
    {
        ModulationTargetKind kind = ModulationTargetKind::disc;
        juce::String id;
        juce::Point<float> position;
        float distance = std::numeric_limits<float>::max();
        bool isValid() const noexcept { return id.isNotEmpty(); }
    };

    template <typename Item>
    static std::optional<juce::Point<float>> positionForId (const std::vector<Item>& items, const juce::String& id)
    {
        const auto found = std::find_if (items.begin(), items.end(), [&id] (const auto& item) { return item.id == id; });
        if (found == items.end()) return std::nullopt;
        if constexpr (std::is_same_v<Item, Disc>) return found->centre;
        else return found->position;
    }

    std::optional<juce::Point<float>> modulationTargetPosition (ModulationTargetKind kind, const juce::String& id) const
    {
        switch (kind)
        {
            case ModulationTargetKind::disc:       return positionForId (rootDiscs, id);
            case ModulationTargetKind::tap:        return positionForId (rootPipeTaps, id);
            case ModulationTargetKind::drain:      return positionForId (rootPipeDrains, id);
            case ModulationTargetKind::quantum:    return positionForId (rootPipeCloners, id);
            case ModulationTargetKind::speedLimit: return positionForId (rootPipeSpeedLimits, id);
            case ModulationTargetKind::wait:       return positionForId (rootPipeWaits, id);
            case ModulationTargetKind::strike:     return positionForId (rootPipeStrikes, id);
            case ModulationTargetKind::teleport:   return positionForId (rootPipeTeleports, id);
            case ModulationTargetKind::filter:     return positionForId (rootPipeFilters, id);
            case ModulationTargetKind::logic:      return positionForId (rootPipeLogics, id);
        }
        return std::nullopt;
    }

    ModulationTargetHit findModulationTarget (juce::Point<float> point, float tolerance) const
    {
        ModulationTargetHit best;
        const auto consider = [&] (ModulationTargetKind kind, const auto& items)
        {
            for (const auto& item : items)
            {
                const auto position = [&]
                {
                    using Item = std::decay_t<decltype (item)>;
                    if constexpr (std::is_same_v<Item, Disc>) return item.centre;
                    else return item.position;
                }();
                const auto distance = position.getDistanceFrom (point);
                if (distance <= tolerance && distance < best.distance)
                    best = { kind, item.id, position, distance };
            }
        };
        consider (ModulationTargetKind::disc, rootDiscs);
        consider (ModulationTargetKind::tap, rootPipeTaps);
        consider (ModulationTargetKind::drain, rootPipeDrains);
        consider (ModulationTargetKind::quantum, rootPipeCloners);
        consider (ModulationTargetKind::speedLimit, rootPipeSpeedLimits);
        consider (ModulationTargetKind::wait, rootPipeWaits);
        consider (ModulationTargetKind::strike, rootPipeStrikes);
        consider (ModulationTargetKind::teleport, rootPipeTeleports);
        consider (ModulationTargetKind::filter, rootPipeFilters);
        consider (ModulationTargetKind::logic, rootPipeLogics);
        return best;
    }

    int findModulatorAt (juce::Point<float> point, float tolerance) const
    {
        for (int i = static_cast<int> (modulators.size()) - 1; i >= 0; --i)
            if (modulators[static_cast<size_t> (i)].position.getDistanceFrom (point) <= tolerance) return i;
        return -1;
    }

    int findModulationConnectionAt (juce::Point<float> point, float tolerance) const
    {
        for (int i = static_cast<int> (modulationConnections.size()) - 1; i >= 0; --i)
        {
            const auto& connection = modulationConnections[static_cast<size_t> (i)];
            const auto source = std::find_if (modulators.begin(), modulators.end(), [&] (const auto& mod) { return mod.id == connection.sourceId; });
            const auto target = modulationTargetPosition (connection.targetKind, connection.targetId);
            if (source != modulators.end() && target.has_value()
                && distanceFromLineSegment (point, source->position, *target) <= tolerance)
                return i;
        }
        return -1;
    }

    static juce::String shortModulatorShapeName (Modulator::Shape shape)
    {
        switch (shape)
        {
            case Modulator::Shape::sine:     return "SIN";
            case Modulator::Shape::triangle: return "TRI";
            case Modulator::Shape::square:   return "SQR";
            case Modulator::Shape::random:   return "RND";
        }
        return "MOD";
    }

    double modulatorValue (const Modulator& source) const
    {
        if (! source.enabled) return 0.5;
        const auto cycles = flowBeatPosition / juce::jmax (0.125, source.cycleBeats) + source.phase;
        const auto phase = cycles - std::floor (cycles);
        switch (source.shape)
        {
            case Modulator::Shape::sine:     return 0.5 + 0.5 * std::sin (phase * juce::MathConstants<double>::twoPi);
            case Modulator::Shape::triangle: return 1.0 - std::abs (phase * 2.0 - 1.0);
            case Modulator::Shape::square:   return phase < 0.5 ? 1.0 : 0.0;
            case Modulator::Shape::random:
            {
                const auto step = static_cast<int64_t> (std::floor (cycles));
                juce::Random random (static_cast<int64_t> (source.id.hashCode64()) ^ (step * 0x5deece66dLL));
                return random.nextDouble();
            }
        }
        return 0.5;
    }

    double modulationSignal (ModulationTargetKind kind, const juce::String& targetId, int parameter) const
    {
        auto signal = 0.0;
        for (const auto& connection : modulationConnections)
        {
            if (connection.targetKind != kind || connection.targetId != targetId || connection.parameter != parameter) continue;
            const auto source = std::find_if (modulators.begin(), modulators.end(), [&] (const auto& mod) { return mod.id == connection.sourceId; });
            if (source == modulators.end() || ! source->enabled) continue;
            auto bipolar = modulatorValue (*source) * 2.0 - 1.0;
            if (connection.inverted) bipolar = -bipolar;
            signal += bipolar * connection.depth + connection.offset;
        }
        return juce::jlimit (-1.0, 1.0, signal);
    }

    bool hasModulationConnection (ModulationTargetKind kind, const juce::String& targetId, int parameter) const
    {
        return std::any_of (modulationConnections.begin(), modulationConnections.end(), [&] (const auto& connection)
        {
            if (connection.targetKind != kind || connection.targetId != targetId
                || connection.parameter != parameter
                || (connection.depth <= 0.000001 && std::abs (connection.offset) <= 0.000001))
                return false;
            const auto source = std::find_if (modulators.begin(), modulators.end(), [&] (const auto& mod)
            { return mod.id == connection.sourceId; });
            return source != modulators.end() && source->enabled;
        });
    }

    double modulatedUnitValue (double base, ModulationTargetKind kind, const juce::String& id, int parameter) const
    {
        return juce::jlimit (0.0, 1.0, base + modulationSignal (kind, id, parameter));
    }

    double modulatedRatioValue (double base, ModulationTargetKind kind, const juce::String& id, int parameter,
                                double minimum, double maximum) const
    {
        return juce::jlimit (minimum, maximum, base * std::pow (2.0, modulationSignal (kind, id, parameter)));
    }

    int modulatedCountValue (int base, ModulationTargetKind kind, const juce::String& id, int parameter,
                             int minimum, int maximum) const
    {
        const auto range = static_cast<double> (maximum - minimum);
        return juce::jlimit (minimum, maximum,
                            static_cast<int> (std::round (base + modulationSignal (kind, id, parameter) * range * 0.5)));
    }

    void drawModulationLayer (juce::Graphics& g)
    {
        const auto purple = juce::Colour (0xffd884ff);
        const auto cyan = juce::Colour (0xff65e6d4);

        const auto drawTarget = [&] (ModulationTargetKind kind, const auto& items)
        {
            for (const auto& item : items)
            {
                const auto position = [&]
                {
                    using Item = std::decay_t<decltype (item)>;
                    if constexpr (std::is_same_v<Item, Disc>) return item.centre;
                    else return item.position;
                }();
                const auto connected = std::any_of (modulationConnections.begin(), modulationConnections.end(), [&] (const auto& connection)
                { return connection.targetKind == kind && connection.targetId == item.id; });
                g.setColour ((connected ? cyan : textMuted()).withAlpha (connected ? 0.82f : 0.34f));
                g.drawEllipse (juce::Rectangle<float> (connected ? 15.0f : 10.0f, connected ? 15.0f : 10.0f).withCentre (position),
                               connected ? 1.8f : 1.1f);
            }
        };
        drawTarget (ModulationTargetKind::disc, rootDiscs);
        drawTarget (ModulationTargetKind::tap, rootPipeTaps);
        drawTarget (ModulationTargetKind::drain, rootPipeDrains);
        drawTarget (ModulationTargetKind::quantum, rootPipeCloners);
        drawTarget (ModulationTargetKind::speedLimit, rootPipeSpeedLimits);
        drawTarget (ModulationTargetKind::wait, rootPipeWaits);
        drawTarget (ModulationTargetKind::strike, rootPipeStrikes);
        drawTarget (ModulationTargetKind::teleport, rootPipeTeleports);
        drawTarget (ModulationTargetKind::filter, rootPipeFilters);
        drawTarget (ModulationTargetKind::logic, rootPipeLogics);

        for (int i = 0; i < static_cast<int> (modulationConnections.size()); ++i)
        {
            const auto& connection = modulationConnections[static_cast<size_t> (i)];
            const auto source = std::find_if (modulators.begin(), modulators.end(), [&] (const auto& mod) { return mod.id == connection.sourceId; });
            const auto target = modulationTargetPosition (connection.targetKind, connection.targetId);
            if (source == modulators.end() || ! target.has_value()) continue;
            const auto selected = i == selectedModulationConnection;
            g.setColour (purple.withAlpha (selected ? 0.95f : static_cast<float> (0.36 + connection.depth * 0.42)));
            g.drawLine ({ source->position, *target }, selected ? 2.6f : 1.5f);
            const auto direction = (*target - source->position);
            const auto length = direction.getDistanceFromOrigin();
            if (length > 8.0f)
            {
                const auto unit = direction / length;
                const auto end = *target - unit * 8.0f;
                const auto normal = juce::Point<float> (-unit.y, unit.x);
                juce::Path arrow;
                arrow.startNewSubPath (end);
                arrow.lineTo (end - unit * 6.0f + normal * 3.5f);
                arrow.lineTo (end - unit * 6.0f - normal * 3.5f);
                arrow.closeSubPath();
                g.fillPath (arrow);
            }
        }

        if (juce::isPositiveAndBelow (connectingModulator, static_cast<int> (modulators.size())))
        {
            const float dash[] { 5.0f, 4.0f };
            g.setColour (purple.withAlpha (0.72f));
            g.drawDashedLine ({ modulators[static_cast<size_t> (connectingModulator)].position, modulationDragPoint }, dash, 2, 1.6f);
        }

        for (int i = 0; i < static_cast<int> (modulators.size()); ++i)
        {
            const auto& source = modulators[static_cast<size_t> (i)];
            const auto selected = i == selectedModulator;
            const auto value = modulatorValue (source);
            const auto halo = juce::Rectangle<float> (38.0f, 38.0f).withCentre (source.position);
            g.setColour (purple.withAlpha (source.enabled ? 0.14f : 0.05f));
            g.fillEllipse (halo);
            g.setColour (purple.withMultipliedAlpha (source.enabled ? 0.92f : 0.30f));
            g.fillEllipse (halo.reduced (7.0f));
            g.setColour (juce::Colour (0xff121019));
            g.fillEllipse (halo.reduced (10.0f));
            g.setColour (cyan.withMultipliedAlpha (source.enabled ? 0.95f : 0.28f));
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f)
                               .withCentre (source.position + juce::Point<float> (0.0f, static_cast<float> ((0.5 - value) * 12.0))));
            g.setColour (textPrimary().withMultipliedAlpha (source.enabled ? 0.90f : 0.34f));
            g.setFont (juce::FontOptions (7.0f, juce::Font::bold));
            g.drawText (shortModulatorShapeName (source.shape), halo.reduced (8.0f), juce::Justification::centred);
            if (selected)
            {
                g.setColour (juce::Colours::white.withAlpha (0.92f));
                g.drawEllipse (halo.expanded (2.0f), 1.6f);
            }
            g.setColour (textPrimary().withAlpha (0.78f));
            g.setFont (juce::FontOptions (9.0f));
            g.drawText (source.name, juce::Rectangle<float> (90.0f, 16.0f).withCentre (source.position.translated (0.0f, 28.0f)),
                        juce::Justification::centred, true);
        }
    }

    void showModulatorSettings (int index)
    {
        if (! juce::isPositiveAndBelow (index, static_cast<int> (modulators.size()))) return;
        const auto source = modulators[static_cast<size_t> (index)];
        const auto safeThis = juce::Component::SafePointer<RoadCanvas> (this);
        auto content = std::make_unique<ModulatorSettingsComponent> (source, [safeThis, index] (const Modulator& updated)
        {
            if (safeThis == nullptr || ! juce::isPositiveAndBelow (index, static_cast<int> (safeThis->modulators.size()))) return;
            safeThis->modulators[static_cast<size_t> (index)] = updated;
            safeThis->notifyChanged (false);
            safeThis->repaint();
        });
        const auto screen = juce::Point<float> (source.position.x * viewScale + viewOffset.x,
                                                 source.position.y * viewScale + viewOffset.y).roundToInt();
        juce::CallOutBox::launchAsynchronously (std::move (content), juce::Rectangle<int> (14, 14).withCentre (screen), this);
    }

    void showModulationConnectionSettings (int index)
    {
        if (! juce::isPositiveAndBelow (index, static_cast<int> (modulationConnections.size()))) return;
        const auto connection = modulationConnections[static_cast<size_t> (index)];
        const auto safeThis = juce::Component::SafePointer<RoadCanvas> (this);
        auto content = std::make_unique<ModulationConnectionSettingsComponent> (connection, [safeThis, index] (const ModulationConnection& updated)
        {
            if (safeThis == nullptr || ! juce::isPositiveAndBelow (index, static_cast<int> (safeThis->modulationConnections.size()))) return;
            safeThis->modulationConnections[static_cast<size_t> (index)] = updated;
            safeThis->notifyChanged (false);
            safeThis->repaint();
        });
        const auto source = std::find_if (modulators.begin(), modulators.end(), [&] (const auto& mod) { return mod.id == connection.sourceId; });
        const auto target = modulationTargetPosition (connection.targetKind, connection.targetId);
        auto centre = getLocalBounds().getCentre();
        if (source != modulators.end() && target.has_value())
        {
            const auto world = (source->position + *target) * 0.5f;
            centre = juce::Point<float> (world.x * viewScale + viewOffset.x, world.y * viewScale + viewOffset.y).roundToInt();
        }
        juce::CallOutBox::launchAsynchronously (std::move (content), juce::Rectangle<int> (14, 14).withCentre (centre), this);
    }

    juce::StringArray sequencingClockNames() const
    {
        juce::StringArray names { "Master" };
        for (const auto& clock : sequencingClocks)
            names.add (clock.name + (clock.enabled ? juce::String() : " (off)"));
        return names;
    }

    void showTapSettings (int index)
    {
        if (! juce::isPositiveAndBelow (index, static_cast<int> (pipeTaps().size()))) return;
        const auto tap = pipeTaps()[static_cast<size_t> (index)];
        const auto safeThis = juce::Component::SafePointer<RoadCanvas> (this);
        auto content = std::make_unique<TapSettingsComponent> (tap, sequencingClockNames(), [safeThis, index] (const PipeTap& updated)
        {
            if (safeThis == nullptr || ! juce::isPositiveAndBelow (index, static_cast<int> (safeThis->pipeTaps().size()))) return;
            const auto& previous = safeThis->pipeTaps()[static_cast<size_t> (index)];
            const auto emitted = previous.emittedDrops;
            const auto nextBeat = previous.nextEmissionBeat;
            const auto clockChanged = previous.clockIndex != updated.clockIndex;
            safeThis->pipeTaps()[static_cast<size_t> (index)] = updated;
            auto& changedTap = safeThis->pipeTaps()[static_cast<size_t> (index)];
            changedTap.emittedDrops = clockChanged ? 0 : emitted;
            changedTap.nextEmissionBeat = clockChanged && changedTap.clockIndex > 0
                                           && juce::isPositiveAndBelow (changedTap.clockIndex - 1, static_cast<int> (safeThis->sequencingClocks.size()))
                                               ? safeThis->sequencingClocks[static_cast<size_t> (changedTap.clockIndex - 1)].phaseBeats
                                               : (clockChanged ? 0.0 : nextBeat);
            safeThis->notifyChanged();
            safeThis->repaint();
        });
        const auto screenPoint = juce::Point<float> (tap.position.x * viewScale + viewOffset.x, tap.position.y * viewScale + viewOffset.y).roundToInt();
        juce::CallOutBox::launchAsynchronously (std::move (content), juce::Rectangle<int> (12, 12).withCentre (screenPoint), this);
    }

    void showDrainSettings (int index)
    {
        if (! juce::isPositiveAndBelow (index, static_cast<int> (pipeDrains().size()))) return;
        const auto drain = pipeDrains()[static_cast<size_t> (index)];
        const auto safeThis = juce::Component::SafePointer<RoadCanvas> (this);
        auto content = std::make_unique<DrainSettingsComponent> (drain, [safeThis, index] (const PipeDrain& updated)
        {
            if (safeThis == nullptr || ! juce::isPositiveAndBelow (index, static_cast<int> (safeThis->pipeDrains().size()))) return;
            safeThis->pipeDrains()[static_cast<size_t> (index)] = updated;
            safeThis->notifyChanged();
            safeThis->repaint();
        });
        const auto screenPoint = juce::Point<float> (drain.position.x * viewScale + viewOffset.x,
                                                      drain.position.y * viewScale + viewOffset.y).roundToInt();
        juce::CallOutBox::launchAsynchronously (std::move (content), juce::Rectangle<int> (12, 12).withCentre (screenPoint), this);
    }

    void showClonerSettings (int index)
    {
        if (! juce::isPositiveAndBelow (index, static_cast<int> (pipeCloners().size()))) return;
        const auto cloner = pipeCloners()[static_cast<size_t> (index)];
        const auto safeThis = juce::Component::SafePointer<RoadCanvas> (this);
        auto content = std::make_unique<ClonerSettingsComponent> (cloner, [safeThis, index] (const PipeCloner& updated)
        {
            if (safeThis == nullptr || ! juce::isPositiveAndBelow (index, static_cast<int> (safeThis->pipeCloners().size()))) return;
            safeThis->pipeCloners()[static_cast<size_t> (index)] = updated;
            safeThis->notifyChanged();
            safeThis->repaint();
        });
        const auto screenPoint = juce::Point<float> (cloner.position.x * viewScale + viewOffset.x,
                                                      cloner.position.y * viewScale + viewOffset.y).roundToInt();
        juce::CallOutBox::launchAsynchronously (std::move (content), juce::Rectangle<int> (12, 12).withCentre (screenPoint), this);
    }

    void showSpeedLimitSettings (int index)
    {
        if (! juce::isPositiveAndBelow (index, static_cast<int> (pipeSpeedLimits().size()))) return;
        const auto limit = pipeSpeedLimits()[static_cast<size_t> (index)];
        const auto safeThis = juce::Component::SafePointer<RoadCanvas> (this);
        auto content = std::make_unique<SpeedLimitSettingsComponent> (limit, [safeThis, index] (const PipeSpeedLimit& updated)
        {
            if (safeThis == nullptr || ! juce::isPositiveAndBelow (index, static_cast<int> (safeThis->pipeSpeedLimits().size()))) return;
            safeThis->pipeSpeedLimits()[static_cast<size_t> (index)] = updated;
            safeThis->notifyChanged();
            safeThis->repaint();
        });
        const auto screenPoint = juce::Point<float> (limit.position.x * viewScale + viewOffset.x,
                                                      limit.position.y * viewScale + viewOffset.y).roundToInt();
        juce::CallOutBox::launchAsynchronously (std::move (content), juce::Rectangle<int> (12, 12).withCentre (screenPoint), this);
    }

    void showWaitSettings (int index)
    {
        if (! juce::isPositiveAndBelow (index, static_cast<int> (pipeWaits().size()))) return;
        const auto wait = pipeWaits()[static_cast<size_t> (index)];
        const auto safeThis = juce::Component::SafePointer<RoadCanvas> (this);
        auto content = std::make_unique<WaitSettingsComponent> (wait, [safeThis, index] (const PipeWait& updated)
        {
            if (safeThis == nullptr || ! juce::isPositiveAndBelow (index, static_cast<int> (safeThis->pipeWaits().size()))) return;
            safeThis->pipeWaits()[static_cast<size_t> (index)] = updated;
            safeThis->notifyChanged();
            safeThis->repaint();
        });
        const auto screenPoint = juce::Point<float> (wait.position.x * viewScale + viewOffset.x,
                                                      wait.position.y * viewScale + viewOffset.y).roundToInt();
        juce::CallOutBox::launchAsynchronously (std::move (content), juce::Rectangle<int> (12, 12).withCentre (screenPoint), this);
    }

    void showStrikeSettings (int index)
    {
        if (! juce::isPositiveAndBelow (index, static_cast<int> (pipeStrikes().size()))) return;
        const auto strike = pipeStrikes()[static_cast<size_t> (index)];
        const auto safeThis = juce::Component::SafePointer<RoadCanvas> (this);
        auto content = std::make_unique<StrikeSettingsComponent> (strike, [safeThis, index] (const PipeStrike& updated)
        {
            if (safeThis == nullptr || ! juce::isPositiveAndBelow (index, static_cast<int> (safeThis->pipeStrikes().size()))) return;
            safeThis->pipeStrikes()[static_cast<size_t> (index)] = updated; safeThis->notifyChanged(); safeThis->repaint();
        });
        const auto screenPoint = juce::Point<float> (strike.position.x * viewScale + viewOffset.x,
                                                      strike.position.y * viewScale + viewOffset.y).roundToInt();
        juce::CallOutBox::launchAsynchronously (std::move (content), juce::Rectangle<int> (12, 12).withCentre (screenPoint), this);
    }

    void showTeleportSettings (int index)
    {
        if (! juce::isPositiveAndBelow (index, static_cast<int> (pipeTeleports().size()))) return;
        juce::StringArray names, ids;
        for (int i = 0; i < static_cast<int> (pipeTeleports().size()); ++i) if (i != index)
        { names.add ("Teleport " + juce::String (i + 1)); ids.add (pipeTeleports()[static_cast<size_t> (i)].id); }
        const auto teleport = pipeTeleports()[static_cast<size_t> (index)];
        const auto safeThis = juce::Component::SafePointer<RoadCanvas> (this);
        auto content = std::make_unique<TeleportSettingsComponent> (teleport, names, ids, [safeThis, index] (const PipeTeleport& updated)
        {
            if (safeThis == nullptr || ! juce::isPositiveAndBelow (index, static_cast<int> (safeThis->pipeTeleports().size()))) return;
            const auto total = safeThis->pipeTeleports()[static_cast<size_t> (index)].totalTeleported;
            const auto windowCount = safeThis->pipeTeleports()[static_cast<size_t> (index)].windowCount;
            const auto windowStart = safeThis->pipeTeleports()[static_cast<size_t> (index)].windowStartBeat;
            safeThis->pipeTeleports()[static_cast<size_t> (index)] = updated;
            auto& saved = safeThis->pipeTeleports()[static_cast<size_t> (index)]; saved.totalTeleported = total; saved.windowCount = windowCount; saved.windowStartBeat = windowStart;
            safeThis->notifyChanged(); safeThis->repaint();
        });
        const auto screenPoint = juce::Point<float> (teleport.position.x * viewScale + viewOffset.x,
                                                      teleport.position.y * viewScale + viewOffset.y).roundToInt();
        juce::CallOutBox::launchAsynchronously (std::move (content), juce::Rectangle<int> (12, 12).withCentre (screenPoint), this);
    }
    void showFilterSettings (int index)
    {
        if (! juce::isPositiveAndBelow (index, static_cast<int> (pipeFilters().size()))) return;
        const auto filter = pipeFilters()[static_cast<size_t> (index)]; const auto safeThis = juce::Component::SafePointer<RoadCanvas> (this);
        auto content = std::make_unique<FilterSettingsComponent> (filter, [safeThis, index] (const PipeFilter& updated)
        { if (safeThis == nullptr || ! juce::isPositiveAndBelow (index, static_cast<int> (safeThis->pipeFilters().size()))) return; safeThis->pipeFilters()[static_cast<size_t> (index)] = updated; safeThis->notifyChanged(); safeThis->repaint(); });
        const auto screenPoint = juce::Point<float> (filter.position.x * viewScale + viewOffset.x, filter.position.y * viewScale + viewOffset.y).roundToInt();
        juce::CallOutBox::launchAsynchronously (std::move (content), juce::Rectangle<int> (12, 12).withCentre (screenPoint), this);
    }

    void showLogicSettings (int index)
    {
        if (! juce::isPositiveAndBelow (index, static_cast<int> (pipeLogics().size()))) return;
        const auto logic = pipeLogics()[static_cast<size_t> (index)];
        const auto safeThis = juce::Component::SafePointer<RoadCanvas> (this);
        auto content = std::make_unique<LogicSettingsComponent> (logic, [safeThis, index] (const PipeLogic& updated)
        {
            if (safeThis == nullptr || ! juce::isPositiveAndBelow (index, static_cast<int> (safeThis->pipeLogics().size()))) return;
            const auto oldMode = safeThis->pipeLogics()[static_cast<size_t> (index)].mode;
            const auto count = oldMode == updated.mode ? safeThis->pipeLogics()[static_cast<size_t> (index)].count : 0;
            const auto flipState = oldMode == updated.mode ? safeThis->pipeLogics()[static_cast<size_t> (index)].flipState : false;
            safeThis->pipeLogics()[static_cast<size_t> (index)] = updated;
            safeThis->pipeLogics()[static_cast<size_t> (index)].count = count;
            safeThis->pipeLogics()[static_cast<size_t> (index)].flipState = flipState;
            safeThis->notifyChanged();
            safeThis->repaint();
        });
        const auto screenPoint = juce::Point<float> (logic.position.x * viewScale + viewOffset.x,
                                                      logic.position.y * viewScale + viewOffset.y).roundToInt();
        juce::CallOutBox::launchAsynchronously (std::move (content), juce::Rectangle<int> (12, 12).withCentre (screenPoint), this);
    }

    void zoomAt (juce::Point<float> screenPoint, float zoomFactor)
    {
        const auto oldScale = viewScale;
        const auto newScale = juce::jlimit (minViewScale, maxViewScale, oldScale * zoomFactor);

        if (std::abs (newScale - oldScale) <= 0.0001f)
            return;

        const auto worldPoint = screenToWorld (screenPoint);
        viewScale = newScale;
        viewOffset = { screenPoint.x - worldPoint.x * viewScale,
                       screenPoint.y - worldPoint.y * viewScale };
        repaint();
    }

    void panBy (juce::Point<float> delta)
    {
        viewOffset += delta;
        repaint();
    }

    float screenToleranceToWorld (float screenPixels) const
    {
        return screenPixels / viewScale;
    }

    juce::Point<float> snappedPoint (juce::Point<float> raw,
                                     int ignoredRoute = -1,
                                     int ignoredNode = -1) const
    {
        if (! snapEnabled)
            return raw;

        auto bestPoint = raw;
        auto bestDistance = snapRadius;

        for (int routeIndex = 0; routeIndex < static_cast<int> (routes().size()); ++routeIndex)
        {
            const auto& route = routes()[static_cast<size_t> (routeIndex)];

            for (int nodeIndex = 0; nodeIndex < static_cast<int> (route.points.size()); ++nodeIndex)
            {
                if (routeIndex == ignoredRoute && nodeIndex == ignoredNode)
                    continue;

                const auto candidate = route.points[static_cast<size_t> (nodeIndex)];
                const auto distance = raw.getDistanceFrom (candidate);

                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestPoint = candidate;
                }
            }
        }

        if (bestPoint != raw)
            return bestPoint;

        const auto segmentHit = findNearestSegment (raw, snapRadius, ignoredRoute,
                                                    tool == Tool::pipe || tool == Tool::warpPipe ? 1 : -1);

        if (segmentHit.route >= 0)
            return segmentHit.point;

        const auto snapped = snapToGrid (raw);
        return raw.getDistanceFrom (snapped) <= snapRadius ? snapped : raw;
    }

    void appendPoint (juce::Point<float> point)
    {
        if (currentRoute.points.empty())
        {
            currentRoute.points.push_back (point);
            return;
        }

        auto& last = currentRoute.points.back();

        if (last.getDistanceFrom (point) < minPointDistance)
            return;

        currentRoute.points.push_back (point);
    }

    void finishCurrentRoute()
    {
        if (! currentRoute.isDrawable())
        {
            currentRoute.points.clear();
            return;
        }

        simplifyRoute (currentRoute);

        if (currentRoute.isDrawable())
        {
            attachRouteEndpointsToExistingSegments (currentRoute);
            selectedRoute = mergeRouteIntoExistingPaths (currentRoute);

            if (selectedRoute < 0)
            {
                routes().push_back (currentRoute);
                selectedRoute = static_cast<int> (routes().size()) - 1;
            }
            else
            {
                mergeRouteEndpointsWithNeighbours (selectedRoute);
                simplifyRoute (routes()[static_cast<size_t> (selectedRoute)]);
            }

            normalisePipeJunctions (routes());

            notifyChanged();
        }

        currentRoute.points.clear();
    }

    void simplifyRoute (RoadRoute& route)
    {
        if (route.points.size() < 4)
            return;

        std::vector<juce::Point<float>> simplified;
        simplified.reserve (route.points.size());
        simplified.push_back (route.points.front());

        for (size_t i = 1; i + 1 < route.points.size(); ++i)
        {
            const auto previous = simplified.back();
            const auto current = route.points[i];
            const auto next = route.points[i + 1];

            const auto tooClose = previous.getDistanceFrom (current) < 14.0f;
            const auto almostStraight = distanceFromLineSegment (current, previous, next) < 3.5f;
            const auto sharedPipeJunction = route.isPipe && isSharedPipePoint (route, current);

            if (sharedPipeJunction || ! tooClose || ! almostStraight)
                simplified.push_back (current);
        }

        simplified.push_back (route.points.back());
        route.points = std::move (simplified);
    }

    bool isSharedPipePoint (const RoadRoute& route, juce::Point<float> point) const
    {
        for (const auto& other : routes())
        {
            if (&other == &route || ! other.isPipe)
                continue;

            for (const auto otherPoint : other.points)
                if (pointsTouch (point, otherPoint))
                    return true;
        }

        return false;
    }

    static void normalisePipeJunctions (std::vector<RoadRoute>& pipeRoutes)
    {
        struct Endpoint { size_t route = 0; juce::Point<float> point; };
        std::vector<Endpoint> endpoints;

        for (size_t routeIndex = 0; routeIndex < pipeRoutes.size(); ++routeIndex)
        {
            const auto& route = pipeRoutes[routeIndex];
            if (! route.isPipe || route.points.empty())
                continue;

            endpoints.push_back ({ routeIndex, route.points.front() });
            if (! pointsTouch (route.points.front(), route.points.back()))
                endpoints.push_back ({ routeIndex, route.points.back() });
        }

        for (const auto& endpoint : endpoints)
        {
            for (size_t routeIndex = 0; routeIndex < pipeRoutes.size(); ++routeIndex)
            {
                auto& route = pipeRoutes[routeIndex];
                if (routeIndex == endpoint.route || ! route.isPipe)
                    continue;

                for (size_t segment = 1; segment < route.points.size(); ++segment)
                {
                    const auto a = route.points[segment - 1];
                    const auto b = route.points[segment];
                    if (pointsTouch (endpoint.point, a) || pointsTouch (endpoint.point, b))
                        break;

                    if (distanceFromLineSegment (endpoint.point, a, b) <= routeJoinTolerance)
                    {
                        route.points.insert (route.points.begin() + static_cast<std::ptrdiff_t> (segment), endpoint.point);
                        break;
                    }
                }
            }
        }

        // Collapse near-identical nodes left by separate drawing strokes onto one
        // canonical coordinate. Flow transitions can then preserve position exactly.
        for (size_t routeA = 0; routeA < pipeRoutes.size(); ++routeA)
        {
            if (! pipeRoutes[routeA].isPipe) continue;
            for (auto& pointA : pipeRoutes[routeA].points)
                for (size_t routeB = routeA; routeB < pipeRoutes.size(); ++routeB)
                {
                    if (! pipeRoutes[routeB].isPipe) continue;
                    for (auto& pointB : pipeRoutes[routeB].points)
                    {
                        if (&pointA == &pointB) continue;
                        if (pointA.getDistanceFrom (pointB) <= routeJoinTolerance) pointB = pointA;
                    }
                }
        }
    }

    static void normaliseNestedPipeJunctions (std::vector<Disc>& discs)
    {
        for (auto& disc : discs)
        {
            normalisePipeJunctions (disc.nestedRoutes);
            normaliseNestedPipeJunctions (disc.nestedDiscs);
        }
    }

    static bool pointsTouch (juce::Point<float> a, juce::Point<float> b) noexcept
    {
        return a.getDistanceFrom (b) <= routeJoinTolerance;
    }

    void attachRouteEndpointsToExistingSegments (RoadRoute& route)
    {
        attachRouteEndpointToExistingSegment (route, true);
        attachRouteEndpointToExistingSegment (route, false);
    }

    void attachRouteEndpointToExistingSegment (RoadRoute& route, bool start)
    {
        if (route.points.empty())
            return;

        auto& endpoint = start ? route.points.front() : route.points.back();
        const auto hit = findNearestSegment (endpoint, routeJoinTolerance, -1, route.isPipe ? 1 : 0);

        if (hit.route < 0)
            return;

        auto& target = routes()[static_cast<size_t> (hit.route)];
        if (route.isPipe) route.colour = target.colour;

        if (! juce::isPositiveAndBelow (hit.insertIndex, static_cast<int> (target.points.size())))
            return;

        auto& before = target.points[static_cast<size_t> (hit.insertIndex - 1)];
        auto& after = target.points[static_cast<size_t> (hit.insertIndex)];

        if (pointsTouch (endpoint, before))
        {
            endpoint = before;
            return;
        }

        if (pointsTouch (endpoint, after))
        {
            endpoint = after;
            return;
        }

        endpoint = hit.point;
        target.points.insert (target.points.begin() + hit.insertIndex, hit.point);
    }

    static void appendWithoutDuplicate (std::vector<juce::Point<float>>& destination,
                                        const std::vector<juce::Point<float>>& source,
                                        bool reverseSource,
                                        bool skipFirst)
    {
        if (source.empty())
            return;

        if (reverseSource)
        {
            auto index = static_cast<int> (source.size()) - 1 - (skipFirst ? 1 : 0);

            for (; index >= 0; --index)
                if (destination.empty() || ! pointsTouch (destination.back(), source[static_cast<size_t> (index)]))
                    destination.push_back (source[static_cast<size_t> (index)]);

            return;
        }

        for (size_t i = skipFirst ? 1u : 0u; i < source.size(); ++i)
            if (destination.empty() || ! pointsTouch (destination.back(), source[i]))
                destination.push_back (source[i]);
    }

    static bool tryMergeRoutes (RoadRoute& existing, const RoadRoute& incoming)
    {
        if (! existing.isDrawable() || ! incoming.isDrawable())
            return false;

        if (existing.isPipe != incoming.isPipe || existing.isWarpPipe != incoming.isWarpPipe)
            return false;

        if (pointsTouch (existing.points.back(), incoming.points.front()))
        {
            existing.points.back() = incoming.points.front();
            appendWithoutDuplicate (existing.points, incoming.points, false, true);
            return true;
        }

        if (pointsTouch (existing.points.front(), incoming.points.back()))
        {
            std::vector<juce::Point<float>> merged;
            merged.reserve (existing.points.size() + incoming.points.size());
            appendWithoutDuplicate (merged, incoming.points, false, false);
            appendWithoutDuplicate (merged, existing.points, false, true);
            existing.points = std::move (merged);
            return true;
        }

        if (pointsTouch (existing.points.back(), incoming.points.back()))
        {
            existing.points.back() = incoming.points.back();
            appendWithoutDuplicate (existing.points, incoming.points, true, true);
            return true;
        }

        if (pointsTouch (existing.points.front(), incoming.points.front()))
        {
            std::vector<juce::Point<float>> merged;
            merged.reserve (existing.points.size() + incoming.points.size());
            appendWithoutDuplicate (merged, incoming.points, true, false);
            appendWithoutDuplicate (merged, existing.points, false, true);
            existing.points = std::move (merged);
            return true;
        }

        return false;
    }

    int mergeRouteIntoExistingPaths (const RoadRoute& route)
    {
        for (int routeIndex = 0; routeIndex < static_cast<int> (routes().size()); ++routeIndex)
        {
            struct PulsePosition { int pulseIndex = -1; juce::Point<float> position; juce::Point<float> direction; };
            std::vector<PulsePosition> pulsePositions;
            const auto& existingBefore = routes()[static_cast<size_t> (routeIndex)];
            for (int pulseIndex = 0; pulseIndex < static_cast<int> (flowPulses.size()); ++pulseIndex)
            {
                const auto& pulse = flowPulses[static_cast<size_t> (pulseIndex)];
                if (pulse.routeIndex != routeIndex) continue;
                const auto position = pointAlongRoute (existingBefore, pulse.distance);
                const auto probeDistance = juce::jlimit (0.0f, existingBefore.getLength(), pulse.distance + (pulse.reverse ? -1.0f : 1.0f));
                pulsePositions.push_back ({ pulseIndex, position, pointAlongRoute (existingBefore, probeDistance) - position });
            }
            if (tryMergeRoutes (routes()[static_cast<size_t> (routeIndex)], route))
            {
                const auto& merged = routes()[static_cast<size_t> (routeIndex)];
                for (const auto& snapshot : pulsePositions)
                {
                    auto& pulse = flowPulses[static_cast<size_t> (snapshot.pulseIndex)];
                    pulse.distance = distanceAlongRouteForPoint (merged, snapshot.position);
                    const auto forwardProbe = pointAlongRoute (merged, juce::jlimit (0.0f, merged.getLength(), pulse.distance + 1.0f)) - snapshot.position;
                    pulse.reverse = snapshot.direction.x * forwardProbe.x + snapshot.direction.y * forwardProbe.y < 0.0f;
                }
                return routeIndex;
            }
        }

        return -1;
    }

    void eraseRouteAndPulses (int routeIndex)
    {
        if (! juce::isPositiveAndBelow (routeIndex, static_cast<int> (routes().size()))) return;
        for (auto& pulse : flowPulses)
        {
            if (pulse.routeIndex == routeIndex) pulse.routeIndex = -1;
            else if (pulse.routeIndex > routeIndex) --pulse.routeIndex;
        }
        routes().erase (routes().begin() + routeIndex);
    }

    void mergeRouteEndpointsWithNeighbours (int routeIndex)
    {
        if (! juce::isPositiveAndBelow (routeIndex, static_cast<int> (routes().size())))
            return;

        bool mergedAny = true;

        while (mergedAny)
        {
            mergedAny = false;

            for (int otherIndex = 0; otherIndex < static_cast<int> (routes().size()); ++otherIndex)
            {
                if (otherIndex == routeIndex)
                    continue;

                struct PulsePosition { int pulseIndex = -1; juce::Point<float> position; juce::Point<float> direction; };
                std::vector<PulsePosition> pulsePositions;
                for (int pulseIndex = 0; pulseIndex < static_cast<int> (flowPulses.size()); ++pulseIndex)
                {
                    const auto& pulse = flowPulses[static_cast<size_t> (pulseIndex)];
                    if (pulse.routeIndex != routeIndex && pulse.routeIndex != otherIndex) continue;
                    const auto& oldRoute = routes()[static_cast<size_t> (pulse.routeIndex)];
                    const auto position = pointAlongRoute (oldRoute, pulse.distance);
                    const auto probeDistance = juce::jlimit (0.0f, oldRoute.getLength(), pulse.distance + (pulse.reverse ? -1.0f : 1.0f));
                    pulsePositions.push_back ({ pulseIndex, position, pointAlongRoute (oldRoute, probeDistance) - position });
                }

                if (tryMergeRoutes (routes()[static_cast<size_t> (routeIndex)],
                                    routes()[static_cast<size_t> (otherIndex)]))
                {
                    const auto& merged = routes()[static_cast<size_t> (routeIndex)];
                    for (const auto& snapshot : pulsePositions)
                    {
                        auto& pulse = flowPulses[static_cast<size_t> (snapshot.pulseIndex)];
                        pulse.routeIndex = routeIndex;
                        pulse.distance = distanceAlongRouteForPoint (merged, snapshot.position);
                        const auto forwardProbe = pointAlongRoute (merged, juce::jlimit (0.0f, merged.getLength(), pulse.distance + 1.0f)) - snapshot.position;
                        pulse.reverse = snapshot.direction.x * forwardProbe.x + snapshot.direction.y * forwardProbe.y < 0.0f;
                    }
                    routes().erase (routes().begin() + otherIndex);
                    for (auto& pulse : flowPulses) if (pulse.routeIndex > otherIndex) --pulse.routeIndex;

                    if (otherIndex < routeIndex)
                        --routeIndex;

                    selectedRoute = routeIndex;
                    mergedAny = true;
                    break;
                }
            }
        }
    }

    static juce::Path createPath (const RoadRoute& route)
    {
        juce::Path path;

        if (route.points.empty())
            return path;

        path.startNewSubPath (route.points.front());

        if (route.points.size() == 2)
        {
            path.lineTo (route.points.back());
            return path;
        }

        for (size_t i = 1; i < route.points.size(); ++i)
            path.lineTo (route.points[i]);

        return path;
    }

    static void addRoutePaintItem (std::vector<RoutePaintItem>& items,
                                   const RoadRoute& route,
                                   bool selected,
                                   float alpha)
    {
        auto& item = items.emplace_back();
        item.path = createPath (route);
        item.bounds = item.path.getBounds().expanded (12.0f);
        item.selected = selected;
        item.pipe = route.isPipe;
        item.warpPipe = route.isWarpPipe;
        item.colour = route.colour;
        item.alpha = alpha;

        if (item.bounds.isEmpty())
            items.pop_back();
    }

    void drawRoute (juce::Graphics& g, const RoadRoute& route, bool selected, float alpha) const
    {
        std::vector<RoutePaintItem> items;
        addRoutePaintItem (items, route, selected, alpha);

        if (items.empty())
            return;

        if (route.isWarpPipe)
        {
            const float dashes[] { 2.2f, 6.0f };
            juce::Path shadow, glow, core;
            juce::PathStrokeType (lineWidth + 5.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded)
                .createDashedStroke (shadow, items.front().path, dashes, 2);
            juce::PathStrokeType (lineWidth + 2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded)
                .createDashedStroke (glow, items.front().path, dashes, 2);
            juce::PathStrokeType (lineWidth, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded)
                .createDashedStroke (core, items.front().path, dashes, 2);
            g.setColour (juce::Colour (0xff020807).withAlpha (0.58f * alpha)); g.fillPath (shadow);
            g.setColour (route.colour.withAlpha ((selected ? 0.72f : 0.48f) * alpha)); g.fillPath (glow);
            g.setColour (route.colour.brighter (0.25f).withAlpha (0.96f * alpha)); g.fillPath (core);
            return;
        }

        drawRouteLayer (g, items.front(), RouteLayer::shadow);
        drawRouteLayer (g, items.front(), RouteLayer::glow);
        drawRouteLayer (g, items.front(), RouteLayer::core);
        drawRouteLayer (g, items.front(), RouteLayer::shine);
    }

    void drawRouteLayer (juce::Graphics& g, const RoutePaintItem& item, RouteLayer layer) const
    {
        if (item.bounds.isEmpty())
            return;

        const auto glowAlpha = item.selected ? 0.64f : 0.42f;
        const auto pipeColour = item.colour.isTransparent() ? pipeColourForIndex (0) : item.colour;
        const auto coreStart = item.pipe ? pipeColour : lineColour();
        const auto coreEnd = item.pipe ? pipeColour.brighter (0.34f) : lineLightColour().interpolatedWith (lineColour(), item.selected ? 0.18f : 0.38f);

        if (layer == RouteLayer::shadow)
        {
            g.setColour (juce::Colour (0xff020807).withAlpha (0.58f * item.alpha));
            g.strokePath (item.path, juce::PathStrokeType (lineWidth + 5.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
            return;
        }

        if (layer == RouteLayer::glow)
        {
            juce::ColourGradient glow ((item.pipe ? pipeColour.darker (0.42f) : (item.selected ? accentColour() : lineDeepColour())).withAlpha (glowAlpha * item.alpha),
                                       item.bounds.getTopLeft(),
                                       lineColour().withAlpha ((item.selected ? 0.44f : 0.30f) * item.alpha),
                                       item.bounds.getBottomRight(),
                                       false);
            g.setGradientFill (glow);
            g.strokePath (item.path, juce::PathStrokeType (lineWidth + 2.2f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
            return;
        }

        if (layer == RouteLayer::core)
        {
            juce::ColourGradient core (coreStart.withAlpha (0.96f * item.alpha), item.bounds.getTopLeft(),
                                       coreEnd.withAlpha (0.96f * item.alpha), item.bounds.getBottomRight(), false);
            if (item.pipe)
            {
                const auto startIndex = pipePaletteIndex (pipeColour);
                for (int i = 1; i < 7; ++i)
                    core.addColour (static_cast<double> (i) / 7.0, pipeColourForIndex (startIndex + i).withAlpha (0.96f * item.alpha));
            }
            g.setGradientFill (core);
            g.strokePath (item.path, juce::PathStrokeType (lineWidth, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
            return;
        }

        g.setColour (juce::Colours::white.withAlpha ((item.selected ? 0.34f : 0.20f) * item.alpha));
        g.strokePath (item.path, juce::PathStrokeType (1.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
    }

    static juce::Point<float> pointAlongRoute (const RoadRoute& route, float distance)
    {
        if (route.points.empty()) return {};
        for (size_t i = 1; i < route.points.size(); ++i)
        {
            const auto segmentLength = route.points[i - 1].getDistanceFrom (route.points[i]);
            if (distance <= segmentLength)
                return route.points[i - 1] + (route.points[i] - route.points[i - 1]) * (segmentLength > 0.0f ? distance / segmentLength : 0.0f);
            distance -= segmentLength;
        }
        return route.points.back();
    }

    static float distanceAlongRouteForPoint (const RoadRoute& route, juce::Point<float> point)
    {
        float travelled = 0.0f;
        float bestDistance = std::numeric_limits<float>::max();
        float bestAlong = 0.0f;
        for (size_t i = 1; i < route.points.size(); ++i)
        {
            const auto a = route.points[i - 1];
            const auto b = route.points[i];
            const auto delta = b - a;
            const auto lengthSquared = delta.x * delta.x + delta.y * delta.y;
            const auto t = lengthSquared > 0.0f ? juce::jlimit (0.0f, 1.0f, ((point - a).x * delta.x + (point - a).y * delta.y) / lengthSquared) : 0.0f;
            const auto closest = a + delta * t;
            const auto distance = closest.getDistanceFrom (point);
            const auto segmentLength = a.getDistanceFrom (b);
            if (distance < bestDistance) { bestDistance = distance; bestAlong = travelled + segmentLength * t; }
            travelled += segmentLength;
        }
        return bestAlong;
    }

    static std::vector<float> routeNodeDistances (const RoadRoute& route)
    {
        std::vector<float> distances (route.points.size(), 0.0f);
        for (size_t i = 1; i < route.points.size(); ++i)
            distances[i] = distances[i - 1] + route.points[i - 1].getDistanceFrom (route.points[i]);
        return distances;
    }

    struct JunctionWay { int routeIndex = -1; int fromNode = -1; int toNode = -1; };

    std::vector<JunctionWay> outgoingWays (juce::Point<float> junction, int incomingRoute, int incomingFromNode, int incomingToNode) const
    {
        std::vector<JunctionWay> ways;
        juce::Point<float> incomingBack;
        if (juce::isPositiveAndBelow (incomingRoute, static_cast<int> (routes().size())))
        {
            const auto& route = routes()[static_cast<size_t> (incomingRoute)];
            if (juce::isPositiveAndBelow (incomingFromNode, static_cast<int> (route.points.size())))
                incomingBack = route.points[static_cast<size_t> (incomingFromNode)] - junction;
        }
        const auto incomingLength = std::sqrt (incomingBack.x * incomingBack.x + incomingBack.y * incomingBack.y);
        for (int routeIndex = 0; routeIndex < static_cast<int> (routes().size()); ++routeIndex)
        {
            const auto& route = routes()[static_cast<size_t> (routeIndex)];
            if (! route.isPipe) continue;
            for (int node = 0; node < static_cast<int> (route.points.size()); ++node)
            {
                if (! pointsTouch (route.points[static_cast<size_t> (node)], junction)) continue;
                for (const auto adjacent : { node - 1, node + 1 })
                {
                    if (! juce::isPositiveAndBelow (adjacent, static_cast<int> (route.points.size()))) continue;
                    const auto isIncomingEdge = routeIndex == incomingRoute
                                             && ((node == incomingToNode && adjacent == incomingFromNode)
                                                 || (node == incomingFromNode && adjacent == incomingToNode));
                    if (isIncomingEdge) continue;

                    const auto direction = route.points[static_cast<size_t> (adjacent)] - junction;
                    const auto length = std::sqrt (direction.x * direction.x + direction.y * direction.y);
                    if (length <= 0.001f) continue;

                    // Separate strokes can contain the same physical edge. Do not
                    // count a duplicate of the incoming edge as a random U-turn.
                    if (incomingLength > 0.001f)
                    {
                        const auto alignment = (direction.x * incomingBack.x + direction.y * incomingBack.y) / (length * incomingLength);
                        if (alignment > 0.999f) continue;
                    }

                    const auto duplicateDirection = std::any_of (ways.begin(), ways.end(), [&] (const auto& way)
                    {
                        const auto& existingRoute = routes()[static_cast<size_t> (way.routeIndex)];
                        const auto existing = existingRoute.points[static_cast<size_t> (way.toNode)] - junction;
                        const auto existingLength = std::sqrt (existing.x * existing.x + existing.y * existing.y);
                        return existingLength > 0.001f
                            && (direction.x * existing.x + direction.y * existing.y) / (length * existingLength) > 0.999f;
                    });
                    if (! duplicateDirection) ways.push_back ({ routeIndex, node, adjacent });
                }
            }
        }
        return ways;
    }

    int logicAt (juce::Point<float> position) const
    {
        for (int i = static_cast<int> (pipeLogics().size()) - 1; i >= 0; --i)
            if (pipeLogics()[static_cast<size_t> (i)].enabled
                && pipeLogics()[static_cast<size_t> (i)].position.getDistanceFrom (position) <= gridSize * 0.38f)
                return i;
        return -1;
    }

    bool logicPasses (PipeLogic& logic, FlowPulse& pulse, int outgoingCount)
    {
        switch (logic.mode)
        {
            case PipeLogic::Mode::gate:
                return hasModulationConnection (ModulationTargetKind::logic, logic.id, 0)
                           ? modulationSignal (ModulationTargetKind::logic, logic.id, 0) >= 0.0
                           : logic.gateOpen;
            case PipeLogic::Mode::counter:
            {
                const auto target = modulatedCountValue (logic.targetCount, ModulationTargetKind::logic, logic.id, 2, 1, 64);
                logic.count = juce::jmin (target, logic.count + 1);
                return logic.count >= target;
            }
            case PipeLogic::Mode::switcher:
                return true;
            case PipeLogic::Mode::comparator:
            {
                constexpr double epsilon = 0.0001;
                const auto compareSpeed = modulatedRatioValue (logic.compareSpeed, ModulationTargetKind::logic, logic.id, 1, 0.125, 4.0);
                if (logic.comparison == PipeLogic::Comparison::less) return pulse.speed < compareSpeed;
                if (logic.comparison == PipeLogic::Comparison::lessOrEqual) return pulse.speed <= compareSpeed;
                if (logic.comparison == PipeLogic::Comparison::equal) return std::abs (pulse.speed - compareSpeed) <= epsilon;
                if (logic.comparison == PipeLogic::Comparison::greaterOrEqual) return pulse.speed >= compareSpeed;
                return pulse.speed > compareSpeed;
            }
            case PipeLogic::Mode::flipFlop:
                logic.flipState = ! logic.flipState;
                return outgoingCount > 1 || logic.flipState;
            case PipeLogic::Mode::everyNth:
                logic.count = logic.count >= logic.targetCount ? 1 : logic.count + 1;
                return logic.count == logic.targetCount;
        }
        return true;
    }

    int chooseOutgoingWay (FlowPulse& pulse, juce::Point<float> junction, int incomingRoute,
                           int incomingFromNode, const std::vector<JunctionWay>& ways)
    {
        if (ways.empty()) return -1;

        const auto logicIndex = logicAt (junction);
        if (logicIndex < 0 || logicIndex == pulse.lastLogic)
            return juce::Random::getSystemRandom().nextInt (static_cast<int> (ways.size()));

        auto& logic = pipeLogics()[static_cast<size_t> (logicIndex)];
        pulse.lastLogic = logicIndex;
        if (! logicPasses (logic, pulse, static_cast<int> (ways.size())))
        {
            if (flowDebugVisible)
                flowDebugEvents.push_back ({ junction, "Stopped by Logic", juce::Time::getMillisecondCounterHiRes() + 1400.0 });
            return -1;
        }

        if (logic.mode != PipeLogic::Mode::switcher && logic.mode != PipeLogic::Mode::flipFlop)
            return juce::Random::getSystemRandom().nextInt (static_cast<int> (ways.size()));

        if (logic.mode == PipeLogic::Mode::switcher && logic.branch == PipeLogic::Branch::random)
            return juce::Random::getSystemRandom().nextInt (static_cast<int> (ways.size()));

        juce::Point<float> forward { 1.0f, 0.0f };
        if (juce::isPositiveAndBelow (incomingRoute, static_cast<int> (routes().size())))
        {
            const auto& incoming = routes()[static_cast<size_t> (incomingRoute)];
            if (juce::isPositiveAndBelow (incomingFromNode, static_cast<int> (incoming.points.size())))
            {
                forward = junction - incoming.points[static_cast<size_t> (incomingFromNode)];
                const auto length = std::sqrt (forward.x * forward.x + forward.y * forward.y);
                if (length > 0.001f) forward /= length;
            }
        }

        std::vector<std::pair<float, int>> angles;
        angles.reserve (ways.size());
        for (int i = 0; i < static_cast<int> (ways.size()); ++i)
        {
            const auto& way = ways[static_cast<size_t> (i)];
            auto direction = routes()[static_cast<size_t> (way.routeIndex)].points[static_cast<size_t> (way.toNode)] - junction;
            const auto length = std::sqrt (direction.x * direction.x + direction.y * direction.y);
            if (length > 0.001f) direction /= length;
            const auto cross = forward.x * direction.y - forward.y * direction.x;
            const auto dot = forward.x * direction.x + forward.y * direction.y;
            angles.push_back ({ std::atan2 (cross, dot), i });
        }
        std::sort (angles.begin(), angles.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });

        if (logic.mode == PipeLogic::Mode::flipFlop)
            return logic.flipState ? angles.front().second : angles.back().second;
        if (logic.branch == PipeLogic::Branch::left) return angles.front().second;
        if (logic.branch == PipeLogic::Branch::right) return angles.back().second;
        return std::min_element (angles.begin(), angles.end(), [] (const auto& a, const auto& b)
        { return std::abs (a.first) < std::abs (b.first); })->second;
    }

    void advanceFlowPulse (FlowPulse& pulse, float amount)
    {
        constexpr float epsilon = 0.001f;
        for (int guard = 0; guard < 64 && amount > epsilon; ++guard)
        {
            if (! juce::isPositiveAndBelow (pulse.routeIndex, static_cast<int> (routes().size()))) return;
            const auto& route = routes()[static_cast<size_t> (pulse.routeIndex)];
            const auto nodeDistances = routeNodeDistances (route);
            if (nodeDistances.size() < 2) return;
            const auto direction = pulse.reverse ? -1 : 1;
            if (route.isWarpPipe)
            {
                const auto destinationNode = direction > 0 ? static_cast<int> (route.points.size()) - 1 : 0;
                pulse.distance = nodeDistances[static_cast<size_t> (destinationNode)];
                const auto incomingNode = destinationNode - direction;
                auto ways = outgoingWays (route.points[static_cast<size_t> (destinationNode)], pulse.routeIndex,
                                          incomingNode, destinationNode);
                if (ways.empty())
                {
                    pulse.routeIndex = -1;
                    return;
                }
                const auto chosenIndex = chooseOutgoingWay (pulse, route.points[static_cast<size_t> (destinationNode)],
                                                            pulse.routeIndex, incomingNode, ways);
                if (chosenIndex < 0) { pulse.routeIndex = -1; return; }
                const auto& chosen = ways[static_cast<size_t> (chosenIndex)];
                pulse.routeIndex = chosen.routeIndex;
                pulse.reverse = chosen.toNode < chosen.fromNode;
                pulse.distance = routeNodeDistances (routes()[static_cast<size_t> (chosen.routeIndex)])[static_cast<size_t> (chosen.fromNode)];
                continue;
            }
            auto nextNode = -1;
            if (direction > 0)
            {
                for (int i = 0; i < static_cast<int> (nodeDistances.size()); ++i)
                    if (nodeDistances[static_cast<size_t> (i)] > pulse.distance + epsilon) { nextNode = i; break; }
            }
            else
            {
                for (int i = static_cast<int> (nodeDistances.size()) - 1; i >= 0; --i)
                    if (nodeDistances[static_cast<size_t> (i)] < pulse.distance - epsilon) { nextNode = i; break; }
            }

            if (nextNode < 0)
            {
                pulse.routeIndex = -1;
                return;
            }

            const auto nodeDistance = nodeDistances[static_cast<size_t> (nextNode)];
            const auto travel = std::abs (nodeDistance - pulse.distance);
            if (amount < travel)
            {
                pulse.distance += static_cast<float> (direction) * amount;
                return;
            }

            pulse.distance = nodeDistance;
            amount -= travel;
            const auto incomingFromNode = nextNode - direction;
            auto ways = outgoingWays (route.points[static_cast<size_t> (nextNode)], pulse.routeIndex, incomingFromNode, nextNode);
            if (ways.empty())
            {
                pulse.routeIndex = -1;
                return;
            }

            const auto chosenIndex = chooseOutgoingWay (pulse, route.points[static_cast<size_t> (nextNode)],
                                                        pulse.routeIndex, incomingFromNode, ways);
            if (chosenIndex < 0) { pulse.routeIndex = -1; return; }
            const auto& chosen = ways[static_cast<size_t> (chosenIndex)];
            pulse.routeIndex = chosen.routeIndex;
            pulse.reverse = chosen.toNode < chosen.fromNode;
            pulse.distance = routeNodeDistances (routes()[static_cast<size_t> (chosen.routeIndex)])[static_cast<size_t> (chosen.fromNode)];
        }
    }

    void resetFlowPulses()
    {
        flowPulses.clear();
        selectedFlowPulse = -1;
        flowBeatPosition = 0.0;
        for (auto& teleport : pipeTeleports())
        { teleport.totalTeleported = 0; teleport.windowCount = 0; teleport.windowStartBeat = 0.0; }
        for (auto& tap : pipeTaps())
        {
            tap.emittedDrops = 0;
            tap.nextEmissionBeat = tap.clockIndex > 0 && juce::isPositiveAndBelow (tap.clockIndex - 1, static_cast<int> (sequencingClocks.size()))
                                     ? sequencingClocks[static_cast<size_t> (tap.clockIndex - 1)].phaseBeats
                                     : 0.0;
        }
        for (auto& logic : pipeLogics())
        { logic.count = 0; logic.flipState = false; }
    }

    void emitDueTapDrops()
    {
        for (auto& tap : pipeTaps())
        {
            if (! tap.enabled || (tap.totalDrops > 0 && tap.emittedDrops >= tap.totalDrops)) continue;
            const SequencingClock* clock = nullptr;
            if (tap.clockIndex > 0 && juce::isPositiveAndBelow (tap.clockIndex - 1, static_cast<int> (sequencingClocks.size())))
                clock = &sequencingClocks[static_cast<size_t> (tap.clockIndex - 1)];
            if (clock != nullptr && ! clock->enabled) continue;

            const auto clockBeatPosition = flowBeatPosition * (clock != nullptr ? clock->ratio : 1.0);
            const auto nominalInterval = modulatedRatioValue (
                tap.randomInterval ? (tap.intervalLowBeats + tap.intervalHighBeats) * 0.5 : tap.intervalBeats,
                ModulationTargetKind::tap, tap.id, 2, 0.125, 32.0);
            const auto swingDelay = [&]
            {
                if (clock == nullptr || (tap.emittedDrops % 2) == 0) return 0.0;
                return nominalInterval * clock->swing;
            };

            for (int guard = 0; guard < 64 && clockBeatPosition + 0.0001 >= tap.nextEmissionBeat + swingDelay(); ++guard)
            {
                if (tap.totalDrops > 0 && tap.emittedDrops >= tap.totalDrops) break;
                const auto hit = findNearestSegment (tap.position, gridSize * 0.55f, -1, 1);
                if (hit.route < 0) break;
                const auto distance = distanceAlongRouteForPoint (routes()[static_cast<size_t> (hit.route)], tap.position);
                const auto baseSpeed = tap.randomSpeed
                    ? juce::jmap (juce::Random::getSystemRandom().nextDouble(),
                                  juce::jlimit (0.25, 4.0, tap.speedLow),
                                  juce::jlimit (tap.speedLow, 4.0, tap.speedHigh))
                    : juce::jlimit (0.25, 4.0, tap.speed);
                const auto dropSpeed = modulatedRatioValue (baseSpeed, ModulationTargetKind::tap, tap.id, 1, 0.25, 4.0);
                const auto dropChance = modulatedUnitValue (tap.probability, ModulationTargetKind::tap, tap.id, 0);
                flowPulses.push_back ({ hit.route, distance, -1, dropSpeed, dropChance, tap.reverse });
                ++tap.emittedDrops;
                const auto interval = tap.randomInterval
                    ? juce::jmap (juce::Random::getSystemRandom().nextDouble(),
                                  juce::jlimit (0.25, 32.0, tap.intervalLowBeats),
                                  juce::jlimit (tap.intervalLowBeats, 32.0, tap.intervalHighBeats))
                    : juce::jlimit (0.25, 8.0, tap.intervalBeats);
                tap.nextEmissionBeat += modulatedRatioValue (interval, ModulationTargetKind::tap, tap.id, 2, 0.125, 32.0);
            }
        }
    }

    void timerCallback() override
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        const auto elapsed = lastFlowTimeMs > 0.0 ? juce::jlimit (0.0, 0.1, (now - lastFlowTimeMs) / 1000.0) : 0.0;
        lastFlowTimeMs = now;
        if (! flowRunning || elapsed <= 0.0) return;
        flowBeatPosition += elapsed * flowBpm / 60.0;
        emitDueTapDrops();
        // Drop velocity is beat-relative: 1x always travels one grid square per beat.
        const auto secondsPerBeat = 60.0 / flowBpm;
        const auto advance = static_cast<float> (elapsed * gridSize / secondsPerBeat);
        std::vector<FlowPulse> spawnedPulses;
        for (auto& pulse : flowPulses)
        {
            if (! juce::isPositiveAndBelow (pulse.routeIndex, static_cast<int> (routes().size()))) continue;
            if (pulse.heldByDisc >= 0)
            {
                const auto stillPlaying = onDiscPlaybackActive
                    && onDiscPlaybackActive (discKeyForCurrentWorld (pulse.heldByDisc));
                if (stillPlaying) continue;
                pulse.heldByDisc = -1;
            }
            if (pulse.waitBeatsRemaining > 0.0)
            {
                pulse.waitBeatsRemaining = juce::jmax (0.0, pulse.waitBeatsRemaining - elapsed * flowBpm / 60.0);
                continue;
            }
            advanceFlowPulse (pulse, advance * static_cast<float> (pulse.speed));
            if (! juce::isPositiveAndBelow (pulse.routeIndex, static_cast<int> (routes().size()))) continue;
            const auto& route = routes()[static_cast<size_t> (pulse.routeIndex)];
            auto position = pointAlongRoute (route, pulse.distance);
            auto hitDrain = -1;
            for (int i = 0; i < static_cast<int> (pipeDrains().size()); ++i)
                if (pipeDrains()[static_cast<size_t> (i)].enabled && pipeDrains()[static_cast<size_t> (i)].position.getDistanceFrom (position) <= gridSize * 0.38f)
                {
                    hitDrain = i;
                    break;
                }
            if (hitDrain >= 0 && hitDrain != pulse.lastDrain
                && juce::Random::getSystemRandom().nextDouble() < modulatedUnitValue (
                       pipeDrains()[static_cast<size_t> (hitDrain)].destructionProbability,
                       ModulationTargetKind::drain, pipeDrains()[static_cast<size_t> (hitDrain)].id, 0))
            {
                if (flowDebugVisible) flowDebugEvents.push_back ({ position, "Destroyed by Drain", now + 1400.0 });
                pulse.routeIndex = -1;
                continue;
            }
            pulse.lastDrain = hitDrain;

            auto hitSpeedLimit = -1;
            for (int i = 0; i < static_cast<int> (pipeSpeedLimits().size()); ++i)
                if (pipeSpeedLimits()[static_cast<size_t> (i)].enabled && pipeSpeedLimits()[static_cast<size_t> (i)].position.getDistanceFrom (position) <= gridSize * 0.38f)
                {
                    hitSpeedLimit = i;
                    break;
                }
            if (hitSpeedLimit >= 0 && hitSpeedLimit != pulse.lastSpeedLimit)
            {
                const auto& limit = pipeSpeedLimits()[static_cast<size_t> (hitSpeedLimit)];
                if (juce::Random::getSystemRandom().nextDouble()
                    < modulatedUnitValue (limit.affectProbability, ModulationTargetKind::speedLimit, limit.id, 1))
                    pulse.speed = modulatedRatioValue (limit.bpmMultiplier, ModulationTargetKind::speedLimit, limit.id, 0, 0.125, 4.0);
            }
            pulse.lastSpeedLimit = hitSpeedLimit;

            auto hitFilter = -1;
            for (int i = 0; i < static_cast<int> (pipeFilters().size()); ++i)
                if (pipeFilters()[static_cast<size_t> (i)].enabled && pipeFilters()[static_cast<size_t> (i)].position.getDistanceFrom (position) <= gridSize * 0.38f) { hitFilter = i; break; }
            if (hitFilter >= 0 && hitFilter != pulse.lastFilter)
            {
                const auto& filter = pipeFilters()[static_cast<size_t> (hitFilter)];
                const auto low = modulatedRatioValue (filter.lowSpeed, ModulationTargetKind::filter, filter.id, 0, 0.125, 4.0);
                const auto high = modulatedRatioValue (filter.highSpeed, ModulationTargetKind::filter, filter.id, 1, low, 4.0);
                const auto passes = filter.mode == PipeFilter::Mode::highpass ? pulse.speed >= low
                                  : filter.mode == PipeFilter::Mode::lowpass ? pulse.speed <= high
                                  : pulse.speed >= low && pulse.speed <= high;
                if (! passes)
                {
                    if (flowDebugVisible) flowDebugEvents.push_back ({ position, "Removed by Speed Filter", now + 1400.0 });
                    pulse.routeIndex = -1; continue;
                }
            }
            pulse.lastFilter = hitFilter;

            const auto hitLogic = logicAt (position);
            if (hitLogic >= 0 && hitLogic != pulse.lastLogic)
            {
                auto& logic = pipeLogics()[static_cast<size_t> (hitLogic)];
                // Switches choose an exit at junctions. Away from a junction they
                // simply pass, while flip-flops alternate pass/block.
                const auto outgoingCount = logic.mode == PipeLogic::Mode::switcher ? 2 : 1;
                if (! logicPasses (logic, pulse, outgoingCount))
                {
                    if (flowDebugVisible) flowDebugEvents.push_back ({ position, "Stopped by Logic", now + 1400.0 });
                    pulse.routeIndex = -1;
                    continue;
                }
            }
            pulse.lastLogic = hitLogic;

            auto hitWait = -1;
            for (int i = 0; i < static_cast<int> (pipeWaits().size()); ++i)
                if (pipeWaits()[static_cast<size_t> (i)].enabled && pipeWaits()[static_cast<size_t> (i)].position.getDistanceFrom (position) <= gridSize * 0.38f)
                {
                    hitWait = i;
                    break;
                }
            if (hitWait >= 0 && hitWait != pulse.lastWait)
            {
                const auto& wait = pipeWaits()[static_cast<size_t> (hitWait)];
                pulse.waitBeatsRemaining = modulatedRatioValue (wait.beats, ModulationTargetKind::wait, wait.id, 0, 0.25, 32.0);
            }
            pulse.lastWait = hitWait;

            auto hitStrike = -1;
            for (int i = 0; i < static_cast<int> (pipeStrikes().size()); ++i)
                if (pipeStrikes()[static_cast<size_t> (i)].enabled && pipeStrikes()[static_cast<size_t> (i)].position.getDistanceFrom (position) <= gridSize * 0.38f)
                { hitStrike = i; break; }
            if (hitStrike >= 0 && hitStrike != pulse.lastStrike && onDiscFlowTriggered)
            {
                const auto& strike = pipeStrikes()[static_cast<size_t> (hitStrike)];
                const std::array<std::pair<bool, juce::Point<float>>, 4> targets {{
                    { strike.left,  { -gridSize, -gridSize } }, { strike.right, { gridSize, -gridSize } },
                    { strike.up,    { -gridSize, gridSize } },  { strike.down,  { gridSize, gridSize } }
                }};
                int fired = 0;
                const auto maxDiscs = modulatedCountValue (strike.maxDiscs, ModulationTargetKind::strike, strike.id, 0, 1, 4);
                for (const auto& target : targets)
                {
                    if (! target.first || fired >= maxDiscs) continue;
                    const auto expected = strike.position + target.second;
                    for (int discIndex = 0; discIndex < static_cast<int> (discs().size()); ++discIndex)
                        if (discs()[static_cast<size_t> (discIndex)].centre.getDistanceFrom (expected) <= gridSize * 0.25f)
                        { onDiscFlowTriggered (discIndex); ++fired; break; }
                }
            }
            pulse.lastStrike = hitStrike;

            auto hitTeleport = -1;
            for (int i = 0; i < static_cast<int> (pipeTeleports().size()); ++i)
                if (pipeTeleports()[static_cast<size_t> (i)].enabled && pipeTeleports()[static_cast<size_t> (i)].position.getDistanceFrom (position) <= gridSize * 0.38f)
                { hitTeleport = i; break; }
            if (hitTeleport >= 0 && hitTeleport != pulse.lastTeleport)
            {
                auto teleported = false;
                auto& source = pipeTeleports()[static_cast<size_t> (hitTeleport)];
                const auto windowBeats = source.windowBars * 4.0;
                if (flowBeatPosition - source.windowStartBeat >= windowBeats)
                { source.windowStartBeat = flowBeatPosition; source.windowCount = 0; }
                const auto belowLifetime = source.stopAfter == 0 || source.totalTeleported < source.stopAfter;
                const auto belowRate = source.windowCount < source.maxPerWindow;
                if (flowDebugVisible && (! belowLifetime || ! belowRate))
                    flowDebugEvents.push_back ({ position, belowLifetime ? "Teleport rate limit" : "Teleport lifetime limit", now + 1400.0 });
                if (belowLifetime && belowRate && juce::Random::getSystemRandom().nextDouble()
                    <= modulatedUnitValue (source.probability, ModulationTargetKind::teleport, source.id, 0))
                {
                    std::vector<int> destinations;
                    for (int i = 0; i < static_cast<int> (pipeTeleports().size()); ++i)
                        if (i != hitTeleport && pipeTeleports()[static_cast<size_t> (i)].enabled && (source.destinationId.isEmpty() || pipeTeleports()[static_cast<size_t> (i)].id == source.destinationId))
                            destinations.push_back (i);
                    if (! destinations.empty())
                    {
                        const auto destinationIndex = destinations[static_cast<size_t> (juce::Random::getSystemRandom().nextInt (static_cast<int> (destinations.size())))];
                        const auto destination = pipeTeleports()[static_cast<size_t> (destinationIndex)].position;
                        const auto routeHit = findNearestSegment (destination, gridSize * 0.55f, -1, 1);
                        if (routeHit.route >= 0)
                        {
                            pulse.routeIndex = routeHit.route; pulse.distance = distanceAlongRouteForPoint (routes()[static_cast<size_t> (routeHit.route)], destination);
                            pulse.lastTeleport = destinationIndex; position = destination;
                            teleported = true;
                            ++source.totalTeleported; ++source.windowCount;
                        }
                    }
                }
                if (! teleported) pulse.lastTeleport = hitTeleport;
            }
            else pulse.lastTeleport = hitTeleport;

            auto hitCloner = -1;
            for (int i = 0; i < static_cast<int> (pipeCloners().size()); ++i)
                if (pipeCloners()[static_cast<size_t> (i)].enabled && pipeCloners()[static_cast<size_t> (i)].position.getDistanceFrom (position) <= gridSize * 0.38f)
                {
                    hitCloner = i;
                    break;
                }
            if (hitCloner >= 0 && hitCloner != pulse.lastCloner && flowPulses.size() + spawnedPulses.size() < 2048)
            {
                auto clones = quantumClonesFor (pulse, pipeCloners()[static_cast<size_t> (hitCloner)]);
                const auto room = static_cast<size_t> (2048) - flowPulses.size() - spawnedPulses.size();
                if (clones.size() > room) clones.resize (room);
                spawnedPulses.insert (spawnedPulses.end(), clones.begin(), clones.end());
            }
            pulse.lastCloner = hitCloner;

            auto hitDisc = -1;
            for (int i = 0; i < static_cast<int> (discs().size()); ++i)
                if (discs()[static_cast<size_t> (i)].centre.getDistanceFrom (position) <= gridSize * 0.48f) { hitDisc = i; break; }
            if (hitDisc >= 0 && hitDisc != pulse.lastDisc && onDiscFlowTriggered
                && juce::Random::getSystemRandom().nextDouble() <= pulse.probability)
            {
                onDiscFlowTriggered (hitDisc);
                if (discs()[static_cast<size_t> (hitDisc)].holdDropsUntilFinished && onDiscPlaybackActive
                    && onDiscPlaybackActive (discKeyForCurrentWorld (hitDisc)))
                    pulse.heldByDisc = hitDisc;
            }
            pulse.lastDisc = hitDisc;
        }
        flowPulses.insert (flowPulses.end(), spawnedPulses.begin(), spawnedPulses.end());
        flowPulses.erase (std::remove_if (flowPulses.begin(), flowPulses.end(), [] (const auto& pulse)
        {
            return pulse.routeIndex < 0;
        }), flowPulses.end());
        if (! juce::isPositiveAndBelow (selectedFlowPulse, static_cast<int> (flowPulses.size())))
            selectedFlowPulse = -1;
        flowDebugEvents.erase (std::remove_if (flowDebugEvents.begin(), flowDebugEvents.end(), [now] (const auto& event)
        { return event.expiresMs <= now; }), flowDebugEvents.end());
        repaint();
    }

    void drawFlowDebugEvents (juce::Graphics& g) const
    {
        if (! flowDebugVisible) return;
        for (const auto& event : flowDebugEvents)
        {
            const auto label = juce::Rectangle<float> (126.0f, 19.0f).withPosition (event.position.x + 8.0f, event.position.y + 8.0f);
            g.setColour (juce::Colour (0xff241018).withAlpha (0.92f)); g.fillRoundedRectangle (label, 4.0f);
            g.setColour (juce::Colour (0xffff91aa)); g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
            g.drawText (event.text, label.reduced (5.0f, 1.0f), juce::Justification::centredLeft, false);
        }
    }

    void drawFlowPulses (juce::Graphics& g) const
    {
        if (flowPulses.empty()) return;
        for (int pulseIndex = 0; pulseIndex < static_cast<int> (flowPulses.size()); ++pulseIndex)
        {
            const auto& pulse = flowPulses[static_cast<size_t> (pulseIndex)];
            if (! juce::isPositiveAndBelow (pulse.routeIndex, static_cast<int> (routes().size()))) continue;
            const auto position = pointAlongRoute (routes()[static_cast<size_t> (pulse.routeIndex)], pulse.distance);
            g.setColour (juce::Colour (0xffb9ffe8).withAlpha (0.22f));
            g.fillEllipse (juce::Rectangle<float> (18.0f, 18.0f).withCentre (position));
            g.setColour (juce::Colour (0xff62f2c7));
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (position));
            const auto isSelected = pulseIndex == selectedFlowPulse;
            if (isSelected)
            {
                g.setColour (juce::Colours::white.withAlpha (0.92f));
                g.drawEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (position), 1.5f);
            }
            if (flowDebugVisible)
            {
                const auto& route = routes()[static_cast<size_t> (pulse.routeIndex)];
                const auto ahead = pointAlongRoute (route, juce::jlimit (0.0f, route.getLength(),
                                                                         pulse.distance + (pulse.reverse ? -7.0f : 7.0f)));
                g.setColour (juce::Colour (0xffffffff).withAlpha (0.72f));
                g.drawArrow ({ position, ahead }, 1.2f, 4.0f, 4.0f);
                auto text = juce::String (pulse.speed, 2) + "x";
                if (pulse.waitBeatsRemaining > 0.0) text << "  wait " << juce::String (pulse.waitBeatsRemaining, 2) << " beats";
                const auto label = juce::Rectangle<float> (118.0f, 18.0f).withPosition (position.x + 8.0f, position.y - 22.0f);
                g.setColour (juce::Colour (0xff07100d).withAlpha (0.88f)); g.fillRoundedRectangle (label, 4.0f);
                g.setColour (juce::Colours::white.withAlpha (0.88f)); g.setFont (juce::FontOptions (9.0f));
                g.drawText (text, label.reduced (5.0f, 1.0f), juce::Justification::centredLeft, false);
            }

            if (isSelected)
            {
                juce::String state = "Moving";
                if (pulse.heldByDisc >= 0) state = "Held by disc " + juce::String (pulse.heldByDisc + 1);
                else if (pulse.waitBeatsRemaining > 0.0) state = "Waiting " + juce::String (pulse.waitBeatsRemaining, 2) + " beats";

                juce::String lastDevice = "None";
                if (pulse.lastFilter >= 0) lastDevice = "Filter";
                else if (pulse.lastLogic >= 0) lastDevice = "Logic";
                else if (pulse.lastTeleport >= 0) lastDevice = "Teleport";
                else if (pulse.lastStrike >= 0) lastDevice = "Strike";
                else if (pulse.lastWait >= 0) lastDevice = "Wait";
                else if (pulse.lastSpeedLimit >= 0) lastDevice = "Speed limit";
                else if (pulse.lastCloner >= 0) lastDevice = "Quantum";
                else if (pulse.lastDrain >= 0) lastDevice = "Drain";
                else if (pulse.lastDisc >= 0) lastDevice = "Disc " + juce::String (pulse.lastDisc + 1);

                const auto& route = routes()[static_cast<size_t> (pulse.routeIndex)];
                auto details = "Drop " + juce::String (pulseIndex + 1) + "   " + juce::String (pulse.speed, 2) + "x\n";
                details << (route.isWarpPipe ? "Warp pipe " : "Pipe ") << pulse.routeIndex + 1 << "   " << state << "\nLast: " << lastDevice;
                auto card = juce::Rectangle<float> (190.0f, 56.0f).withPosition (position.x + 14.0f, position.y - 66.0f);
                g.setColour (juce::Colour (0xff111815).withAlpha (0.96f));
                g.fillRoundedRectangle (card, 6.0f);
                g.setColour (juce::Colour (0xff405149));
                g.drawRoundedRectangle (card, 6.0f, 1.0f);
                g.setColour (juce::Colours::white.withAlpha (0.92f));
                g.setFont (juce::FontOptions (10.0f));
                g.drawFittedText (details, card.reduced (8.0f, 5.0f).toNearestInt(), juce::Justification::centredLeft, 3);
            }
        }
    }

    std::vector<FlowPulse> quantumClonesFor (const FlowPulse& source, const PipeCloner& cloner) const
    {
        struct Candidate { FlowPulse pulse; juce::Point<float> direction; };
        std::vector<Candidate> candidates;
        bool isNode = false;

        for (int routeIndex = 0; routeIndex < static_cast<int> (routes().size()); ++routeIndex)
        {
            const auto& route = routes()[static_cast<size_t> (routeIndex)];
            if (! route.isPipe) continue;
            const auto distances = routeNodeDistances (route);
            for (int node = 0; node < static_cast<int> (route.points.size()); ++node)
            {
                if (! pointsTouch (route.points[static_cast<size_t> (node)], cloner.position)) continue;
                isNode = true;
                for (const auto adjacent : { node - 1, node + 1 })
                {
                    if (! juce::isPositiveAndBelow (adjacent, static_cast<int> (route.points.size()))) continue;
                    FlowPulse clone = source;
                    clone.routeIndex = routeIndex;
                    clone.distance = distances[static_cast<size_t> (node)];
                    clone.reverse = adjacent < node;
                    clone.lastCloner = -1;
                    const auto direction = route.points[static_cast<size_t> (adjacent)] - route.points[static_cast<size_t> (node)];
                    candidates.push_back ({ clone, direction });
                }
            }
        }

        if (! isNode && juce::isPositiveAndBelow (source.routeIndex, static_cast<int> (routes().size())))
        {
            const auto& route = routes()[static_cast<size_t> (source.routeIndex)];
            const auto distance = distanceAlongRouteForPoint (route, cloner.position);
            const auto before = pointAlongRoute (route, juce::jmax (0.0f, distance - 1.0f));
            const auto after = pointAlongRoute (route, juce::jmin (route.getLength(), distance + 1.0f));
            for (const auto reverse : { false, true })
            {
                auto clone = source;
                clone.distance = distance;
                clone.reverse = reverse;
                clone.lastCloner = -1;
                candidates.push_back ({ clone, reverse ? before - cloner.position : after - cloner.position });
            }
        }

        if (candidates.empty()) return {};

        const auto count = static_cast<int> (candidates.size());
        auto mode = cloner.fourWayMode;
        if (! isNode) mode = cloner.straightMode;
        else if (count <= 2) mode = cloner.twoWayMode;
        else if (count == 3) mode = cloner.threeWayMode;

        std::vector<FlowPulse> result;
        const auto cloneChance = modulatedUnitValue (cloner.cloneProbability, ModulationTargetKind::quantum, cloner.id, 0);
        const auto addCandidate = [&] (int index)
        {
            if (juce::Random::getSystemRandom().nextDouble() > cloneChance)
                return;
            auto clone = candidates[static_cast<size_t> (index)].pulse;
            clone.lastCloner = static_cast<int> (&cloner - pipeCloners().data());
            result.push_back (clone);
        };
        const auto attempts = modulatedCountValue (cloner.maxClones, ModulationTargetKind::quantum, cloner.id, 1, 1, 16);
        if (mode == PipeCloner::DirectionMode::all)
        {
            for (int i = 0; i < attempts; ++i) addCandidate (i % count);
            return result;
        }
        if (mode == PipeCloner::DirectionMode::random)
        {
            for (int i = 0; i < attempts; ++i)
                addCandidate (juce::Random::getSystemRandom().nextInt (count));
            return result;
        }

        const auto& sourceRoute = routes()[static_cast<size_t> (source.routeIndex)];
        const auto sourcePoint = pointAlongRoute (sourceRoute, source.distance);
        const auto nearby = pointAlongRoute (sourceRoute, juce::jlimit (0.0f, sourceRoute.getLength(),
                                                                       source.distance + (source.reverse ? -1.0f : 1.0f)));
        auto heading = nearby - sourcePoint;
        if (heading.getDistanceFromOrigin() < 0.001f) heading = { 1.0f, 0.0f };
        auto best = 0;
        auto bestScore = mode == PipeCloner::DirectionMode::leftmost ? -std::numeric_limits<float>::max()
                                                                     : std::numeric_limits<float>::max();
        for (int i = 0; i < count; ++i)
        {
            const auto direction = candidates[static_cast<size_t> (i)].direction;
            const auto angle = std::atan2 (heading.x * direction.y - heading.y * direction.x,
                                           heading.x * direction.x + heading.y * direction.y);
            const auto score = mode == PipeCloner::DirectionMode::straightest ? std::abs (angle) : angle;
            const auto better = mode == PipeCloner::DirectionMode::leftmost ? score > bestScore : score < bestScore;
            if (better) { best = i; bestScore = score; }
        }
        for (int i = 0; i < attempts; ++i) addCandidate (best);
        return result;
    }

    void drawDisc (juce::Graphics& g, const Disc& disc, bool selected, bool flashing) const
    {
        const auto diameter = compactDiscs ? gridSize * 0.55f : gridSize;
        const auto scale = diameter / gridSize;
        const auto area = juce::Rectangle<float> (diameter, diameter).withCentre (disc.centre);
        const auto rim = area.expanded (1.4f * scale);
        const auto face = area.reduced (2.3f * scale);

        if (selected || flashing)
        {
            g.setColour ((flashing ? juce::Colour (0xffffe27a) : discRimColour()).withAlpha (flashing ? 0.34f : 0.18f));
            g.fillEllipse (area.expanded ((flashing ? 10.0f : 8.0f) * scale));
            g.setColour ((flashing ? juce::Colour (0xfffff1a8) : accentColour()).withAlpha (flashing ? 0.92f : 0.50f));
            g.drawEllipse (area.expanded ((flashing ? 7.4f : 6.2f) * scale), juce::jmax (0.8f, (flashing ? 2.0f : 1.15f) * scale));
        }

        g.setColour (juce::Colour (0xff010504).withAlpha (0.56f));
        g.fillEllipse (area.translated (1.5f * scale, 2.4f * scale).expanded (2.2f * scale));

        juce::ColourGradient rimFill (discRimColour().withAlpha (0.98f), rim.getTopLeft(),
                                      discCoolColour().darker (0.42f), rim.getBottomRight(), false);
        g.setGradientFill (rimFill);
        g.fillEllipse (rim);

        g.setColour (juce::Colour (0xff020807).withAlpha (0.38f));
        g.drawEllipse (rim, 1.0f);

        juce::ColourGradient fill (discHotColour(), face.getTopLeft() + juce::Point<float> (3.0f, 2.0f),
                                   discCoolColour().withMultipliedSaturation (0.88f), face.getBottomRight(), true);
        fill.addColour (0.42, discColour());
        fill.addColour (0.78, juce::Colour (0xffd85e72));
        g.setGradientFill (fill);
        g.fillEllipse (face);

        g.setColour (juce::Colours::white.withAlpha (0.34f));
        g.fillEllipse (face.withSizeKeepingCentre (face.getWidth() * 0.36f, face.getHeight() * 0.24f)
                            .translated (-3.0f * scale, -3.2f * scale));

        g.setColour (juce::Colour (0xff4a1537).withAlpha (0.18f));
        g.fillEllipse (face.withTrimmedTop (face.getHeight() * 0.50f).expanded (1.0f, 0.0f));

        g.setColour (juce::Colour (0xff080c0a).withAlpha (0.82f));
        g.drawEllipse (face, selected ? juce::jmax (1.0f, 1.7f * scale)
                                      : juce::jmax (0.7f, scale));

        drawDiscOrbit (g, area, disc, selected);
        drawDiscElementSatellites (g, area, disc, selected);
        drawDiscElementBadge (g, area, disc);
    }

    static float discOrbitRadius (juce::Rectangle<float> discArea) noexcept
    {
        return discArea.getWidth() * 0.5f + 27.0f;
    }

    float orbitVisualAlpha() const noexcept
    {
        return orbitElementsDimmed ? 0.10f : 1.0f;
    }

    static juce::Point<float> pointOnDiscOrbit (juce::Rectangle<float> discArea, float angleRadians) noexcept
    {
        return discArea.getCentre() + juce::Point<float> (std::cos (angleRadians), std::sin (angleRadians)) * discOrbitRadius (discArea);
    }

    void drawDiscOrbit (juce::Graphics& g, juce::Rectangle<float> discArea, const Disc& disc, bool selected) const
    {
        const auto elementCount = disc.getElementCount();

        if (elementCount <= 0)
            return;

        const auto centre = discArea.getCentre();
        const auto ringCount = getDiscOrbitRingCount (elementCount);
        const auto orbitColour = selected ? accentColour().brighter (0.12f)
                                          : discRimColour().interpolatedWith (soundElementColour(), 0.32f);
        const auto alpha = orbitVisualAlpha();

        for (int ring = 0; ring < ringCount; ++ring)
        {
            const auto radius = discOrbitRadius (discArea) + static_cast<float> (ring) * discOrbitRingSpacing;
            const auto orbitArea = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

            g.setColour (orbitColour.withAlpha (alpha * (selected ? 0.13f : 0.08f) / static_cast<float> (ring + 1)));
            g.drawEllipse (orbitArea.expanded (2.2f), 0.55f);
            g.setColour (orbitColour.withAlpha (alpha * (selected ? 0.46f : 0.30f) / static_cast<float> (ring + 1)));
            g.drawEllipse (orbitArea, selected ? 0.95f : 0.70f);
        }
    }

    struct DiscOrbitElement
    {
        juce::Colour colour;
    };

    static int getDiscOrbitRingCount (int elementCount) noexcept
    {
        return juce::jmax (1, (elementCount + maxDiscElementsPerOrbitRing - 1) / maxDiscElementsPerOrbitRing);
    }

    static void addOrbitElements (std::vector<DiscOrbitElement>& elements,
                                  int count,
                                  juce::Colour colour)
    {
        for (int i = 0; i < count; ++i)
            elements.push_back ({ colour });
    }

    void drawDiscElementSatellites (juce::Graphics& g,
                                    juce::Rectangle<float> discArea,
                                    const Disc& disc,
                                    bool selected) const
    {
        std::vector<DiscOrbitElement> elements;
        elements.reserve (static_cast<size_t> (disc.getElementCount()));
        addOrbitElements (elements, disc.soundElementCount, soundElementColour());
        addOrbitElements (elements, disc.nestedWorldCount, worldElementColour());
        addOrbitElements (elements, static_cast<int> (disc.scCodeElements.size()), scCodeElementColour());
        addOrbitElements (elements, static_cast<int> (disc.pdPatches.size()), pdPatchElementColour());
        addOrbitElements (elements, static_cast<int> (disc.scSheets.size()), scSheetElementColour());
        addOrbitElements (elements, static_cast<int> (disc.orcaGrids.size()), orcaGridElementColour());
        addOrbitElements (elements, static_cast<int> (disc.carousels.size()), carouselElementColour());
        addOrbitElements (elements, static_cast<int> (disc.pipeWorlds.size()), pipeElementColour());

        if (elements.empty())
            return;

        const auto centre = discArea.getCentre();
        const auto baseRadius = discOrbitRadius (discArea);
        const auto satelliteSize = elements.size() <= 12 ? 7.8f : 6.4f;
        const auto alpha = orbitVisualAlpha();

        for (int i = 0; i < static_cast<int> (elements.size()); ++i)
        {
            const auto ring = i / maxDiscElementsPerOrbitRing;
            const auto indexInRing = i % maxDiscElementsPerOrbitRing;
            const auto ringStart = ring * maxDiscElementsPerOrbitRing;
            const auto ringCount = juce::jmin (maxDiscElementsPerOrbitRing, static_cast<int> (elements.size()) - ringStart);
            const auto step = juce::MathConstants<float>::twoPi / static_cast<float> (ringCount);
            const auto startAngle = ringCount == 1 ? -juce::MathConstants<float>::halfPi
                                                   : -juce::MathConstants<float>::halfPi - step * 0.5f;
            const auto angle = startAngle + static_cast<float> (indexInRing) * step;
            const auto radius = baseRadius + static_cast<float> (ring) * discOrbitRingSpacing;
            const auto satelliteCentre = centre + juce::Point<float> (std::cos (angle), std::sin (angle)) * radius;
            const auto satellite = juce::Rectangle<float> (satelliteSize, satelliteSize).withCentre (satelliteCentre);
            const auto colour = elements[static_cast<size_t> (i)].colour;

            g.setColour (juce::Colour (0xff020706).withAlpha (0.62f * alpha));
            g.fillEllipse (satellite.translated (0.8f, 1.0f).expanded (1.1f));

            g.setColour (colour.withAlpha (alpha * (selected ? 0.30f : 0.20f)));
            g.fillEllipse (satellite.expanded (3.1f));

            juce::ColourGradient fill (colour.brighter (0.20f).withAlpha (0.96f * alpha), satellite.getTopLeft(),
                                       colour.darker (0.28f).withAlpha (0.96f * alpha), satellite.getBottomRight(), true);
            fill.addColour (0.58, colour.withAlpha (0.96f * alpha));
            g.setGradientFill (fill);
            g.fillEllipse (satellite);

            g.setColour (juce::Colours::white.withAlpha (0.34f * alpha));
            g.fillEllipse (satellite.withSizeKeepingCentre (satelliteSize * 0.34f, satelliteSize * 0.26f)
                                .translated (-satelliteSize * 0.16f, -satelliteSize * 0.18f));

            g.setColour (juce::Colour (0xff050807).withAlpha (0.56f * alpha));
            g.drawEllipse (satellite, 0.85f);
        }
    }

    void drawSoundElementBeads (juce::Graphics& g, juce::Rectangle<float> discArea, int soundCount, bool selected) const
    {
        if (soundCount <= 0)
            return;

        const auto centre = discArea.getCentre();
        const auto radius = discOrbitRadius (discArea);
        const auto visibleBeads = juce::jmin (soundCount, 12);
        const auto startAngle = -juce::MathConstants<float>::halfPi;
        const auto step = juce::MathConstants<float>::twoPi / static_cast<float> (visibleBeads);
        const auto beadColour = selected ? accentColour().brighter (0.16f)
                                         : soundElementColour().interpolatedWith (discHotColour(), 0.26f);

        for (int i = 0; i < visibleBeads; ++i)
        {
            const auto angle = startAngle + static_cast<float> (i) * step;
            const auto beadCentre = centre + juce::Point<float> (std::cos (angle), std::sin (angle)) * radius;
            const auto beadArea = juce::Rectangle<float> (4.2f, 4.2f).withCentre (beadCentre);

            g.setColour (juce::Colour (0xff050907).withAlpha (0.46f));
            g.fillEllipse (beadArea.translated (0.7f, 0.8f));
            g.setColour (discRimColour().withAlpha (0.42f));
            g.drawEllipse (beadArea.expanded (1.2f), 0.65f);
            g.setColour (beadColour.withAlpha (0.96f));
            g.fillEllipse (beadArea);
        }

        if (soundCount > visibleBeads)
            drawOverflowBadge (g, discArea, soundCount);
    }

    void drawNestedWorldMark (juce::Graphics& g, juce::Rectangle<float> discArea, bool hasNestedWorld) const
    {
        if (! hasNestedWorld)
            return;

        const auto centre = pointOnDiscOrbit (discArea, -juce::MathConstants<float>::halfPi);
        const auto portal = juce::Rectangle<float> (10.5f, 10.5f).withCentre (centre);

        g.setColour (worldElementColour().withAlpha (0.24f));
        g.fillEllipse (portal);

        g.setColour (worldElementColour().withAlpha (0.88f));
        g.drawEllipse (portal, 0.9f);

        juce::Path orbit;
        orbit.addCentredArc (centre.x, centre.y, portal.getWidth() * 0.44f, portal.getHeight() * 0.24f,
                             0.48f, -2.55f, 0.55f, true);
        g.strokePath (orbit, juce::PathStrokeType (0.85f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (juce::Colour (0xff111815).withAlpha (0.62f));
        g.fillEllipse (juce::Rectangle<float> (2.2f, 2.2f).withCentre (centre));
    }

    void drawOverflowBadge (juce::Graphics& g, juce::Rectangle<float> discArea, int soundCount) const
    {
        const auto badge = juce::Rectangle<float> (14.0f, 10.0f).withCentre (pointOnDiscOrbit (discArea, -0.28f));

        g.setColour (juce::Colour (0xff111815).withAlpha (0.76f));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (soundElementColour());
        g.setFont (juce::FontOptions (7.5f, juce::Font::bold));
        g.drawText (juce::String (soundCount), badge, juce::Justification::centred, false);
    }

    void drawScCodeMark (juce::Graphics& g, juce::Rectangle<float> discArea, bool hasScCode) const
    {
        if (! hasScCode)
            return;

        const auto chip = juce::Rectangle<float> (13.0f, 9.0f).withCentre (pointOnDiscOrbit (discArea, juce::MathConstants<float>::pi * 0.25f));

        g.setColour (juce::Colour (0xff08120b).withAlpha (0.72f));
        g.fillRoundedRectangle (chip, 2.5f);
        g.setColour (scCodeElementColour().withAlpha (0.95f));
        g.drawRoundedRectangle (chip, 2.5f, 0.9f);
        g.setFont (juce::FontOptions (7.0f, juce::Font::bold));
        g.drawText ("SC", chip, juce::Justification::centred, false);
    }

    void drawScSheetMark (juce::Graphics& g, juce::Rectangle<float> discArea, bool hasScSheet) const
    {
        if (! hasScSheet)
            return;

        const auto chip = juce::Rectangle<float> (13.0f, 10.0f).withCentre (pointOnDiscOrbit (discArea, juce::MathConstants<float>::pi * 0.72f));

        g.setColour (juce::Colour (0xff061512).withAlpha (0.76f));
        g.fillRoundedRectangle (chip, 2.5f);
        g.setColour (scSheetElementColour().withAlpha (0.95f));
        g.drawRoundedRectangle (chip, 2.5f, 0.9f);

        const auto grid = chip.reduced (3.1f, 2.5f);
        g.drawVerticalLine (static_cast<int> (grid.getCentreX()), grid.getY(), grid.getBottom());
        g.drawHorizontalLine (static_cast<int> (grid.getCentreY()), grid.getX(), grid.getRight());
    }

    void drawOrcaGridMark (juce::Graphics& g, juce::Rectangle<float> discArea, bool hasOrcaGrid) const
    {
        if (! hasOrcaGrid)
            return;

        const auto chip = juce::Rectangle<float> (14.0f, 10.0f).withCentre (pointOnDiscOrbit (discArea, -juce::MathConstants<float>::pi * 0.78f));

        g.setColour (juce::Colour (0xff070b18).withAlpha (0.76f));
        g.fillRoundedRectangle (chip, 2.5f);
        g.setColour (orcaGridElementColour().withAlpha (0.96f));
        g.drawRoundedRectangle (chip, 2.5f, 0.9f);
        g.setFont (juce::FontOptions (7.0f, juce::Font::bold));
        g.drawText ("OR", chip, juce::Justification::centred, false);
    }

    void drawDiscElementBadge (juce::Graphics& g, juce::Rectangle<float> discArea, const Disc& disc) const
    {
        if (disc.getElementCount() <= 0)
            return;

        const auto badge = discArea.reduced (discArea.getWidth() * 0.29f);
        g.setColour (juce::Colour (0xff090d0b).withAlpha (0.44f));
        g.fillEllipse (badge);

        g.setColour (discHotColour().withAlpha (0.88f));
        g.drawEllipse (badge, 0.75f);
        g.setColour (juce::Colour (0xff22120a));
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.drawText (juce::String (disc.getElementCount()), badge, juce::Justification::centred, false);
    }

    void drawNodes (juce::Graphics& g) const
    {
        for (int routeIndex = 0; routeIndex < static_cast<int> (routes().size()); ++routeIndex)
        {
            const auto& route = routes()[static_cast<size_t> (routeIndex)];
            const auto selected = routeIndex == selectedRoute;

            for (int nodeIndex = 0; nodeIndex < static_cast<int> (route.points.size()); ++nodeIndex)
            {
                const auto point = route.points[static_cast<size_t> (nodeIndex)];
                const auto radius = selected && nodeIndex == selectedNode ? nodeRadius + 2.0f : nodeRadius;
                const auto area = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (point);

                g.setColour (selected ? juce::Colour (0xfff0c05a) : juce::Colour (0xffdce7df));
                g.fillEllipse (area);
                g.setColour (juce::Colour (0xff171b18));
                g.drawEllipse (area, 1.4f);
            }
        }
    }

    void drawBackground (juce::Graphics& g) const
    {
        const auto bounds = getLocalBounds().toFloat();
        juce::ColourGradient gradient (backgroundTop(), bounds.getTopLeft(),
                                       backgroundBottom(), bounds.getBottomRight(), false);
        g.setGradientFill (gradient);
        g.fillRect (bounds);

        const auto width = getWidth();
        const auto height = getHeight();

        drawGrid (g, gridSize, gridMinorColour(), static_cast<float> (width), static_cast<float> (height));
        drawGrid (g, gridSize * 4.0f, gridMajorColour(), static_cast<float> (width), static_cast<float> (height));
    }

    void drawGrid (juce::Graphics& g, float spacing, juce::Colour colour, float width, float height) const
    {
        const auto scaledSpacing = spacing * viewScale;

        if (scaledSpacing < 4.0f)
            return;

        auto firstX = std::fmod (viewOffset.x, scaledSpacing);
        auto firstY = std::fmod (viewOffset.y, scaledSpacing);

        if (firstX > 0.0f)
            firstX -= scaledSpacing;

        if (firstY > 0.0f)
            firstY -= scaledSpacing;

        g.setColour (colour);

        for (float x = firstX; x <= width; x += scaledSpacing)
            g.drawVerticalLine (static_cast<int> (std::round (x)), 0.0f, height);

        for (float y = firstY; y <= height; y += scaledSpacing)
            g.drawHorizontalLine (static_cast<int> (std::round (y)), 0.0f, width);
    }

    std::pair<int, int> findNearestNode (juce::Point<float> point, float tolerance) const
    {
        int bestRoute = -1;
        int bestNode = -1;
        auto bestDistance = tolerance;

        for (int routeIndex = 0; routeIndex < static_cast<int> (routes().size()); ++routeIndex)
        {
            const auto& route = routes()[static_cast<size_t> (routeIndex)];

            for (int nodeIndex = 0; nodeIndex < static_cast<int> (route.points.size()); ++nodeIndex)
            {
                const auto distance = point.getDistanceFrom (route.points[static_cast<size_t> (nodeIndex)]);

                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestRoute = routeIndex;
                    bestNode = nodeIndex;
                }
            }
        }

        return { bestRoute, bestNode };
    }

    int findNearestRoute (juce::Point<float> point, float tolerance) const
    {
        int bestRoute = -1;
        auto bestDistance = tolerance;

        for (int routeIndex = 0; routeIndex < static_cast<int> (routes().size()); ++routeIndex)
        {
            const auto& route = routes()[static_cast<size_t> (routeIndex)];

            for (size_t pointIndex = 1; pointIndex < route.points.size(); ++pointIndex)
            {
                const auto distance = distanceFromLineSegment (point,
                                                               route.points[pointIndex - 1],
                                                               route.points[pointIndex]);

                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestRoute = routeIndex;
                }
            }
        }

        return bestRoute;
    }

    int findDiscAt (juce::Point<float> point, float tolerance = -1.0f) const
    {
        if (tolerance < 0.0f)
        {
            const auto diameter = compactDiscs ? gridSize * 0.55f : gridSize;
            tolerance = diameter * 0.55f;
        }

        for (int i = static_cast<int> (discs().size()) - 1; i >= 0; --i)
        {
            if (point.getDistanceFrom (discs()[static_cast<size_t> (i)].centre) <= tolerance)
                return i;
        }

        return -1;
    }

    SegmentHit findNearestSegment (juce::Point<float> point, float tolerance, int ignoredRoute = -1, int routeKind = -1) const
    {
        SegmentHit best;
        auto bestDistance = tolerance;

        for (int routeIndex = 0; routeIndex < static_cast<int> (routes().size()); ++routeIndex)
        {
            if (routeIndex == ignoredRoute)
                continue;

            const auto& route = routes()[static_cast<size_t> (routeIndex)];
            if (routeKind >= 0 && route.isPipe != (routeKind == 1))
                continue;

            for (size_t pointIndex = 1; pointIndex < route.points.size(); ++pointIndex)
            {
                const auto closestPoint = closestPointOnLineSegment (point,
                                                                     route.points[pointIndex - 1],
                                                                     route.points[pointIndex]);
                const auto distance = point.getDistanceFrom (closestPoint);

                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best.route = routeIndex;
                    best.insertIndex = static_cast<int> (pointIndex);
                    best.point = closestPoint;
                }
            }
        }

        return best;
    }

    void notifyChanged (bool resetActiveFlow = true)
    {
        modulationConnections.erase (std::remove_if (modulationConnections.begin(), modulationConnections.end(), [this] (const auto& connection)
        {
            const auto sourceExists = std::any_of (modulators.begin(), modulators.end(), [&] (const auto& source)
            { return source.id == connection.sourceId; });
            return ! sourceExists || ! modulationTargetPosition (connection.targetKind, connection.targetId).has_value();
        }), modulationConnections.end());
        if (! restoringHistory)
        {
            const auto current = createProjectState();
            if (lastCommittedState.isValid()
                && current.toXmlString() != lastCommittedState.toXmlString())
            {
                undoStates.push_back (lastCommittedState);
                if (undoStates.size() > 100) undoStates.erase (undoStates.begin());
            }
            lastCommittedState = current;
        }
        if (flowRunning && resetActiveFlow) resetFlowPulses();
        if (onRoutesChanged != nullptr)
            onRoutesChanged();
    }

    void requestDiscPanel()
    {
        if (onDiscPanelRequested != nullptr)
            onDiscPanelRequested();

        if (onRoutesChanged != nullptr)
            onRoutesChanged();
    }
};

class ScCodeEditorPanel final : public juce::Component,
                                private juce::CodeDocument::Listener
{
public:
    using ChangeCallback = std::function<void()>;
    using TestCallback = std::function<juce::String()>;

    ScCodeEditorPanel (RoadCanvas& canvasToUse,
                       RoadCanvas::DiscHandle handleToUse,
                       int codeIndexToUse,
                       ChangeCallback changeCallback,
                       TestCallback testCallback)
        : canvas (canvasToUse),
          handle (std::move (handleToUse)),
          codeIndex (juce::jmax (0, codeIndexToUse)),
          onChange (std::move (changeCallback)),
          onTest (std::move (testCallback)),
          codeEditor (codeDocument, &tokeniser)
    {
        setOpaque (true);
        addAndMakeVisible (titleLabel);
        addAndMakeVisible (statusLabel);
        addAndMakeVisible (durationLabel);
        addAndMakeVisible (durationBox);
        addAndMakeVisible (feedbackLabel);
        addAndMakeVisible (commitButton);
        addAndMakeVisible (resetButton);
        addAndMakeVisible (clearButton);
        addAndMakeVisible (testButton);
        addAndMakeVisible (wrapToggle);
        addAndMakeVisible (fontSmallerButton);
        addAndMakeVisible (fontLargerButton);
        addAndMakeVisible (sineSnippetButton);
        addAndMakeVisible (sequenceSnippetButton);
        addAndMakeVisible (envSnippetButton);
        addAndMakeVisible (codeEditor);

        styleEditorLabel (titleLabel, 17.0f, true);
        styleEditorLabel (statusLabel, 12.0f, false);
        styleEditorLabel (durationLabel, 13.0f, false);
        styleEditorLabel (feedbackLabel, 12.0f, false);
        styleScDurationBox (durationBox);
        configureCodeEditor();
        styleEditorButton (commitButton, "commit");
        styleEditorButton (resetButton, "reset");
        styleEditorButton (clearButton, "clear");
        styleEditorButton (testButton, "test");
        styleEditorButton (fontSmallerButton, "-");
        styleEditorButton (fontLargerButton, "+");
        styleEditorButton (sineSnippetButton, "sine");
        styleEditorButton (sequenceSnippetButton, "seq");
        styleEditorButton (envSnippetButton, "env");

        wrapToggle.setButtonText ("Tabs");
        wrapToggle.setToggleState (true, juce::dontSendNotification);
        wrapToggle.setColour (juce::ToggleButton::textColourId, textMuted());

        durationBox.onReturnKey = [this] { commitDurationText(); };
        durationBox.onFocusLost = [this] { commitDurationText(); };
        durationBox.onTextChange = [this]
        {
            if (! suppressCallbacks)
                durationLabel.setText (durationLabelText (durationFromText (durationBox.getText())),
                                       juce::dontSendNotification);
        };

        commitButton.onClick = [this] { commitEditor(); };
        resetButton.onClick = [this] { replaceCode (defaultScCode()); };
        clearButton.onClick = [this] { replaceCode ({}); };
        testButton.onClick = [this] { testCode(); };
        wrapToggle.onClick = [this]
        {
            codeEditor.setTabSize (4, ! wrapToggle.getToggleState());
            codeEditor.repaint();
        };
        fontSmallerButton.onClick = [this] { setEditorFontSize (editorFontSize - 1.0f); };
        fontLargerButton.onClick = [this] { setEditorFontSize (editorFontSize + 1.0f); };
        sineSnippetButton.onClick = [this] { insertSnippet ("SinOsc.ar(pitch.midicps) * EnvGen.kr(Env.perc(0.01, 0.4), doneAction: 2) * amp"); };
        sequenceSnippetButton.onClick = [this] { insertSnippet ("Demand.kr(Impulse.kr(tempo / 30), 0, Dseq([0, 3, 7, 10], inf))"); };
        envSnippetButton.onClick = [this] { insertSnippet ("EnvGen.kr(Env.linen(0.01, sustain.max(0.25), 0.25), doneAction: 2)"); };

        refreshFromDisc();
        codeDocument.addListener (this);
    }

    ~ScCodeEditorPanel() override
    {
        codeDocument.removeListener (this);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff101614));
        g.setColour (juce::Colour (0xff26372f));
        g.drawHorizontalLine (156, 0.0f, static_cast<float> (getWidth()));
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (16, 14);
        auto titleRow = bounds.removeFromTop (26);
        titleLabel.setBounds (titleRow.removeFromLeft (180));
        statusLabel.setBounds (titleRow);
        bounds.removeFromTop (8);

        auto durationRow = bounds.removeFromTop (30);
        durationLabel.setBounds (durationRow.removeFromLeft (220));
        durationRow.removeFromLeft (10);
        durationBox.setBounds (durationRow.removeFromLeft (96));
        durationRow.removeFromLeft (10);
        commitButton.setBounds (durationRow.removeFromLeft (82));
        durationRow.removeFromLeft (8);
        resetButton.setBounds (durationRow.removeFromLeft (70));
        durationRow.removeFromLeft (8);
        clearButton.setBounds (durationRow.removeFromLeft (64));
        durationRow.removeFromLeft (8);
        testButton.setBounds (durationRow.removeFromLeft (64));
        bounds.removeFromTop (8);

        auto toolsRow = bounds.removeFromTop (30);
        wrapToggle.setBounds (toolsRow.removeFromLeft (82));
        toolsRow.removeFromLeft (10);
        fontSmallerButton.setBounds (toolsRow.removeFromLeft (34));
        toolsRow.removeFromLeft (6);
        fontLargerButton.setBounds (toolsRow.removeFromLeft (34));
        toolsRow.removeFromLeft (16);
        sineSnippetButton.setBounds (toolsRow.removeFromLeft (64));
        toolsRow.removeFromLeft (8);
        sequenceSnippetButton.setBounds (toolsRow.removeFromLeft (64));
        toolsRow.removeFromLeft (8);
        envSnippetButton.setBounds (toolsRow.removeFromLeft (64));
        bounds.removeFromTop (8);

        feedbackLabel.setBounds (bounds.removeFromTop (22));
        bounds.removeFromTop (12);

        codeEditor.setBounds (bounds);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress ('s', juce::ModifierKeys::commandModifier, 0))
        {
            commitEditor();
            return true;
        }

        if (key == juce::KeyPress ('=', juce::ModifierKeys::commandModifier, 0)
            || key == juce::KeyPress ('+', juce::ModifierKeys::commandModifier, 0))
        {
            setEditorFontSize (editorFontSize + 1.0f);
            return true;
        }

        if (key == juce::KeyPress ('-', juce::ModifierKeys::commandModifier, 0))
        {
            setEditorFontSize (editorFontSize - 1.0f);
            return true;
        }

        return Component::keyPressed (key);
    }

    void refreshFromDisc()
    {
        const auto info = canvas.getDiscScCodeInfo (handle, codeIndex);
        const auto enabled = info.valid;

        titleLabel.setText (enabled ? "SC Code " + juce::String (codeIndex + 1) : "SC Code Removed",
                            juce::dontSendNotification);
        durationLabel.setText (enabled ? durationLabelText (info.durationSeconds)
                                       : "This code block has been removed",
                               juce::dontSendNotification);

        {
            const juce::ScopedValueSetter<bool> suppress (suppressCallbacks, true);
            durationBox.setText (enabled ? durationText (info.durationSeconds) : "-", false);
            codeEditor.loadContent (enabled ? info.code : juce::String());
        }

        durationBox.setEnabled (enabled);
        codeEditor.setReadOnly (! enabled);
        commitButton.setEnabled (enabled);
        resetButton.setEnabled (enabled);
        clearButton.setEnabled (enabled);
        testButton.setEnabled (enabled);
        wrapToggle.setEnabled (enabled);
        fontSmallerButton.setEnabled (enabled);
        fontLargerButton.setEnabled (enabled);
        sineSnippetButton.setEnabled (enabled);
        sequenceSnippetButton.setEnabled (enabled);
        envSnippetButton.setEnabled (enabled);
        updateEditorStatus();
        updateBracketFeedback();
    }

private:
    RoadCanvas& canvas;
    RoadCanvas::DiscHandle handle;
    int codeIndex = 0;
    ChangeCallback onChange;
    TestCallback onTest;
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label durationLabel;
    juce::Label feedbackLabel;
    juce::TextEditor durationBox;
    juce::TextButton commitButton;
    juce::TextButton resetButton;
    juce::TextButton clearButton;
    juce::TextButton testButton;
    juce::ToggleButton wrapToggle;
    juce::TextButton fontSmallerButton;
    juce::TextButton fontLargerButton;
    juce::TextButton sineSnippetButton;
    juce::TextButton sequenceSnippetButton;
    juce::TextButton envSnippetButton;
    SuperColliderTokeniser tokeniser;
    juce::CodeDocument codeDocument;
    juce::CodeEditorComponent codeEditor;
    float editorFontSize = 12.0f;
    bool suppressCallbacks = false;

    void configureCodeEditor()
    {
        codeEditor.setLineNumbersShown (true);
        codeEditor.setScrollbarThickness (12);
        codeEditor.setTabSize (4, false);
        codeEditor.setFont (juce::Font (juce::FontOptions (editorFontSize)));
        codeEditor.setColour (juce::CodeEditorComponent::backgroundColourId, appBackground());
        codeEditor.setColour (juce::CodeEditorComponent::highlightColourId, scCodeElementColour().withAlpha (0.22f));
        codeEditor.setColour (juce::CodeEditorComponent::defaultTextColourId, textPrimary());
        codeEditor.setColour (juce::CodeEditorComponent::lineNumberBackgroundId, juce::Colour (0xff0b100e));
        codeEditor.setColour (juce::CodeEditorComponent::lineNumberTextId, textMuted().withAlpha (0.72f));
        codeEditor.setColourScheme (tokeniser.getDefaultColourScheme());
    }

    void commitEditor()
    {
        commitDurationText();

        const auto check = checkBrackets();
        if (! check.ok)
        {
            setFeedback (check.message, juce::Colour (0xffff8d7b));
            return;
        }

        if (canvas.setDiscScCode (handle, codeIndex, codeDocument.getAllContent()))
        {
            statusLabel.setText ("committed  /  " + getCodeStatsText(), juce::dontSendNotification);
            setFeedback ("Ready", juce::Colour (0xff9ff6a3));
            notifyChanged();
        }
    }

    void replaceCode (const juce::String& code)
    {
        {
            const juce::ScopedValueSetter<bool> suppress (suppressCallbacks, true);
            codeEditor.loadContent (code);
        }

        commitEditor();
    }

    void insertSnippet (const juce::String& snippet)
    {
        codeEditor.insertTextAtCaret (snippet);
        codeEditor.grabKeyboardFocus();
        commitEditor();
    }

    void testCode()
    {
        const auto check = checkBrackets();
        if (! check.ok)
        {
            setFeedback (check.message, juce::Colour (0xffff8d7b));
            return;
        }

        commitEditor();

        if (onTest != nullptr)
            setFeedback (onTest(), juce::Colour (0xff7fd7ff));
    }

    void setEditorFontSize (float newSize)
    {
        editorFontSize = juce::jlimit (10.0f, 22.0f, newSize);
        codeEditor.setFont (juce::Font (juce::FontOptions (editorFontSize)));
        updateEditorStatus();
    }

    juce::String getCodeStatsText() const
    {
        auto text = codeDocument.getAllContent().replace ("\r\n", "\n").replaceCharacter ('\r', '\n');
        int lines = text.isEmpty() ? 0 : 1;

        for (int i = 0; i < text.length(); ++i)
            if (text[i] == '\n')
                ++lines;

        const auto caret = codeEditor.getCaretPos();
        return juce::String (lines) + " lines  /  " + juce::String (text.length()) + " chars  /  "
             + "L" + juce::String (caret.getLineNumber() + 1) + ":C" + juce::String (caret.getIndexInLine() + 1) + "  /  "
             + juce::String (editorFontSize, 0) + " pt";
    }

    void updateEditorStatus()
    {
        statusLabel.setText (getCodeStatsText(), juce::dontSendNotification);
    }

    struct BracketCheck
    {
        bool ok = true;
        juce::String message = "Brackets balanced";
    };

    BracketCheck checkBrackets() const
    {
        const auto text = codeDocument.getAllContent();
        std::vector<juce::juce_wchar> expectedClosers;
        bool inLineComment = false;
        bool inBlockComment = false;
        juce::juce_wchar stringCloser = 0;

        for (int i = 0; i < text.length(); ++i)
        {
            const auto c = text[i];
            const auto next = i + 1 < text.length() ? text[i + 1] : 0;

            if (inLineComment)
            {
                if (c == '\n')
                    inLineComment = false;

                continue;
            }

            if (inBlockComment)
            {
                if (c == '*' && next == '/')
                {
                    inBlockComment = false;
                    ++i;
                }

                continue;
            }

            if (stringCloser != 0)
            {
                if (c == '\\')
                {
                    ++i;
                    continue;
                }

                if (c == stringCloser)
                    stringCloser = 0;

                continue;
            }

            if (c == '/' && next == '/')
            {
                inLineComment = true;
                ++i;
                continue;
            }

            if (c == '/' && next == '*')
            {
                inBlockComment = true;
                ++i;
                continue;
            }

            if (c == '"' || c == '\'')
            {
                stringCloser = c;
                continue;
            }

            if (c == '(') { expectedClosers.push_back (')'); continue; }
            if (c == '[') { expectedClosers.push_back (']'); continue; }
            if (c == '{') { expectedClosers.push_back ('}'); continue; }

            if (c == ')' || c == ']' || c == '}')
            {
                if (expectedClosers.empty())
                    return { false, "Extra closing " + juce::String::charToString (c) };

                const auto expected = expectedClosers.back();
                expectedClosers.pop_back();

                if (c != expected)
                    return { false, "Expected " + juce::String::charToString (expected)
                                  + " before " + juce::String::charToString (c) };
            }
        }

        if (stringCloser != 0)
            return { false, "Unclosed string" };

        if (inBlockComment)
            return { false, "Unclosed block comment" };

        if (! expectedClosers.empty())
            return { false, "Missing " + juce::String::charToString (expectedClosers.back()) };

        return {};
    }

    void updateBracketFeedback()
    {
        const auto check = checkBrackets();
        setFeedback (check.message, check.ok ? juce::Colour (0xff9ff6a3)
                                             : juce::Colour (0xffff8d7b));
    }

    void setFeedback (const juce::String& text, juce::Colour colour)
    {
        feedbackLabel.setColour (juce::Label::textColourId, colour);
        feedbackLabel.setText (text, juce::dontSendNotification);
    }

    void codeDocumentTextInserted (const juce::String&, int) override
    {
        documentChanged();
    }

    void codeDocumentTextDeleted (int, int) override
    {
        documentChanged();
    }

    void documentChanged()
    {
        if (suppressCallbacks)
            return;

        if (canvas.setDiscScCode (handle, codeIndex, codeDocument.getAllContent()))
            notifyChanged();

        updateEditorStatus();
        updateBracketFeedback();
    }

    void commitDurationText()
    {
        if (suppressCallbacks)
            return;

        const auto duration = durationFromText (durationBox.getText());

        if (canvas.setDiscScDuration (handle, codeIndex, duration))
        {
            durationBox.setText (durationText (duration), false);
            durationLabel.setText (durationLabelText (duration), juce::dontSendNotification);
            notifyChanged();
        }
    }

    void notifyChanged()
    {
        if (onChange != nullptr)
            onChange();
    }
};

class ScSheetEditorPanel final : public juce::Component
{
public:
    using ChangeCallback = std::function<void()>;

    ScSheetEditorPanel (RoadCanvas& canvasToUse,
                        RoadCanvas::DiscHandle handleToUse,
                        ChangeCallback changeCallback)
        : canvas (canvasToUse),
          handle (std::move (handleToUse)),
          onChange (std::move (changeCallback))
    {
        setOpaque (true);
        addAndMakeVisible (titleLabel);
        addAndMakeVisible (infoLabel);
        addAndMakeVisible (addRowButton);
        addAndMakeVisible (removeRowButton);
        addAndMakeVisible (duplicateRowButton);
        addAndMakeVisible (table);

        styleEditorLabel (titleLabel, 17.0f, true);
        styleEditorLabel (infoLabel, 13.0f, false);
        styleEditorButton (addRowButton, "+ row");
        styleEditorButton (removeRowButton, "- row");
        styleEditorButton (duplicateRowButton, "copy row");

        addRowButton.onClick = [this]
        {
            if (canvas.addScSheetRowToDisc (handle))
            {
                refreshFromDisc();
                notifyChanged();
            }
        };

        removeRowButton.onClick = [this]
        {
            if (canvas.removeScSheetRowFromDisc (handle, table.getSelectedRow()))
            {
                refreshFromDisc();
                notifyChanged();
            }
            else
            {
                infoLabel.setText ("Select a row first", juce::dontSendNotification);
            }
        };

        duplicateRowButton.onClick = [this]
        {
            if (canvas.duplicateScSheetRowFromDisc (handle, table.getSelectedRow()))
            {
                refreshFromDisc();
                notifyChanged();
            }
            else
            {
                infoLabel.setText ("Select a row first", juce::dontSendNotification);
            }
        };

        table.setOnChange ([this]
        {
            canvas.notifyDiscScSheetChanged (handle);
            refreshFromDisc();
            notifyChanged();
        });

        refreshFromDisc();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff101614));
        g.setColour (juce::Colour (0xff26372f));
        g.drawHorizontalLine (88, 0.0f, static_cast<float> (getWidth()));
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (16, 14);
        titleLabel.setBounds (bounds.removeFromTop (26));
        bounds.removeFromTop (8);

        auto controls = bounds.removeFromTop (34);
        addRowButton.setBounds (controls.removeFromLeft (92));
        controls.removeFromLeft (8);
        removeRowButton.setBounds (controls.removeFromLeft (92));
        controls.removeFromLeft (8);
        duplicateRowButton.setBounds (controls.removeFromLeft (104));
        bounds.removeFromTop (8);

        infoLabel.setBounds (bounds.removeFromTop (22));
        bounds.removeFromTop (8);
        table.setBounds (bounds);
    }

    void refreshFromDisc()
    {
        const auto info = canvas.getDiscInfo (handle);
        const auto enabled = info.valid && info.hasScSheet;

        titleLabel.setText (enabled ? "SCsheet" : "SCsheet Removed", juce::dontSendNotification);
        infoLabel.setText (enabled
                               ? juce::String (info.scSheetRowCount) + " rows  /  " + juce::String (info.scSheetSectionCount) + " sections"
                               : "No SCsheet on this disc",
                           juce::dontSendNotification);
        table.setDocument (enabled ? canvas.getDiscScSheetDocument (handle) : nullptr);

        addRowButton.setEnabled (enabled);
        removeRowButton.setEnabled (enabled);
        duplicateRowButton.setEnabled (enabled);
        table.setEnabled (enabled);
    }

private:
    RoadCanvas& canvas;
    RoadCanvas::DiscHandle handle;
    ChangeCallback onChange;
    juce::Label titleLabel;
    juce::Label infoLabel;
    juce::TextButton addRowButton;
    juce::TextButton removeRowButton;
    juce::TextButton duplicateRowButton;
    ScoreTableComponent table;

    void notifyChanged()
    {
        if (onChange != nullptr)
            onChange();
    }
};

class PdPatchEditorPanel final : public juce::Component,
                                 private juce::Timer
{
public:
    using ChangeCallback = std::function<void()>;
    using TestCallback = std::function<juce::String()>;
    using GuiTriggerCallback = std::function<juce::String (const juce::String&, const juce::String&, const juce::StringArray&, float, bool, const juce::String&)>;

    PdPatchEditorPanel (RoadCanvas& canvasToUse,
                        RoadCanvas::DiscHandle handleToUse,
                        int patchIndexToUse,
                        ChangeCallback changeCallback,
                        TestCallback testCallback,
                        GuiTriggerCallback guiTriggerCallback,
                        ScDiscAudioEngine& audioEngineToUse)
        : canvas (canvasToUse),
          audioEngine (audioEngineToUse),
          handle (std::move (handleToUse)),
          patchIndex (juce::jmax (0, patchIndexToUse)),
          onChange (std::move (changeCallback)),
          onTest (std::move (testCallback)),
          onGuiTrigger (std::move (guiTriggerCallback)),
          initialInfo (canvasToUse.getDiscPdPatchInfo (handle, patchIndex)),
          browser (createBrowserOptions (initialInfo.patch,
                                         {},
                                         {},
                                         {},
                                         {},
                                         this))
    {
        setOpaque (true);
        addAndMakeVisible (titleLabel);
        addAndMakeVisible (statusLabel);
        addAndMakeVisible (durationLabel);
        addAndMakeVisible (durationBox);
        addAndMakeVisible (importButton);
        addAndMakeVisible (exportButton);
        addAndMakeVisible (resetButton);
        addAndMakeVisible (testButton);
        addAndMakeVisible (browser);

        styleEditorLabel (titleLabel, 17.0f, true);
        styleEditorLabel (statusLabel, 12.0f, false);
        styleEditorLabel (durationLabel, 13.0f, false);
        styleScDurationBox (durationBox);
        styleEditorButton (importButton, "import");
        styleEditorButton (exportButton, "export");
        styleEditorButton (resetButton, "reset");
        styleEditorButton (testButton, "test");

        durationBox.onReturnKey = [this] { commitDurationText(); };
        durationBox.onFocusLost = [this] { commitDurationText(); };
        durationBox.onTextChange = [this]
        {
            if (! suppressCallbacks)
                durationLabel.setText (durationLabelText (durationFromText (durationBox.getText())),
                                       juce::dontSendNotification);
        };

        resetButton.onClick = [this]
        {
            if (canvas.setDiscPdPatch (handle, patchIndex, defaultPdPatch(), {}))
            {
                reloadBrowser (defaultPdPatch());
                loadProjectMetadataAsync();
                notifyChanged();
            }
        };
        importButton.onClick = [this] { importPatch(); };
        exportButton.onClick = [this] { exportPatch(); };
        testButton.onClick = [this]
        {
            commitDurationText();

            if (onTest != nullptr)
                statusLabel.setText (onTest(), juce::dontSendNotification);

            refreshMessageSubscriptions();
        };

        browser.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
        const auto safeThis = juce::Component::SafePointer<PdPatchEditorPanel> (this);
        audioEngine.setPdMessageOutputCallback ([safeThis] (const juce::String& receiver,
                                                            const juce::String& selector,
                                                            const juce::StringArray& atoms)
        {
            juce::MessageManager::callAsync ([safeThis, receiver, selector, atoms]
            {
                if (safeThis != nullptr)
                    safeThis->forwardPdMessageToBrowser (receiver, selector, atoms);
            });
        });
        refreshFromDisc();
        loadProjectMetadataAsync();
        startTimerHz (8);
    }

    ~PdPatchEditorPanel() override
    {
        stopTimer();
        audioEngine.setPdMessageSubscriptions ({});
        audioEngine.setPdMessageOutputCallback ({});
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff101614));
        g.setColour (juce::Colour (0xff26372f));
        g.drawHorizontalLine (96, 0.0f, static_cast<float> (getWidth()));
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (16, 14);
        auto titleRow = bounds.removeFromTop (26);
        titleLabel.setBounds (titleRow.removeFromLeft (180));
        statusLabel.setBounds (titleRow);
        bounds.removeFromTop (8);

        auto controls = bounds.removeFromTop (34);
        durationLabel.setBounds (controls.removeFromLeft (190));
        controls.removeFromLeft (10);
        durationBox.setBounds (controls.removeFromLeft (96));
        controls.removeFromLeft (10);
        importButton.setBounds (controls.removeFromLeft (76));
        controls.removeFromLeft (8);
        exportButton.setBounds (controls.removeFromLeft (76));
        controls.removeFromLeft (8);
        resetButton.setBounds (controls.removeFromLeft (76));
        controls.removeFromLeft (8);
        testButton.setBounds (controls.removeFromLeft (64));
        bounds.removeFromTop (14);

        browser.setBounds (bounds);
    }

private:
    RoadCanvas& canvas;
    ScDiscAudioEngine& audioEngine;
    RoadCanvas::DiscHandle handle;
    int patchIndex = 0;
    ChangeCallback onChange;
    TestCallback onTest;
    GuiTriggerCallback onGuiTrigger;
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label durationLabel;
    juce::TextEditor durationBox;
    juce::TextButton importButton;
    juce::TextButton exportButton;
    juce::TextButton resetButton;
    juce::TextButton testButton;
    RoadCanvas::PdPatchInfo initialInfo;
    juce::WebBrowserComponent browser;
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool suppressCallbacks = false;
    int metadataLoadGeneration = 0;
    juce::StringArray monitorReceiverTemplates;
    std::map<juce::String, juce::String> monitorReceiverAliases;
    juce::StringArray monitorArrayTemplates;
    std::map<juce::String, juce::String> monitorArrayAliases;
    std::uint64_t lastArraySnapshotHash = 0;
    bool hasArraySnapshotHash = false;

    static void addAbstractionNamesFromDirectory (juce::StringArray& names, const juce::File& directory)
    {
        if (! directory.isDirectory())
            return;

        for (const auto& file : directory.findChildFiles (juce::File::findFiles, false, "*.pd"))
            names.addIfNotAlreadyThere (file.getFileNameWithoutExtension());
    }

    static juce::String unescapePdAtom (const juce::String& atom)
    {
        juce::String result;
        bool escaping = false;

        for (int i = 0; i < atom.length(); ++i)
        {
            const auto c = atom[i];

            if (escaping)
            {
                result << c;
                escaping = false;
            }
            else if (c == '\\')
            {
                escaping = true;
            }
            else
            {
                result << c;
            }
        }

        if (escaping)
            result << '\\';

        return result;
    }

    static juce::StringArray pdAtoms (const juce::String& text)
    {
        juce::StringArray atoms;
        juce::String current;
        bool escaping = false;

        for (int i = 0; i < text.length(); ++i)
        {
            const auto c = text[i];

            if (escaping)
            {
                current << '\\' << c;
                escaping = false;
                continue;
            }

            if (c == '\\')
            {
                escaping = true;
                continue;
            }

            if (c == ' ' || c == '\t')
            {
                if (current.isNotEmpty())
                {
                    atoms.add (current);
                    current.clear();
                }

                continue;
            }

            current << c;
        }

        if (escaping)
            current << '\\';

        if (current.isNotEmpty())
            atoms.add (current);

        return atoms;
    }

    static juce::String stripPdFormatHints (const juce::String& text)
    {
        bool escaping = false;

        for (int i = 0; i < text.length(); ++i)
        {
            const auto c = text[i];

            if (escaping)
            {
                escaping = false;
                continue;
            }

            if (c == '\\')
            {
                escaping = true;
                continue;
            }

            if (c == ',')
                return text.substring (0, i).trim();
        }

        return text.trim();
    }

    static juce::String objectNameFromPdObjectLine (const juce::String& line)
    {
        if (! line.trimStart().startsWith ("#X obj "))
            return {};

        auto atoms = pdAtoms (line.upToFirstOccurrenceOf (";", false, false).fromFirstOccurrenceOf ("#X obj ", false, false));

        return atoms.size() >= 3 ? unescapePdAtom (atoms[2]).trim() : juce::String {};
    }

    static juce::Array<juce::var> abstractionPortCountsForFile (const juce::File& file)
    {
        int inlets = 0;
        int outlets = 0;
        int depth = 0;
        juce::StringArray lines;
        lines.addLines (file.loadFileAsString());

        for (const auto& rawLine : lines)
        {
            const auto line = rawLine.trim();

            if (line.startsWith ("#N canvas "))
            {
                ++depth;
                continue;
            }

            if (line.startsWith ("#X restore "))
            {
                depth = juce::jmax (0, depth - 1);
                continue;
            }

            if (depth != 1)
                continue;

            const auto name = objectNameFromPdObjectLine (line);

            if (name == "inlet" || name == "inlet~")
                ++inlets;
            else if (name == "outlet" || name == "outlet~")
                ++outlets;
        }

        return { inlets, outlets };
    }

    static void addAbstractionPortsFromDirectory (juce::DynamicObject& ports, const juce::File& directory)
    {
        if (! directory.isDirectory())
            return;

        for (const auto& file : directory.findChildFiles (juce::File::findFiles, false, "*.pd"))
        {
            const juce::Identifier abstractionName (file.getFileNameWithoutExtension());

            if (! ports.getProperties().contains (abstractionName))
                ports.setProperty (abstractionName, juce::var (abstractionPortCountsForFile (file)));
        }
    }

    static void addAbstractionSourcesFromDirectory (juce::DynamicObject& sources, const juce::File& directory)
    {
        if (! directory.isDirectory())
            return;

        for (const auto& file : directory.findChildFiles (juce::File::findFiles, false, "*.pd"))
        {
            const juce::Identifier abstractionName (file.getFileNameWithoutExtension());

            if (! sources.getProperties().contains (abstractionName))
                sources.setProperty (abstractionName, juce::var (file.loadFileAsString()));
        }
    }

    static void addHelpPatchSourcesFromDirectory (juce::DynamicObject& sources, const juce::File& directory)
    {
        if (! directory.isDirectory())
            return;

        for (const auto& file : directory.findChildFiles (juce::File::findFiles, true, "*-help.pd"))
            sources.setProperty (file.getFileNameWithoutExtension().upToLastOccurrenceOf ("-help", false, false),
                                 juce::var (file.loadFileAsString()));
    }

    static juce::StringArray declaredPdPaths (const juce::String& patch)
    {
        juce::StringArray paths;
        juce::StringArray lines;
        lines.addLines (patch);

        for (const auto& rawLine : lines)
        {
            const auto line = rawLine.trim();

            if (! line.startsWith ("#X declare ") && ! line.startsWith ("#X obj "))
                continue;

            const auto body = stripPdFormatHints (line.upToFirstOccurrenceOf (";", false, false).trim());
            const auto tokens = pdAtoms (body);
            const auto firstDeclareToken = line.startsWith ("#X declare ") ? 2 : 4;

            if (line.startsWith ("#X obj ") && (tokens.size() <= 4 || tokens[4] != "declare"))
                continue;

            for (int i = firstDeclareToken; i < tokens.size(); ++i)
            {
                const auto token = tokens[i];

                if ((token == "-path" || token == "-stdpath") && i + 1 < tokens.size())
                {
                    const auto path = unescapePdAtom (tokens[++i]).unquoted().trim();

                    if (path.isNotEmpty())
                        paths.addIfNotAlreadyThere (token == "-stdpath" ? juce::String (pdStdPathPrefix) + path : path);
                }
            }
        }

        return paths;
    }

    static juce::String pdMetadataCacheKey (const juce::String& searchPath, const juce::String& patch)
    {
        juce::StringArray lines;
        juce::StringArray declareLines;
        lines.addLines (patch);

        for (const auto& rawLine : lines)
        {
            const auto line = rawLine.trim();

            if (line.startsWith ("#X declare "))
            {
                declareLines.add (line);
                continue;
            }

            if (line.startsWith ("#X obj "))
            {
                const auto body = stripPdFormatHints (line.upToFirstOccurrenceOf (";", false, false).trim());
                const auto tokens = pdAtoms (body);

                if (tokens.size() > 4 && tokens[4] == "declare")
                    declareLines.add (line);
            }
        }

        return searchPath + "\n" + juce::String::toHexString (declareLines.joinIntoString ("\n").hashCode64());
    }

    static juce::File resolvePdProjectPath (const juce::String& path, const juce::File& baseDirectory)
    {
        if (path.startsWith (pdStdPathPrefix))
        {
            const auto standardPath = path.fromFirstOccurrenceOf (pdStdPathPrefix, false, false);

            if (juce::File::isAbsolutePath (standardPath))
                return juce::File (standardPath);

            const auto extraRoot = bundledPdExtraRoot();
            auto resolved = extraRoot.getChildFile (standardPath);

            if (resolved.isDirectory())
                return resolved;

            return extraRoot.getParentDirectory().getChildFile (standardPath);
        }

        const auto declared = juce::File (path);

        if (juce::File::isAbsolutePath (path))
            return declared;

        if (path.startsWith ("./"))
            return baseDirectory.getChildFile (path.fromFirstOccurrenceOf ("./", false, false));

        return baseDirectory.getChildFile (path);
    }

    static void forEachDeclaredPdProjectDirectory (const juce::String& directoryPath,
                                                   const juce::String& patch,
                                                   const std::function<void (const juce::File&)>& visit)
    {
        const auto directory = juce::File (directoryPath);

        for (const auto& declaredPath : declaredPdPaths (patch))
        {
            const auto canResolveWithoutProjectDirectory = declaredPath.startsWith (pdStdPathPrefix)
                                                        || juce::File::isAbsolutePath (declaredPath);

            if (directory.isDirectory() || canResolveWithoutProjectDirectory)
                visit (resolvePdProjectPath (declaredPath, directory));
        }
    }

    static juce::StringArray abstractionNamesForPatchProject (const juce::String& directoryPath, const juce::String& patch)
    {
        juce::StringArray names;
        const auto directory = juce::File (directoryPath);
        addAbstractionNamesFromDirectory (names, directory);
        forEachDeclaredPdProjectDirectory (directoryPath, patch, [&names] (const juce::File& declaredDirectory)
        {
            addAbstractionNamesFromDirectory (names, declaredDirectory);
        });

        return names;
    }

    static juce::var abstractionPortsForPatchProject (const juce::String& directoryPath, const juce::String& patch)
    {
        auto ports = new juce::DynamicObject();
        const auto directory = juce::File (directoryPath);
        addAbstractionPortsFromDirectory (*ports, directory);
        forEachDeclaredPdProjectDirectory (directoryPath, patch, [ports] (const juce::File& declaredDirectory)
        {
            addAbstractionPortsFromDirectory (*ports, declaredDirectory);
        });

        return juce::var (ports);
    }

    static juce::var abstractionSourcesForPatchProject (const juce::String& directoryPath, const juce::String& patch)
    {
        auto sources = new juce::DynamicObject();
        const auto directory = juce::File (directoryPath);
        addAbstractionSourcesFromDirectory (*sources, directory);
        forEachDeclaredPdProjectDirectory (directoryPath, patch, [sources] (const juce::File& declaredDirectory)
        {
            addAbstractionSourcesFromDirectory (*sources, declaredDirectory);
        });

        return juce::var (sources);
    }

    static juce::var helpPatchSourcesForPatchProject (const juce::String& directoryPath, const juce::String& patch)
    {
        auto sources = new juce::DynamicObject();
        addHelpPatchSourcesFromDirectory (*sources, bundledPdExtraRoot());

        const auto directory = juce::File (directoryPath);
        addHelpPatchSourcesFromDirectory (*sources, directory);
        forEachDeclaredPdProjectDirectory (directoryPath, patch, [sources] (const juce::File& declaredDirectory)
        {
            addHelpPatchSourcesFromDirectory (*sources, declaredDirectory);
        });

        return juce::var (sources);
    }

    static juce::File abstractionFileForPatchProject (const juce::String& directoryPath,
                                                      const juce::String& patch,
                                                      const juce::String& abstractionName)
    {
        if (abstractionName.isEmpty())
            return {};

        const auto fileName = abstractionName + ".pd";
        const auto directory = juce::File (directoryPath);

        if (directory.isDirectory())
        {
            auto candidate = directory.getChildFile (fileName);
            if (candidate.existsAsFile())
                return candidate;

            for (const auto& declaredPath : declaredPdPaths (patch))
            {
                candidate = resolvePdProjectPath (declaredPath, directory).getChildFile (fileName);
                if (candidate.existsAsFile())
                    return candidate;
            }
        }

        juce::File found;
        forEachDeclaredPdProjectDirectory (directoryPath, patch, [&found, &fileName] (const juce::File& declaredDirectory)
        {
            if (found.existsAsFile())
                return;

            const auto candidate = declaredDirectory.getChildFile (fileName);
            if (candidate.existsAsFile())
                found = candidate;
        });

        if (found.existsAsFile())
            return found;

        return {};
    }

    static juce::WebBrowserComponent::Options createBrowserOptions (const juce::String& initialPatch,
                                                                    const juce::StringArray& initialAbstractions,
                                                                    const juce::var& initialAbstractionPorts,
                                                                    const juce::var& initialAbstractionSources,
                                                                    const juce::var& initialHelpPatchSources,
                                                                    PdPatchEditorPanel* owner)
    {
        return juce::WebBrowserComponent::Options {}
            .withNativeIntegrationEnabled()
            .withInitialisationData ("patch", initialPatch)
            .withInitialisationData ("abstractions", juce::var (initialAbstractions))
            .withInitialisationData ("abstractionPorts", initialAbstractionPorts)
            .withInitialisationData ("abstractionSources", initialAbstractionSources)
            .withInitialisationData ("helpPatchSources", initialHelpPatchSources)
            .withResourceProvider ([] (const juce::String& path) -> std::optional<juce::WebBrowserComponent::Resource>
            {
                if (path == "/" || path == "/index.html")
                    return stringWebResource (pdPatchEditorHtml(), "text/html");

                return std::nullopt;
            })
            .withEventListener ("patchChanged", [owner] (juce::var payload)
            {
                if (owner != nullptr)
                    owner->handlePatchChanged (payload);
            })
            .withEventListener ("guiTriggered", [owner] (juce::var payload)
            {
                if (owner != nullptr)
                    owner->handleGuiTriggered (payload);
            })
            .withEventListener ("abstractionChanged", [owner] (juce::var payload)
            {
                if (owner != nullptr)
                    owner->handleAbstractionChanged (payload);
            })
            .withEventListener ("monitorReceivers", [owner] (juce::var payload)
            {
                if (owner != nullptr)
                    owner->handleMonitorReceivers (payload);
            })
            .withEventListener ("monitorArrays", [owner] (juce::var payload)
            {
                if (owner != nullptr)
                    owner->handleMonitorArrays (payload);
            });
    }

    void refreshFromDisc()
    {
        const auto info = canvas.getDiscPdPatchInfo (handle, patchIndex);
        const auto enabled = info.valid;

        titleLabel.setText (enabled ? "Pd Patch " + juce::String (patchIndex + 1) : "Pd Patch Removed",
                            juce::dontSendNotification);
        durationLabel.setText (enabled ? durationLabelText (info.durationSeconds)
                                       : "This Pd patch has been removed",
                               juce::dontSendNotification);

        {
            const juce::ScopedValueSetter<bool> suppress (suppressCallbacks, true);
            durationBox.setText (enabled ? durationText (info.durationSeconds) : "-", false);
        }

        durationBox.setEnabled (enabled);
        importButton.setEnabled (enabled);
        exportButton.setEnabled (enabled);
        resetButton.setEnabled (enabled);
        testButton.setEnabled (enabled);
        browser.setEnabled (enabled);
        statusLabel.setText (enabled ? "Pd patch canvas" : "No patch", juce::dontSendNotification);
    }

    void handlePatchChanged (const juce::var& payload)
    {
        if (suppressCallbacks)
            return;

        if (auto* object = payload.getDynamicObject())
        {
            const auto patch = object->getProperty ("patch").toString();

            if (canvas.setDiscPdPatch (handle, patchIndex, patch))
                notifyChanged();
        }
    }

    void handleGuiTriggered (const juce::var& payload)
    {
        handlePatchChanged (payload);

        if (auto* object = payload.getDynamicObject())
        {
            const auto receiver = object->getProperty ("send").toString();
            const auto name = object->getProperty ("name").toString();
            const auto selector = object->getProperty ("selector").toString();
            const auto value = static_cast<float> (object->getProperty ("value"));
            const auto bangOnly = name == "bng";
            const auto triggerPatch = object->getProperty ("triggerPatch").toString();
            juce::StringArray atoms;
            const auto atomPayload = object->getProperty ("atoms");

            if (auto* array = atomPayload.getArray())
                for (const auto& atom : *array)
                    atoms.add (atom.toString());

            if (onGuiTrigger != nullptr)
            {
                statusLabel.setText (onGuiTrigger (receiver, selector, atoms, value, bangOnly, triggerPatch), juce::dontSendNotification);
                refreshMessageSubscriptions (triggerPatch);
                return;
            }
        }

        if (onTest != nullptr)
            statusLabel.setText (onTest(), juce::dontSendNotification);
    }

    void handleMonitorReceivers (const juce::var& payload)
    {
        monitorReceiverTemplates.clear();
        if (auto* object = payload.getDynamicObject())
            if (auto* values = object->getProperty ("receivers").getArray())
                for (const auto& value : *values)
                    monitorReceiverTemplates.addIfNotAlreadyThere (value.toString());
        refreshMessageSubscriptions();
    }

    void handleMonitorArrays (const juce::var& payload)
    {
        monitorArrayTemplates.clear();
        if (auto* object = payload.getDynamicObject())
            if (auto* values = object->getProperty ("arrays").getArray())
                for (const auto& value : *values)
                    monitorArrayTemplates.addIfNotAlreadyThere (value.toString());
        hasArraySnapshotHash = false;
        pollLiveArrays();
    }

    void timerCallback() override
    {
        pollLiveArrays();
    }

    void pollLiveArrays()
    {
        const auto info = canvas.getDiscPdPatchInfo (handle, patchIndex);
        juce::StringArray resolvedNames;
        monitorArrayAliases.clear();

        if (info.valid && ! monitorArrayTemplates.isEmpty())
        {
            const auto dollarZero = audioEngine.getPdPatchDollarZero (info.patch, info.searchPath);
            for (const auto& nameTemplate : monitorArrayTemplates)
            {
                auto name = nameTemplate;
                if (name.contains ("$0"))
                {
                    if (dollarZero <= 0)
                        continue;
                    name = name.replace ("$0", juce::String (dollarZero));
                }
                resolvedNames.addIfNotAlreadyThere (name);
                monitorArrayAliases[name] = nameTemplate;
            }
        }

        const auto snapshots = info.valid ? audioEngine.readPdArrays (info.patch, resolvedNames, info.searchPath)
                                          : std::vector<PdAudioEngine::ArraySnapshot> {};
        std::uint64_t hash = 1469598103934665603ull;
        const auto mixByte = [&hash] (std::uint8_t byte) { hash = (hash ^ byte) * 1099511628211ull; };
        juce::Array<juce::var> arrayPayload;

        for (const auto& snapshot : snapshots)
        {
            auto item = new juce::DynamicObject();
            const auto alias = monitorArrayAliases.find (snapshot.name);
            const auto displayName = alias != monitorArrayAliases.end() ? alias->second : snapshot.name;
            item->setProperty ("name", displayName);
            item->setProperty ("totalSize", snapshot.totalSize);
            for (int shift = 0; shift < 32; shift += 8)
                mixByte (static_cast<std::uint8_t> (static_cast<std::uint32_t> (snapshot.totalSize) >> shift));
            juce::Array<juce::var> values;
            values.ensureStorageAllocated (static_cast<int> (snapshot.values.size()));

            for (const auto value : snapshot.values)
            {
                std::uint32_t bits = 0;
                std::memcpy (&bits, &value, sizeof (bits));
                for (int shift = 0; shift < 32; shift += 8)
                    mixByte (static_cast<std::uint8_t> (bits >> shift));
                values.add (value);
            }
            const auto utf8Name = displayName.toUTF8();
            for (auto* byte = utf8Name.getAddress(); byte != nullptr && *byte != '\0'; ++byte)
                mixByte (static_cast<std::uint8_t> (*byte));
            item->setProperty ("values", juce::var (values));
            arrayPayload.add (juce::var (item));
        }

        mixByte (static_cast<std::uint8_t> (arrayPayload.size() & 0xff));
        if (hasArraySnapshotHash && hash == lastArraySnapshotHash)
            return;

        hasArraySnapshotHash = true;
        lastArraySnapshotHash = hash;
        auto payload = new juce::DynamicObject();
        payload->setProperty ("arrays", juce::var (arrayPayload));
        browser.emitEventIfBrowserIsVisible ("pdArrays", juce::var (payload));
    }

    void refreshMessageSubscriptions (const juce::String& activePatch = {})
    {
        const auto info = canvas.getDiscPdPatchInfo (handle, patchIndex);
        const auto patch = activePatch.trim().isNotEmpty() ? activePatch : info.patch;
        const auto dollarZero = info.valid ? audioEngine.getPdPatchDollarZero (patch, info.searchPath) : 0;
        juce::StringArray resolvedReceivers;
        monitorReceiverAliases.clear();

        for (const auto& receiverTemplate : monitorReceiverTemplates)
        {
            auto receiver = receiverTemplate;
            if (receiver.contains ("$0"))
            {
                if (dollarZero <= 0)
                    continue;
                receiver = receiver.replace ("$0", juce::String (dollarZero));
            }
            resolvedReceivers.addIfNotAlreadyThere (receiver);
            monitorReceiverAliases[receiver] = receiverTemplate;
        }

        audioEngine.setPdMessageSubscriptions (resolvedReceivers);
    }

    void forwardPdMessageToBrowser (const juce::String& receiver,
                                    const juce::String& selector,
                                    const juce::StringArray& atoms)
    {
        auto payload = new juce::DynamicObject();
        const auto alias = monitorReceiverAliases.find (receiver);
        payload->setProperty ("receiver", alias != monitorReceiverAliases.end() ? alias->second : receiver);
        payload->setProperty ("selector", selector);
        juce::Array<juce::var> atomValues;
        for (const auto& atom : atoms)
            atomValues.add (atom);
        payload->setProperty ("atoms", juce::var (atomValues));
        browser.emitEventIfBrowserIsVisible ("pdMessage", juce::var (payload));
    }

    void handleAbstractionChanged (const juce::var& payload)
    {
        if (auto* object = payload.getDynamicObject())
        {
            const auto name = object->getProperty ("name").toString();
            const auto patch = object->getProperty ("patch").toString();
            const auto info = canvas.getDiscPdPatchInfo (handle, patchIndex);

            if (! info.valid)
                return;

            const auto file = abstractionFileForPatchProject (info.searchPath, info.patch, name);

            if (file.existsAsFile() && file.replaceWithText (patch))
            {
                statusLabel.setText ("Saved " + file.getFileName(), juce::dontSendNotification);
                notifyChanged();
            }
            else
            {
                statusLabel.setText ("Could not save abstraction " + name, juce::dontSendNotification);
            }
        }
    }

    void reloadBrowser (const juce::String& patch,
                        const juce::StringArray& abstractionNames = {},
                        const juce::var& abstractionPorts = {},
                        const juce::var& abstractionSources = {},
                        const juce::var& helpPatchSources = {})
    {
        const juce::ScopedValueSetter<bool> suppress (suppressCallbacks, true);
        browser.evaluateJavascript ("canvasStack=[]; updateNestedIndicator(); parsePatch(" + juce::JSON::toString (juce::var (patch))
                                    + "); registerAbstractionNames(" + juce::JSON::toString (juce::var (abstractionNames))
                                    + "); registerAbstractionPorts(" + juce::JSON::toString (abstractionPorts)
                                    + "); registerAbstractionSources(" + juce::JSON::toString (abstractionSources)
                                    + "); registerHelpPatchSources(" + juce::JSON::toString (helpPatchSources)
                                    + "); if (sourceVisible) sourceText.value = serialize(); render(); emit();");
    }

    struct PdProjectMetadata
    {
        juce::StringArray abstractionNames;
        juce::var abstractionPorts;
        juce::var abstractionSources;
        juce::var helpPatchSources;
    };

    void loadProjectMetadataAsync()
    {
        const auto info = canvas.getDiscPdPatchInfo (handle, patchIndex);
        if (! info.valid)
            return;

        const auto generation = ++metadataLoadGeneration;
        const auto patch = info.patch;
        const auto searchPath = info.searchPath;
        const auto cacheKey = pdMetadataCacheKey (searchPath, patch);
        const auto safeThis = juce::Component::SafePointer<PdPatchEditorPanel> (this);
        static juce::CriticalSection metadataCacheLock;
        static std::map<juce::String, std::shared_ptr<const PdProjectMetadata>> metadataCache;

        {
            const juce::ScopedLock lock (metadataCacheLock);
            if (const auto existing = metadataCache.find (cacheKey); existing != metadataCache.end())
            {
                applyProjectMetadata (*existing->second);
                return;
            }
        }

        statusLabel.setText ("Pd patch canvas  /  loading objects", juce::dontSendNotification);

        juce::Thread::launch ([safeThis, generation, patch, searchPath, cacheKey]
        {
            auto metadata = std::make_shared<PdProjectMetadata>();
            metadata->abstractionNames = abstractionNamesForPatchProject (searchPath, patch);
            metadata->abstractionPorts = abstractionPortsForPatchProject (searchPath, patch);
            metadata->abstractionSources = abstractionSourcesForPatchProject (searchPath, patch);
            metadata->helpPatchSources = helpPatchSourcesForPatchProject (searchPath, patch);

            {
                const juce::ScopedLock lock (metadataCacheLock);
                metadataCache[cacheKey] = metadata;
            }

            juce::MessageManager::callAsync ([safeThis, generation, metadata]
            {
                if (safeThis == nullptr || generation != safeThis->metadataLoadGeneration)
                    return;

                safeThis->applyProjectMetadata (*metadata);
            });
        });
    }

    void applyProjectMetadata (const PdProjectMetadata& metadata)
    {
        browser.evaluateJavascript ("registerAbstractionNames(" + juce::JSON::toString (juce::var (metadata.abstractionNames))
                                    + "); registerAbstractionPorts(" + juce::JSON::toString (metadata.abstractionPorts)
                                    + "); registerAbstractionSources(" + juce::JSON::toString (metadata.abstractionSources)
                                    + "); registerHelpPatchSources(" + juce::JSON::toString (metadata.helpPatchSources)
                                    + "); render();");
        statusLabel.setText ("Pd patch canvas", juce::dontSendNotification);
    }

    void importPatch()
    {
        const auto safeThis = juce::Component::SafePointer<PdPatchEditorPanel> (this);
        fileChooser = std::make_unique<juce::FileChooser> ("Import Pd patch",
                                                           juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                           "*.pd");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                                  [safeThis] (const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            const auto file = chooser.getResult();
            if (! file.existsAsFile())
                return;

            const auto patch = file.loadFileAsString();

            if (safeThis->canvas.setDiscPdPatch (safeThis->handle,
                                                 safeThis->patchIndex,
                                                 patch,
                                                 file.getParentDirectory().getFullPathName()))
            {
                safeThis->reloadBrowser (patch);
                safeThis->loadProjectMetadataAsync();
                safeThis->statusLabel.setText ("Imported " + file.getFileName(), juce::dontSendNotification);
                safeThis->notifyChanged();
            }
        });
    }

    void exportPatch()
    {
        const auto info = canvas.getDiscPdPatchInfo (handle, patchIndex);
        if (! info.valid)
            return;

        const auto safeThis = juce::Component::SafePointer<PdPatchEditorPanel> (this);
        const auto defaultFile = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                     .getChildFile ("disc-pd-" + juce::String (patchIndex + 1) + ".pd");
        fileChooser = std::make_unique<juce::FileChooser> ("Export Pd patch",
                                                           defaultFile,
                                                           "*.pd");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [safeThis] (const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            auto file = chooser.getResult();
            if (file == juce::File())
                return;

            if (file.getFileExtension().isEmpty())
                file = file.withFileExtension (".pd");

            const auto current = safeThis->canvas.getDiscPdPatchInfo (safeThis->handle, safeThis->patchIndex);
            if (! current.valid)
                return;

            if (file.replaceWithText (current.patch))
                safeThis->statusLabel.setText ("Exported " + file.getFileName(), juce::dontSendNotification);
            else
                safeThis->statusLabel.setText ("Export failed", juce::dontSendNotification);
        });
    }

    void commitDurationText()
    {
        if (suppressCallbacks)
            return;

        const auto duration = durationFromText (durationBox.getText());

        if (canvas.setDiscPdDuration (handle, patchIndex, duration))
        {
            durationBox.setText (durationText (duration), false);
            durationLabel.setText (durationLabelText (duration), juce::dontSendNotification);
            notifyChanged();
        }
    }

    void notifyChanged()
    {
        if (onChange != nullptr)
            onChange();
    }
};

class OrcaGridEditorPanel final : public juce::Component
{
public:
    using ChangeCallback = std::function<void()>;

    OrcaGridEditorPanel (RoadCanvas& canvasToUse,
                         RoadCanvas::DiscHandle handleToUse,
                         ChangeCallback changeCallback)
        : canvas (canvasToUse),
          handle (std::move (handleToUse)),
          onChange (std::move (changeCallback)),
          editor (model)
    {
        setOpaque (true);
        addAndMakeVisible (titleLabel);
        addAndMakeVisible (infoLabel);
        addAndMakeVisible (clearButton);
        addAndMakeVisible (fitButton);
        addAndMakeVisible (editor);

        styleEditorLabel (titleLabel, 17.0f, true);
        styleEditorLabel (infoLabel, 13.0f, false);
        styleEditorButton (clearButton, "clear");
        styleEditorButton (fitButton, "fit");

        editor.setTheme (createOrcaTheme());
        editor.setRulersVisible (true);

        clearButton.onClick = [this]
        {
            if (canvas.clearDiscOrcaGrid (handle))
            {
                refreshFromDisc();
                notifyChanged();
            }
        };

        fitButton.onClick = [this] { editor.fitToView(); };

        editor.setOnChange ([this]
        {
            if (suppressCallbacks)
                return;

            if (canvas.setDiscOrcaGrid (handle, model.createSnapshot()))
                notifyChanged();
        });

        refreshFromDisc();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff101614));
        g.setColour (juce::Colour (0xff26372f));
        g.drawHorizontalLine (88, 0.0f, static_cast<float> (getWidth()));
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (16, 14);
        titleLabel.setBounds (bounds.removeFromTop (26));
        bounds.removeFromTop (8);

        auto controls = bounds.removeFromTop (34);
        clearButton.setBounds (controls.removeFromLeft (92));
        controls.removeFromLeft (8);
        fitButton.setBounds (controls.removeFromLeft (72));
        bounds.removeFromTop (8);

        infoLabel.setBounds (bounds.removeFromTop (22));
        bounds.removeFromTop (8);
        editor.setBounds (bounds);
    }

    void refreshFromDisc()
    {
        const auto info = canvas.getDiscInfo (handle);
        const auto enabled = info.valid && info.hasOrcaGrid;

        titleLabel.setText (enabled ? "Orca Grid" : "Orca Grid Removed", juce::dontSendNotification);
        infoLabel.setText (enabled
                               ? juce::String (info.orcaGridWidth) + " x " + juce::String (info.orcaGridHeight) + " cells  /  fires 64 frames"
                               : "No Orca grid on this disc",
                           juce::dontSendNotification);

        {
            const juce::ScopedValueSetter<bool> suppress (suppressCallbacks, true);
            model.applySnapshot (enabled ? canvas.getDiscOrcaGrid (handle)
                                         : gridcollider::GridModel (16, 10).createSnapshot());
            editor.clearUndoHistory();
            editor.repaint();
        }

        clearButton.setEnabled (enabled);
        fitButton.setEnabled (enabled);
        editor.setEnabled (enabled);
    }

private:
    RoadCanvas& canvas;
    RoadCanvas::DiscHandle handle;
    ChangeCallback onChange;
    juce::Label titleLabel;
    juce::Label infoLabel;
    juce::TextButton clearButton;
    juce::TextButton fitButton;
    gridcollider::GridModel model { 16, 10 };
    gridcollider::GridEditorComponent editor;
    bool suppressCallbacks = false;

    void notifyChanged()
    {
        if (onChange != nullptr)
            onChange();
    }
};

class CarouselEditorPanel final : public juce::Component
{
public:
    CarouselEditorPanel (RoadCanvas& canvasToUse, RoadCanvas::DiscHandle handleToUse, int indexToUse,
                         ScDiscAudioEngine& audio, std::function<void()> changed)
        : canvas (canvasToUse), handle (std::move (handleToUse)), index (indexToUse), onChange (std::move (changed))
    {
        addAndMakeVisible (editor);
        editor.setDocument (canvas.getDiscCarousel (handle, index));
        editor.onChange = [this] (const CarouselDocument& document)
        {
            if (canvas.setDiscCarousel (handle, index, document) && onChange) onChange();
        };
        editor.onNote = [&audio] (int midi) { audio.triggerMidiNote (midi); };
    }
    ~CarouselEditorPanel() override { editor.setRunning (false); }
    void resized() override { editor.setBounds (getLocalBounds()); }
    void start() { editor.setRunning (true); }
private:
    RoadCanvas& canvas;
    RoadCanvas::DiscHandle handle;
    int index = 0;
    std::function<void()> onChange;
    CarouselEditorComponent editor;
};

class FloatingEditorWindow final : public juce::DocumentWindow
{
public:
    using CloseCallback = std::function<void()>;

    FloatingEditorWindow (juce::String title,
                          juce::Component* content,
                          juce::Component* componentToCentreAround,
                          int width,
                          int height,
                          CloseCallback closeCallback)
        : DocumentWindow (std::move (title),
                          juce::Colour (0xff101614),
                          DocumentWindow::allButtons),
          onClose (std::move (closeCallback))
    {
        setUsingNativeTitleBar (true);
        setResizable (true, true);
        setResizeLimits (420, 280, 2200, 1600);
        setContentOwned (content, true);

        if (componentToCentreAround != nullptr)
            centreAroundComponent (componentToCentreAround, width, height);
        else
            centreWithSize (width, height);

        setVisible (true);
    }

    void closeButtonPressed() override
    {
        setVisible (false);

        if (onClose != nullptr)
            juce::MessageManager::callAsync (onClose);
    }

private:
    CloseCallback onClose;
};

class ElementDotButton final : public juce::Button
{
public:
    enum class Kind
    {
        nestedWorld,
        scCode,
        pdPatch,
        scSheet,
        orcaGrid,
        carousel,
        pipeWorld
    };

    ElementDotButton (Kind kindToUse, int indexToUse, juce::Colour colourToUse, juce::String tooltip)
        : juce::Button (std::move (tooltip)), kind (kindToUse), index (indexToUse), colour (colourToUse)
    {
        setTooltip (getName());
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus (false);
    }

    Kind getKind() const noexcept { return kind; }
    int getIndex() const noexcept { return index; }

    std::function<void (Kind, int)> onSingleClick;
    std::function<void (Kind, int)> onDoubleClick;

    void setSelected (bool shouldBeSelected)
    {
        selected = shouldBeSelected;
        repaint();
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        juce::Button::mouseUp (event);

        if (event.mods.isPopupMenu())
            return;

        if (event.getNumberOfClicks() >= 2)
        {
            if (onDoubleClick != nullptr)
                onDoubleClick (kind, index);
        }
        else if (onSingleClick != nullptr)
            onSingleClick (kind, index);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto area = getLocalBounds().toFloat().reduced (3.0f);
        const auto centre = area.getCentre();
        const auto radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.34f;

        g.setColour (colour.withAlpha (selected ? 0.30f : 0.14f));
        g.fillEllipse (juce::Rectangle<float> (radius * 2.9f, radius * 2.9f).withCentre (centre));

        auto fill = colour;
        if (highlighted)
            fill = fill.brighter (0.10f);
        if (down)
            fill = fill.darker (0.08f);

        g.setColour (fill);
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));

        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.fillEllipse (juce::Rectangle<float> (radius * 0.72f, radius * 0.72f)
                            .withCentre ({ centre.x - radius * 0.32f, centre.y - radius * 0.34f }));

        g.setColour ((selected ? textPrimary() : appBackground()).withAlpha (selected ? 0.92f : 0.50f));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre),
                       selected ? 2.0f : 1.2f);
    }

private:
    Kind kind;
    int index = 0;
    juce::Colour colour;
    bool selected = false;
};

class LayersPanelBackground final : public juce::Component
{
public:
    explicit LayersPanelBackground (bool showAccentToUse = true) : showAccent (showAccentToUse)
    {
        setInterceptsMouseClicks (false, false);
    }

    void setDividerY (int newDividerY)
    {
        dividerY = newDividerY;
        repaint();
    }

    void setAccent (juce::Colour newAccent)
    {
        accent = newAccent;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto panel = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.20f));
        g.fillRoundedRectangle (panel.translated (0.0f, 2.0f).reduced (1.0f), 10.0f);
        g.setColour (surfaceColour().withAlpha (0.94f));
        g.fillRoundedRectangle (panel.reduced (1.0f), 9.0f);
        g.setColour (juce::Colours::white.withAlpha (0.09f));
        g.drawRoundedRectangle (panel.reduced (1.5f), 9.0f, 0.7f);
        if (showAccent)
        {
            g.setColour (accent.withAlpha (0.88f));
            g.fillRoundedRectangle (panel.withWidth (3.0f).reduced (0.0f, 11.0f), 1.5f);
        }
        if (dividerY > 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawHorizontalLine (dividerY, 8.0f, panel.getRight() - 8.0f);
        }
    }

private:
    bool showAccent = true;
    int dividerY = -1;
    juce::Colour accent { accentColour() };
};

class StereoMeter final : public juce::Component
{
public:
    void setLevels (float left, float right) { levels[0] = left; levels[1] = right; repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        for (int channel = 0; channel < 2; ++channel)
        {
            auto bar = area.removeFromTop (area.getHeight() / static_cast<float> (2 - channel)).reduced (0.0f, 2.0f);
            g.setColour (juce::Colour (0xff26312d)); g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (levels[channel] > 0.9f ? juce::Colour (0xffff6961) : juce::Colour (0xff5ee6b8));
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, levels[channel])), 2.0f);
        }
    }
private:
    float levels[2] {};
};

class DiscMixerPanel final : public juce::Component
{
public:
    DiscMixerPanel (RoadCanvas& canvasToUse, float initialMaster,
                    std::function<void(float)> masterChanged,
                    std::function<void()> changed)
        : canvas (canvasToUse), masterGain (initialMaster), onMasterChanged (std::move (masterChanged)), onChanged (std::move (changed))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setSize (920, 500);
    }

    void paint (juce::Graphics& g) override
    {
        channels = canvas.getMixerChannels();
        g.fillAll (juce::Colour (0xff0b100e));
        g.setColour (juce::Colours::white.withAlpha (0.94f)); g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        g.drawText ("Mixer", 24, 18, 180, 28, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff87938d)); g.setFont (juce::FontOptions (11.0f));
        g.drawText (channels.empty() ? "Add discs to create channels" : juce::String (channels.size()) + " disc channels", 24, 45, 220, 20, juce::Justification::centredLeft);

        for (int i = 0; i < static_cast<int> (channels.size()); ++i) drawStrip (g, i, channels[static_cast<size_t> (i)], false);
        RoadCanvas::MixerChannel master; master.name = "Stereo Out"; master.level = masterGain;
        drawStrip (g, static_cast<int> (channels.size()), master, true);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        channels = canvas.getMixerChannels();
        activeStrip = stripAt (event.position);
        if (activeStrip < 0) return;
        const auto master = activeStrip == static_cast<int> (channels.size());
        if (! master) canvas.selectMixerChannel (activeStrip);
        if (! master && muteBounds (activeStrip).contains (event.position))
        {
            auto channel = channels[static_cast<size_t> (activeStrip)]; channel.muted = ! channel.muted;
            canvas.setMixerChannel (activeStrip, channel.level, channel.pan, channel.muted, channel.solo); changed(); activeControl = Control::none; return;
        }
        if (! master && soloBounds (activeStrip).contains (event.position))
        {
            auto channel = channels[static_cast<size_t> (activeStrip)]; channel.solo = ! channel.solo;
            canvas.setMixerChannel (activeStrip, channel.level, channel.pan, channel.muted, channel.solo); changed(); activeControl = Control::none; return;
        }
        if (! master && panBounds (activeStrip).expanded (8.0f).contains (event.position)) { activeControl = Control::pan; updatePan (event.position.x); }
        else if (faderBounds (activeStrip).expanded (14.0f, 8.0f).contains (event.position)) { activeControl = Control::level; updateLevel (event.position.y); }
        else activeControl = Control::none;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (activeControl == Control::pan) updatePan (event.position.x);
        else if (activeControl == Control::level) updateLevel (event.position.y);
    }
    void mouseUp (const juce::MouseEvent&) override { activeStrip = -1; activeControl = Control::none; }

private:
    enum class Control { none, level, pan };
    static constexpr float stripWidth = 108.0f, stripGap = 12.0f, left = 24.0f, top = 82.0f;
    juce::Rectangle<float> stripBounds (int index) const { return { left + static_cast<float> (index) * (stripWidth + stripGap), top, stripWidth, static_cast<float> (getHeight()) - top - 24.0f }; }
    juce::Rectangle<float> panBounds (int index) const { return stripBounds (index).reduced (13.0f).withTrimmedTop (78.0f).removeFromTop (9.0f); }
    juce::Rectangle<float> faderBounds (int index) const { auto b = stripBounds (index); return { b.getCentreX() - 15.0f, b.getY() + 128.0f, 30.0f, b.getHeight() - 194.0f }; }
    juce::Rectangle<float> muteBounds (int index) const { auto b = stripBounds (index); return { b.getX() + 14.0f, b.getBottom() - 43.0f, 34.0f, 25.0f }; }
    juce::Rectangle<float> soloBounds (int index) const { auto b = stripBounds (index); return { b.getRight() - 48.0f, b.getBottom() - 43.0f, 34.0f, 25.0f }; }
    int stripAt (juce::Point<float> point) const
    {
        for (int i = 0; i <= static_cast<int> (channels.size()); ++i) if (stripBounds (i).contains (point)) return i;
        return -1;
    }
    void drawStrip (juce::Graphics& g, int index, const RoadCanvas::MixerChannel& channel, bool master)
    {
        auto strip = stripBounds (index); const auto accent = master ? juce::Colour (0xffffbd4a) : juce::Colour (0xff2997ff);
        g.setColour (channel.selected ? accent.withAlpha (0.16f) : juce::Colour (0xff151c19)); g.fillRoundedRectangle (strip, 6.0f);
        g.setColour (accent.withAlpha (master ? 0.72f : channel.selected ? 0.58f : 0.24f)); g.drawRoundedRectangle (strip, 6.0f, 1.0f);
        auto tag = strip.removeFromTop (28.0f).reduced (9.0f, 7.0f); g.setColour (accent.withAlpha (0.18f)); g.fillRoundedRectangle (tag, 3.0f);
        g.setColour (accent.withAlpha (0.88f)); g.setFont (juce::FontOptions (9.0f, juce::Font::bold)); g.drawText (master ? "MASTER" : "DISC " + juce::String (index + 1), tag, juce::Justification::centred);
        g.setColour (juce::Colours::white.withAlpha (0.88f)); g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawFittedText (channel.name, stripBounds (index).withTrimmedTop (34.0f).removeFromTop (30.0f).toNearestInt().reduced (5, 0), juce::Justification::centred, 1);
        if (! master)
        {
            const auto pan = panBounds (index); g.setColour (juce::Colour (0xff34423c)); g.fillRoundedRectangle (pan, 3.0f);
            g.setColour (accent.withAlpha (0.85f)); const auto x = pan.getX() + pan.getWidth() * (channel.pan + 1.0f) * 0.5f; g.fillEllipse (x - 5.0f, pan.getCentreY() - 5.0f, 10.0f, 10.0f);
            g.setColour (juce::Colour (0xff87938d)); g.setFont (juce::FontOptions (9.0f)); g.drawText ("PAN", pan.translated (0, -16), juce::Justification::centred);
        }
        const auto fader = faderBounds (index); const auto norm = juce::jlimit (0.0f, 1.0f, channel.level / 1.5f); const auto y = fader.getBottom() - fader.getHeight() * norm;
        g.setColour (juce::Colour (0xff34423c)); g.fillRoundedRectangle (fader.withWidth (7.0f).withCentre (fader.getCentre()), 3.5f);
        g.setColour (accent.withAlpha (0.78f)); g.fillRoundedRectangle ({ fader.getCentreX() - 3.5f, y, 7.0f, fader.getBottom() - y }, 3.5f);
        g.setColour (juce::Colours::white); g.fillRoundedRectangle ({ fader.getCentreX() - 11.0f, y - 7.0f, 22.0f, 14.0f }, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.78f)); g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (juce::String (channel.level, 2), stripBounds (index).withTrimmedBottom (48.0f).removeFromBottom (22.0f), juce::Justification::centred);
        if (! master)
        {
            drawToggle (g, muteBounds (index), "M", channel.muted, juce::Colour (0xffff6961));
            drawToggle (g, soloBounds (index), "S", channel.solo, juce::Colour (0xffffbd4a));
        }
    }
    static void drawToggle (juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& text, bool active, juce::Colour colour)
    {
        g.setColour (active ? colour.withAlpha (0.86f) : juce::Colour (0xff202925)); g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (active ? juce::Colour (0xff08100d) : juce::Colours::white.withAlpha (0.62f)); g.setFont (juce::FontOptions (10.0f, juce::Font::bold)); g.drawText (text, bounds, juce::Justification::centred);
    }
    void updateLevel (float y)
    {
        if (activeStrip < 0) return; const auto fader = faderBounds (activeStrip);
        const auto level = juce::jlimit (0.0f, 1.5f, (fader.getBottom() - y) / fader.getHeight() * 1.5f);
        if (activeStrip == static_cast<int> (channels.size())) { masterGain = level; if (onMasterChanged) onMasterChanged (level); }
        else { auto c = channels[static_cast<size_t> (activeStrip)]; canvas.setMixerChannel (activeStrip, level, c.pan, c.muted, c.solo); }
        changed(); repaint();
    }
    void updatePan (float x)
    {
        if (! juce::isPositiveAndBelow (activeStrip, static_cast<int> (channels.size()))) return; const auto pan = panBounds (activeStrip);
        const auto value = juce::jlimit (-1.0f, 1.0f, (x - pan.getX()) / pan.getWidth() * 2.0f - 1.0f);
        auto c = channels[static_cast<size_t> (activeStrip)]; canvas.setMixerChannel (activeStrip, c.level, value, c.muted, c.solo); changed(); repaint();
    }
    void changed() { if (onChanged) onChanged(); }
    RoadCanvas& canvas; float masterGain = 1.0f; std::vector<RoadCanvas::MixerChannel> channels;
    std::function<void(float)> onMasterChanged; std::function<void()> onChanged;
    int activeStrip = -1; Control activeControl = Control::none;
};

class MainComponent final : public juce::AudioAppComponent,
                            private juce::MidiInputCallback,
                            private juce::Timer,
                            public juce::MenuBarModel
{
public:
    MainComponent()
        : selectButton (IconButton::Icon::select, "Select"),
          drawButton (IconButton::Icon::draw, "Lay pipe"),
          warpPipeButton (IconButton::Icon::warpPipe, "Lay warp pipe"),
          editButton (IconButton::Icon::edit, "Edit nodes"),
          discButton (IconButton::Icon::disc, "Place disc"),
          addElementButton (IconButton::Icon::addElement, "Add element"),
          eraseButton (IconButton::Icon::erase, "Erase"),
          tapButton (IconButton::Icon::tap, "Place tap source"),
          drainButton (IconButton::Icon::drain, "Place drain"),
          cloneButton (IconButton::Icon::clone, "Quantum"),
          speedLimitButton (IconButton::Icon::speedLimit, "Speed limit"),
          waitButton (IconButton::Icon::wait, "Wait"),
          strikeButton (IconButton::Icon::strike, "Strike"),
          teleportButton (IconButton::Icon::teleport, "Teleport"),
          filterButton (IconButton::Icon::filter, "Speed filter"),
          logicButton (IconButton::Icon::logic, "Logic"),
          modulatorButton (IconButton::Icon::modulator, "Place modulator"),
          modConnectButton (IconButton::Icon::modConnect, "Connect modulation"),
          nestedWorldDot (ElementDotButton::Kind::nestedWorld, 0, worldElementColour(), "Nested world"),
          scCodeDot (ElementDotButton::Kind::scCode, 0, scCodeElementColour(), "SC code"),
          pdPatchDot (ElementDotButton::Kind::pdPatch, 0, pdPatchElementColour(), "Pd patch"),
          scSheetDot (ElementDotButton::Kind::scSheet, 0, scSheetElementColour(), "SCsheet"),
          orcaGridDot (ElementDotButton::Kind::orcaGrid, 0, orcaGridElementColour(), "Orca grid"),
          carouselDot (ElementDotButton::Kind::carousel, 0, carouselElementColour(), "Carousel"),
          pipeWorldDot (ElementDotButton::Kind::pipeWorld, 0, pipeElementColour(), "Pipe world")
    {
        setOpaque (true);
        setLookAndFeel (&minimalLookAndFeel);

        addAndMakeVisible (canvas);
        addAndMakeVisible (leftToolPanelBackground);
        addAndMakeVisible (layersPanelBackground);

        addAndMakeVisible (selectButton);
        addAndMakeVisible (drawButton);
        addAndMakeVisible (warpPipeButton);
        addAndMakeVisible (editButton);
        addAndMakeVisible (discButton);
        addAndMakeVisible (eraseButton);
        addAndMakeVisible (tapButton);
        addAndMakeVisible (drainButton);
        addAndMakeVisible (cloneButton);
        addAndMakeVisible (speedLimitButton);
        addAndMakeVisible (waitButton);
        addAndMakeVisible (strikeButton);
        addAndMakeVisible (teleportButton);
        addAndMakeVisible (filterButton);
        addAndMakeVisible (logicButton);
        addAndMakeVisible (modulatorButton);
        addAndMakeVisible (modConnectButton);

        for (auto* button : { &selectButton, &drawButton, &warpPipeButton, &editButton, &discButton,
                              &tapButton, &drainButton, &cloneButton, &speedLimitButton, &waitButton, &strikeButton, &teleportButton,
                              &filterButton, &logicButton, &modulatorButton, &modConnectButton, &eraseButton })
            button->setIconScale (1.5f);

        addAndMakeVisible (flowButton);
        addAndMakeVisible (pauseButton);
        addAndMakeVisible (stopButton);
        addAndMakeVisible (elapsedTimeLabel);
        addAndMakeVisible (snapToggle);
        addAndMakeVisible (tempoLabel);
        addAndMakeVisible (tempoEditor);
        addAndMakeVisible (gridUnitLabel);
        addAndMakeVisible (gridUnitBox);
        addAndMakeVisible (triggerQuantizeSlider);
        addAndMakeVisible (clockButton);
        addAndMakeVisible (statusLabel);
        addAndMakeVisible (masterMeter);
        addAndMakeVisible (layersTitleLabel);
        addAndMakeVisible (layersPathLabel);
        addAndMakeVisible (layersMainButton);
        addAndMakeVisible (layersRootButton);
        addAndMakeVisible (layersUpButton);
        addAndMakeVisible (layersEnterButton);
        addAndMakeVisible (layersModulationButton);
        addAndMakeVisible (dataPaneTitle);
        addAndMakeVisible (dataPaneSummary);
        addAndMakeVisible (triggerModeLabel);
        addAndMakeVisible (triggerModeBox);
        addAndMakeVisible (holdDropsToggle);
        addAndMakeVisible (discLevelSlider); addAndMakeVisible (discPanSlider);
        addAndMakeVisible (discMuteButton); addAndMakeVisible (discSoloButton);
        addAndMakeVisible (elementModeLabel);
        addAndMakeVisible (elementModeBox);
        addAndMakeVisible (elementProbabilityBox);
        addAndMakeVisible (nestedWorldDot);
        addAndMakeVisible (scCodeDot);
        addAndMakeVisible (pdPatchDot);
        addAndMakeVisible (scSheetDot);
        addAndMakeVisible (orcaGridDot);
        addAndMakeVisible (carouselDot);
        addAndMakeVisible (pipeWorldDot);
        addAndMakeVisible (worldLabel);
        addAndMakeVisible (worldInfoLabel);
        addAndMakeVisible (worldRemoveButton);
        addAndMakeVisible (worldAddButton);
        addAndMakeVisible (worldEnterButton);
        addAndMakeVisible (scCodeLabel);
        addAndMakeVisible (scCodeOpenButton);
        addAndMakeVisible (scCodeMinusButton);
        addAndMakeVisible (scCodePlusButton);
        addAndMakeVisible (scCodeInfoLabel);
        addAndMakeVisible (pdPatchLabel);
        addAndMakeVisible (pdPatchOpenButton);
        addAndMakeVisible (pdPatchMinusButton);
        addAndMakeVisible (pdPatchPlusButton);
        addAndMakeVisible (pdPatchInfoLabel);
        addAndMakeVisible (scSheetLabel);
        addAndMakeVisible (scSheetOpenButton);
        addAndMakeVisible (scSheetMinusButton);
        addAndMakeVisible (scSheetPlusButton);
        addAndMakeVisible (scSheetInfoLabel);
        addAndMakeVisible (orcaGridLabel);
        addAndMakeVisible (orcaGridOpenButton);
        addAndMakeVisible (orcaGridMinusButton);
        addAndMakeVisible (orcaGridPlusButton);
        addAndMakeVisible (orcaGridInfoLabel);
        addAndMakeVisible (carouselLabel);
        addAndMakeVisible (carouselOpenButton);
        addAndMakeVisible (carouselMinusButton);
        addAndMakeVisible (carouselPlusButton);
        addAndMakeVisible (carouselInfoLabel);
        addAndMakeVisible (pipeWorldLabel);
        addAndMakeVisible (pipeWorldOpenButton);
        addAndMakeVisible (pipeWorldMinusButton);
        addAndMakeVisible (pipeWorldPlusButton);
        addAndMakeVisible (pipeWorldInfoLabel);
        addAndMakeVisible (fireDiscButton);
        addAndMakeVisible (dataPaneCloseButton);

        selectButton.setRadioGroupId (1001);
        drawButton.setRadioGroupId (1001);
        warpPipeButton.setRadioGroupId (1001);
        editButton.setRadioGroupId (1001);
        discButton.setRadioGroupId (1001);
        eraseButton.setRadioGroupId (1001);
        tapButton.setRadioGroupId (1001);
        drainButton.setRadioGroupId (1001);
        cloneButton.setRadioGroupId (1001);
        speedLimitButton.setRadioGroupId (1001);
        waitButton.setRadioGroupId (1001);
        strikeButton.setRadioGroupId (1001);
        teleportButton.setRadioGroupId (1001);
        filterButton.setRadioGroupId (1001);
        logicButton.setRadioGroupId (1001);
        modulatorButton.setRadioGroupId (1001);
        modConnectButton.setRadioGroupId (1001);
        selectButton.setClickingTogglesState (true);
        drawButton.setClickingTogglesState (true);
        warpPipeButton.setClickingTogglesState (true);
        editButton.setClickingTogglesState (true);
        discButton.setClickingTogglesState (true);
        eraseButton.setClickingTogglesState (true);
        tapButton.setClickingTogglesState (true);
        drainButton.setClickingTogglesState (true);
        cloneButton.setClickingTogglesState (true);
        speedLimitButton.setClickingTogglesState (true);
        waitButton.setClickingTogglesState (true);
        strikeButton.setClickingTogglesState (true);
        teleportButton.setClickingTogglesState (true);
        filterButton.setClickingTogglesState (true);
        logicButton.setClickingTogglesState (true);
        modulatorButton.setClickingTogglesState (true);
        modConnectButton.setClickingTogglesState (true);
        selectButton.setToggleState (true, juce::dontSendNotification);

        selectButton.onClick = [this] { setTool (RoadCanvas::Tool::select); };
        drawButton.onClick = [this] { setTool (RoadCanvas::Tool::pipe); };
        warpPipeButton.setTooltip ("Lay a dotted pipe that drops cross instantly");
        warpPipeButton.onClick = [this] { setTool (RoadCanvas::Tool::warpPipe); };
        editButton.onClick = [this] { setTool (RoadCanvas::Tool::edit); };
        discButton.onClick = [this] { setTool (RoadCanvas::Tool::disc); };
        addElementButton.onClick = [this] { addElementToSelectedDisc(); };
        eraseButton.onClick = [this] { setTool (RoadCanvas::Tool::erase); };
        tapButton.setTooltip ("Place or remove a flow source on a pipe");
        tapButton.onClick = [this] { setTool (RoadCanvas::Tool::tap); };
        drainButton.setTooltip ("Place a probabilistic drop drain on a pipe");
        drainButton.onClick = [this] { setTool (RoadCanvas::Tool::drain); };
        cloneButton.setTooltip ("Place a Quantum drop multiplier on a pipe");
        cloneButton.onClick = [this] { setTool (RoadCanvas::Tool::clone); };
        speedLimitButton.setTooltip ("Set passing drop speed relative to global BPM");
        speedLimitButton.onClick = [this] { setTool (RoadCanvas::Tool::speedLimit); };
        waitButton.setTooltip ("Hold passing drops for a number of beats");
        waitButton.onClick = [this] { setTool (RoadCanvas::Tool::wait); };
        strikeButton.setTooltip ("Trigger adjacent discs simultaneously from a passing drop");
        strikeButton.onClick = [this] { setTool (RoadCanvas::Tool::strike); };
        teleportButton.setTooltip ("Teleport passing drops to another teleport");
        teleportButton.onClick = [this] { setTool (RoadCanvas::Tool::teleport); };
        filterButton.setTooltip ("Filter passing drops by their current speed");
        filterButton.onClick = [this] { setTool (RoadCanvas::Tool::filter); };
        logicButton.setTooltip ("Place a gate, counter, switch, comparison or drop rule");
        logicButton.onClick = [this] { setTool (RoadCanvas::Tool::logic); };
        modulatorButton.setTooltip ("Place a transport-synchronised modulation source");
        modulatorButton.onClick = [this] { setTool (RoadCanvas::Tool::modulator); };
        modConnectButton.setTooltip ("Draw modulation from a source to a main-layer target");
        modConnectButton.onClick = [this] { setTool (RoadCanvas::Tool::modConnect); };
        styleEditorButton (flowButton, "Play");
        flowButton.setTooltip ("Play or resume the top-level flow");
        flowButton.onClick = [this]
        {
            setTransportRunning (true);
        };
        styleEditorButton (pauseButton, "Pause");
        pauseButton.setTooltip ("Pause the top-level flow at its current position");
        pauseButton.onClick = [this] { setTransportRunning (false); };
        styleEditorButton (stopButton, "Stop");
        stopButton.setTooltip ("Stop playback and return to the start");
        stopButton.onClick = [this] { resetTransport(); };
        configurePaneLabel (elapsedTimeLabel, 12.0f, false);
        elapsedTimeLabel.setJustificationType (juce::Justification::centred);
        elapsedTimeLabel.setText ("00:00", juce::dontSendNotification);
        snapToggle.setTooltip ("Snap pipes and discs to the grid");
        tempoEditor.setTooltip ("Global tempo in beats per minute");
        gridUnitBox.setTooltip ("Musical duration represented by one grid unit");
        layersRootButton.onClick = [this] { exitToRootWorld(); };
        layersUpButton.onClick = [this] { exitNestedWorld(); };
        layersEnterButton.onClick = [this] { enterNestedWorldForSelectedDisc(); };
        layersMainButton.onClick = [this]
        {
            if (canvas.isModulationLayerVisible()) toggleModulationLayer();
        };
        layersModulationButton.onClick = [this]
        {
            if (! canvas.isModulationLayerVisible() && canvas.getWorldDepth() == 0) toggleModulationLayer();
        };

        soundMinusButton.onClick = [this]
        {
            canvas.removeElementFromSelectedDisc();
            refreshDataPane();
            updateStatus();
        };

        soundPlusButton.onClick = [this]
        {
            canvas.addElementToSelectedDisc();
            refreshDataPane();
            updateStatus();
        };

        worldRemoveButton.onClick = [this]
        {
            canvas.removeNestedWorldFromSelectedDisc();
            refreshDataPane();
            updateStatus();
        };

        worldAddButton.onClick = [this]
        {
            canvas.addNestedWorldToSelectedDisc();
            refreshDataPane();
            updateStatus();
        };

        scCodeMinusButton.onClick = [this]
        {
            canvas.removeScCodeFromSelectedDisc();
            scCodeWindow = nullptr;
            refreshDataPane();
            updateStatus();
        };

        scCodePlusButton.onClick = [this]
        {
            if (canvas.addScCodeToSelectedDisc())
            {
                refreshDataPane();
                const auto info = canvas.getSelectedDiscInfo();
                selectedElement = ElementSelection { ElementDotButton::Kind::scCode,
                                                     juce::jmax (0, info.scCodeCount - 1) };
                refreshDataPane();
                updateStatus();
                openScCodeWindow();
            }
        };

        scCodeOpenButton.onClick = [this] { openScCodeWindow(); };

        pdPatchMinusButton.onClick = [this]
        {
            canvas.removePdPatchFromSelectedDisc();
            pdPatchWindow = nullptr;
            refreshDataPane();
            updateStatus();
            resized();
        };

        pdPatchPlusButton.onClick = [this]
        {
            if (canvas.addPdPatchToSelectedDisc())
            {
                refreshDataPane();
                const auto info = canvas.getSelectedDiscInfo();
                selectedElement = ElementSelection { ElementDotButton::Kind::pdPatch,
                                                     juce::jmax (0, info.pdPatchCount - 1) };
                refreshDataPane();
                updateStatus();
                resized();
                openPdPatchWindow();
            }
        };

        pdPatchOpenButton.onClick = [this] { openPdPatchWindow(); };

        scSheetMinusButton.onClick = [this]
        {
            canvas.removeScSheetFromSelectedDisc();
            scSheetWindow = nullptr;
            refreshDataPane();
            updateStatus();
            resized();
        };

        scSheetPlusButton.onClick = [this]
        {
            if (canvas.addScSheetToSelectedDisc())
            {
                refreshDataPane();
                updateStatus();
                resized();
                openScSheetWindow();
            }
        };

        scSheetOpenButton.onClick = [this] { openScSheetWindow(); };

        orcaGridMinusButton.onClick = [this]
        {
            canvas.removeOrcaGridFromSelectedDisc();
            orcaGridWindow = nullptr;
            refreshDataPane();
            updateStatus();
            resized();
        };

        orcaGridPlusButton.onClick = [this]
        {
            if (canvas.addOrcaGridToSelectedDisc())
            {
                refreshDataPane();
                updateStatus();
                resized();
                openOrcaGridWindow();
            }
        };

        orcaGridOpenButton.onClick = [this] { openOrcaGridWindow(); };

        carouselMinusButton.onClick = [this] { canvas.removeCarouselFromSelectedDisc(); carouselWindow = nullptr; refreshDataPane(); updateStatus(); resized(); };
        carouselPlusButton.onClick = [this] { if (canvas.addCarouselToSelectedDisc()) { const auto info=canvas.getSelectedDiscInfo(); selectedElement=ElementSelection{ElementDotButton::Kind::carousel,juce::jmax(0,info.carouselCount-1)}; refreshDataPane(); updateStatus(); resized(); openCarouselWindow(); } };
        carouselOpenButton.onClick = [this] { openCarouselWindow(); };
        pipeWorldMinusButton.onClick = [this] { canvas.removePipeWorldFromSelectedDisc(); pipeElementWindow = nullptr; refreshDataPane(); updateStatus(); resized(); };
        pipeWorldPlusButton.onClick = [this] { if (canvas.addPipeWorldToSelectedDisc()) { const auto info=canvas.getSelectedDiscInfo(); selectedElement=ElementSelection{ElementDotButton::Kind::pipeWorld,juce::jmax(0,info.pipeWorldCount-1)}; refreshDataPane(); updateStatus(); resized(); openPipeElementWindow(); } };
        pipeWorldOpenButton.onClick = [this] { openPipeElementWindow(); };

        worldEnterButton.onClick = [this] { enterNestedWorldForSelectedDisc(); };
        fireDiscButton.onClick = [this] { fireSelectedDisc(); };
        dataPaneCloseButton.onClick = [this]
        {
            stopPreviewAudio();
            dataPaneOpen = false;
            selectedElement = {};
            resized();
            repaint();
        };

        auto selectElementDot = [this] (ElementDotButton::Kind kind, int index)
        {
            selectedElement = ElementSelection { kind, index };
            refreshElementDots();
            grabKeyboardFocus();
        };

        auto openElementDot = [this] (ElementDotButton::Kind kind, int index)
        {
            selectedElement = ElementSelection { kind, index };
            refreshElementDots();
            openSelectedElementDot();
        };

        nestedWorldDot.onSingleClick = selectElementDot;
        scCodeDot.onSingleClick = selectElementDot;
        pdPatchDot.onSingleClick = selectElementDot;
        scSheetDot.onSingleClick = selectElementDot;
        orcaGridDot.onSingleClick = selectElementDot;
        carouselDot.onSingleClick = selectElementDot;
        pipeWorldDot.onSingleClick = selectElementDot;
        nestedWorldDot.onDoubleClick = openElementDot;
        scCodeDot.onDoubleClick = openElementDot;
        pdPatchDot.onDoubleClick = openElementDot;
        scSheetDot.onDoubleClick = openElementDot;
        orcaGridDot.onDoubleClick = openElementDot;
        carouselDot.onDoubleClick = openElementDot;
        pipeWorldDot.onDoubleClick = openElementDot;

        snapToggle.setButtonText ("Snap");
        snapToggle.setToggleState (true, juce::dontSendNotification);
        snapToggle.setColour (juce::ToggleButton::textColourId, textMuted());
        snapToggle.onClick = [this] { canvas.setSnapEnabled (snapToggle.getToggleState()); };

        styleEditorLabel (tempoLabel, 12.0f, false);
        tempoLabel.setText ("BPM", juce::dontSendNotification);
        styleScDurationBox (tempoEditor);
        tempoEditor.getProperties().set ("compactTransport", true);
        tempoEditor.setColour (juce::TextEditor::backgroundColourId, raisedSurface());
        tempoEditor.setColour (juce::TextEditor::focusedOutlineColourId, accentColour());
        tempoEditor.setInputRestrictions (6, "0123456789.");
        tempoEditor.setJustification (juce::Justification::centred);
        tempoEditor.setText (juce::String (globalTempoBpm, 0), false);
        tempoEditor.onReturnKey = [this] { commitGlobalTempo(); };
        tempoEditor.onFocusLost = [this] { commitGlobalTempo(); };

        styleEditorLabel (gridUnitLabel, 12.0f, false);
        gridUnitLabel.setText ("Grid", juce::dontSendNotification);
        gridUnitLabel.setVisible (false);
        gridUnitBox.addItemList ({ "1/16 beat", "1/8 beat", "1/4 beat", "1/2 beat",
                                   "1 beat", "1 bar", "2 bars", "4 bars" }, 1);
        gridUnitBox.setSelectedId (gridUnitChoice, juce::dontSendNotification);
        gridUnitBox.setColour (juce::ComboBox::backgroundColourId, raisedSurface());
        gridUnitBox.setColour (juce::ComboBox::textColourId, textPrimary());
        gridUnitBox.setColour (juce::ComboBox::outlineColourId, subtleStroke());
        gridUnitBox.setColour (juce::ComboBox::arrowColourId, textMuted());
        gridUnitBox.onChange = [this]
        {
            const auto choice = gridUnitBox.getSelectedId();
            if (choice <= 0 || choice == gridUnitChoice) return;
            gridUnitChoice = choice;
            canvas.setFlowTiming (globalTempoBpm, beatsForGridChoice (gridUnitChoice));
            markProjectDirty();
        };
        triggerQuantizeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        triggerQuantizeSlider.setRange (0.0, 5.0, 1.0);
        triggerQuantizeSlider.setValue (triggerQuantizeChoice, juce::dontSendNotification);
        triggerQuantizeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
        triggerQuantizeSlider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff25302c));
        triggerQuantizeSlider.setColour (juce::Slider::trackColourId, accentColour());
        triggerQuantizeSlider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        triggerQuantizeSlider.setColour (juce::Slider::textBoxTextColourId, textPrimary());
        triggerQuantizeSlider.setColour (juce::Slider::textBoxOutlineColourId, subtleStroke());
        triggerQuantizeSlider.setTooltip ("Quantize disc triggers from immediate to the next bar");
        triggerQuantizeSlider.textFromValueFunction = [] (double value)
        {
            static const std::array<const char*, 6> labels { "None", "1/16", "1/8", "1/4", "1/2", "Bar" };
            return juce::String (labels[static_cast<size_t> (juce::jlimit (0, 5, static_cast<int> (std::round (value))))]);
        };
        triggerQuantizeSlider.onValueChange = [this]
        {
            triggerQuantizeChoice = juce::jlimit (0, 5, static_cast<int> (std::round (triggerQuantizeSlider.getValue())));
            markProjectDirty();
        };
        styleEditorButton (clockButton, "Clocks");
        clockButton.setTooltip ("Configure auxiliary clocks");
        clockButton.onClick = [this] { openClockSettings(); };
        refreshClockButton();

        statusLabel.setJustificationType (juce::Justification::centredRight);
        statusLabel.setColour (juce::Label::textColourId, textMuted());

        configurePaneLabel (layersTitleLabel, 11.0f, true);
        configurePaneLabel (layersPathLabel, 11.0f, false);
        configurePaneButton (layersMainButton, "Main");
        configurePaneButton (layersRootButton, "Root");
        configurePaneButton (layersUpButton, "Back");
        configurePaneButton (layersEnterButton, "Enter world");
        configurePaneButton (layersModulationButton, "Modulation");
        layersMainButton.setClickingTogglesState (false);
        layersModulationButton.setClickingTogglesState (false);
        layersModulationButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffa96de8));
        configurePaneLabel (dataPaneTitle, 18.0f, true);
        configurePaneLabel (dataPaneSummary, 13.0f, false);
        configurePaneLabel (triggerModeLabel, 12.0f, false);
        triggerModeLabel.setText ("Trigger", juce::dontSendNotification);
        triggerModeBox.addItem ("Every drop", 1);
        triggerModeBox.addItem ("After finish", 2);
        triggerModeBox.addItem ("Exclusive", 3);
        triggerModeBox.setTooltip ("Choose how this disc responds to arriving drops");
        triggerModeBox.setColour (juce::ComboBox::backgroundColourId, raisedSurface());
        triggerModeBox.setColour (juce::ComboBox::textColourId, textPrimary());
        triggerModeBox.setColour (juce::ComboBox::outlineColourId, subtleStroke());
        triggerModeBox.setColour (juce::ComboBox::arrowColourId, textMuted());
        triggerModeBox.onChange = [this]
        {
            if (triggerModeBox.getSelectedId() > 0)
                canvas.setSelectedDiscTriggerMode (triggerModeBox.getSelectedId() - 1);
        };
        holdDropsToggle.setButtonText ("Hold drops until playback finishes");
        holdDropsToggle.setTooltip ("Queue arriving drops at this disc while it is playing");
        holdDropsToggle.setColour (juce::ToggleButton::textColourId, textPrimary());
        holdDropsToggle.onClick = [this] { canvas.setSelectedDiscHoldDrops (holdDropsToggle.getToggleState()); };
        const auto configureMixSlider = [] (juce::Slider& slider)
        {
            slider.setSliderStyle (juce::Slider::LinearHorizontal); slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
            slider.setColour (juce::Slider::trackColourId, juce::Colour (0xff2997ff)); slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
            slider.setColour (juce::Slider::textBoxTextColourId, textPrimary()); slider.setColour (juce::Slider::textBoxOutlineColourId, subtleStroke());
        };
        configureMixSlider (discLevelSlider); configureMixSlider (discPanSlider);
        discLevelSlider.setRange (0.0, 1.5, 0.01); discLevelSlider.setTextValueSuffix ("x");
        discPanSlider.setRange (-1.0, 1.0, 0.01);
        configurePaneButton (discMuteButton, "M"); configurePaneButton (discSoloButton, "S");
        discMuteButton.setClickingTogglesState (true); discSoloButton.setClickingTogglesState (true);
        const auto updateMix = [this] { canvas.setSelectedDiscMix (static_cast<float> (discLevelSlider.getValue()), static_cast<float> (discPanSlider.getValue()), discMuteButton.getToggleState(), discSoloButton.getToggleState()); };
        discLevelSlider.onValueChange = updateMix; discPanSlider.onValueChange = updateMix; discMuteButton.onClick = updateMix; discSoloButton.onClick = updateMix;
        configurePaneLabel (elementModeLabel, 12.0f, false);
        elementModeLabel.setText ("Timeline", juce::dontSendNotification);
        elementModeBox.addItem ("Together", 1);
        elementModeBox.addItem ("Sequential", 2);
        elementModeBox.addItem ("Chain", 3);
        elementModeBox.addItem ("Random", 4);
        elementModeBox.addItem ("Probability", 5);
        elementModeBox.setTooltip ("Choose how this disc schedules its elements");
        elementModeBox.setColour (juce::ComboBox::backgroundColourId, raisedSurface());
        elementModeBox.setColour (juce::ComboBox::textColourId, textPrimary());
        elementModeBox.setColour (juce::ComboBox::outlineColourId, subtleStroke());
        elementModeBox.setColour (juce::ComboBox::arrowColourId, textMuted());
        elementModeBox.onChange = [this]
        {
            const auto mode = elementModeBox.getSelectedId() - 1;
            if (mode >= 0) canvas.setSelectedDiscElementMode (mode);
            elementProbabilityBox.setVisible (dataPaneOpen && mode == static_cast<int> (Disc::ElementMode::probability));
            resized();
        };
        styleScDurationBox (elementProbabilityBox);
        elementProbabilityBox.getProperties().set ("compactTransport", true);
        elementProbabilityBox.setInputRestrictions (4, "0123456789%");
        elementProbabilityBox.setTooltip ("Chance that each element fires");
        const auto commitElementProbability = [this]
        {
            const auto value = juce::jlimit (0.0, 100.0, elementProbabilityBox.getText().getDoubleValue());
            elementProbabilityBox.setText (juce::String (value, 0) + "%", false);
            canvas.setSelectedDiscElementProbability (value / 100.0);
        };
        elementProbabilityBox.onReturnKey = commitElementProbability;
        elementProbabilityBox.onFocusLost = commitElementProbability;
        configurePaneLabel (worldLabel, 13.0f, true);
        configurePaneLabel (worldInfoLabel, 12.0f, false);
        configurePaneButton (worldRemoveButton, "-");
        configurePaneButton (worldAddButton, "+");
        configurePaneButton (worldEnterButton, "enter");
        configurePaneLabel (scCodeLabel, 13.0f, true);
        configurePaneButton (scCodeOpenButton, "open");
        configurePaneButton (scCodeMinusButton, "-");
        configurePaneButton (scCodePlusButton, "+");
        configurePaneLabel (scCodeInfoLabel, 12.0f, false);
        configurePaneLabel (pdPatchLabel, 13.0f, true);
        configurePaneButton (pdPatchOpenButton, "open");
        configurePaneButton (pdPatchMinusButton, "-");
        configurePaneButton (pdPatchPlusButton, "+");
        configurePaneLabel (pdPatchInfoLabel, 12.0f, false);
        configurePaneLabel (scSheetLabel, 13.0f, true);
        configurePaneButton (scSheetOpenButton, "open");
        configurePaneButton (scSheetMinusButton, "-");
        configurePaneButton (scSheetPlusButton, "+");
        configurePaneLabel (scSheetInfoLabel, 12.0f, false);
        configurePaneLabel (orcaGridLabel, 13.0f, true);
        configurePaneButton (orcaGridOpenButton, "open");
        configurePaneButton (orcaGridMinusButton, "-");
        configurePaneButton (orcaGridPlusButton, "+");
        configurePaneLabel (orcaGridInfoLabel, 12.0f, false);
        configurePaneLabel (carouselLabel, 13.0f, true);
        configurePaneButton (carouselOpenButton, "open");
        configurePaneButton (carouselMinusButton, "-");
        configurePaneButton (carouselPlusButton, "+");
        configurePaneLabel (carouselInfoLabel, 12.0f, false);
        configurePaneLabel (pipeWorldLabel, 13.0f, true);
        configurePaneButton (pipeWorldOpenButton, "open");
        configurePaneButton (pipeWorldMinusButton, "-");
        configurePaneButton (pipeWorldPlusButton, "+");
        configurePaneLabel (pipeWorldInfoLabel, 12.0f, false);
        configurePaneButton (fireDiscButton, "fire");
        configurePaneButton (dataPaneCloseButton, "close");

        canvas.onRoutesChanged = [this]
        {
            if (! suppressProjectDirty)
            {
                projectDirty = true;
                updateProjectPresentation();
            }
            updateStatus();

            if (dataPaneOpen)
            {
                dataPaneOpen = canvas.getSelectedDiscInfo().valid;
                refreshDataPane();
                resized();
                repaint();
            }
        };
        canvas.onDiscPanelRequested = [this]
        {
            dataPaneOpen = canvas.getSelectedDiscInfo().valid;
            refreshDataPane();
            refreshLayersPanel();
            resized();
            repaint();
        };
        canvas.onSelectionChanged = [this]
        {
            refreshLayersPanel();

            if (dataPaneOpen)
            {
                dataPaneOpen = canvas.getSelectedDiscInfo().valid;
                refreshDataPane();
                resized();
            }

            updateStatus();
            repaint();
        };
        canvas.onDiscFlowTriggered = [this] (int discIndex)
        {
            scheduleDiscTrigger (discIndex);
        };
        canvas.onDiscPlaybackActive = [this] (const juce::String& key)
        {
            const auto now = juce::Time::getMillisecondCounterHiRes();
            return std::any_of (activeDiscs.begin(), activeDiscs.end(), [&] (const auto& active)
            {
                return active.key == key && (! std::isfinite (active.untilMs) || active.untilMs > now);
            });
        };
        canvas.setFlowTiming (globalTempoBpm, beatsForGridChoice (gridUnitChoice));
        updateStatus();
        refreshDataPane();
        refreshLayersPanel();
        refreshToolVisibility();

        setWantsKeyboardFocus (true);
        setSize (1040, 720);
        setAudioChannels (2, 2);

        for (const auto& device : juce::MidiInput::getAvailableDevices())
        {
            deviceManager.setMidiInputDeviceEnabled (device.identifier, true);
            deviceManager.addMidiInputDeviceCallback (device.identifier, this);
            midiInputIdentifiers.add (device.identifier);
        }

        for (const auto& device : juce::MidiOutput::getAvailableDevices())
            if (auto output = juce::MidiOutput::openDevice (device.identifier))
                midiOutputs.push_back (std::move (output));

        scAudio.setPdMidiOutputCallback ([this] (int port, const juce::MidiMessage& message)
        {
            if (juce::isPositiveAndBelow (port, static_cast<int> (midiOutputs.size())))
                midiOutputs[static_cast<size_t> (port)]->sendMessageNow (message);
        });

        startTimerHz (10);

#if JUCE_MAC
        applicationMenu.addItem (menuAbout, "About Blendings");
        juce::MenuBarModel::setMacMainMenu (this, &applicationMenu);
#endif
        updateProjectPresentation();
    }

    ~MainComponent() override
    {
#if JUCE_MAC
        if (juce::MenuBarModel::getMacMainMenu() == this)
            juce::MenuBarModel::setMacMainMenu (nullptr);
#endif
        for (const auto& identifier : midiInputIdentifiers)
            deviceManager.removeMidiInputDeviceCallback (identifier, this);

        shutdownAudio();
        scAudio.setPdMidiOutputCallback ({});
        midiOutputs.clear();

        scCodeWindow = nullptr;
        pdPatchWindow = nullptr;
        scSheetWindow = nullptr;
        orcaGridWindow = nullptr;
        carouselWindow = nullptr;
        setLookAndFeel (nullptr);
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        scAudio.prepare (sampleRate, samplesPerBlockExpected, 2, 2);
        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (safeThis != nullptr)
                safeThis->updateStatus();
        });
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        if (bufferToFill.buffer == nullptr)
            return;

        if (bufferToFill.startSample == 0 && bufferToFill.numSamples == bufferToFill.buffer->getNumSamples())
        {
            scratchInput.makeCopyOf (*bufferToFill.buffer, true);
            scAudio.render (*bufferToFill.buffer, &scratchInput);
            bufferToFill.buffer->applyGain (masterGain.load());
            captureMasterLevels (*bufferToFill.buffer, 0, bufferToFill.numSamples);
            return;
        }

        scratchAudio.setSize (bufferToFill.buffer->getNumChannels(), bufferToFill.numSamples, false, false, true);
        scratchInput.setSize (bufferToFill.buffer->getNumChannels(), bufferToFill.numSamples, false, false, true);
        for (int channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
            scratchInput.copyFrom (channel, 0, *bufferToFill.buffer, channel, bufferToFill.startSample, bufferToFill.numSamples);
        scAudio.render (scratchAudio, &scratchInput);

        for (int channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
        {
            bufferToFill.buffer->clear (channel, bufferToFill.startSample, bufferToFill.numSamples);

            if (channel < scratchAudio.getNumChannels())
                bufferToFill.buffer->copyFrom (channel, bufferToFill.startSample, scratchAudio, channel, 0, bufferToFill.numSamples);
        }
        for (int channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
            bufferToFill.buffer->applyGain (channel, bufferToFill.startSample, bufferToFill.numSamples, masterGain.load());
        captureMasterLevels (*bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples);
    }

    void releaseResources() override
    {
        scAudio.release();
        scratchAudio.setSize (0, 0);
        scratchInput.setSize (0, 0);
    }

    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override
    {
        const auto port = source != nullptr ? midiInputIdentifiers.indexOf (source->getIdentifier()) : 0;
        scAudio.sendPdMidiMessage (message, juce::jmax (0, port));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (appBackground());

        const auto toolbar = getLocalBounds().removeFromTop (toolbarHeight).toFloat();
        g.setColour (surfaceColour());
        g.fillRect (toolbar);

        g.setColour (subtleStroke().withAlpha (0.72f));
        g.drawHorizontalLine (toolbarHeight - 1, 0.0f, static_cast<float> (getWidth()));

        if (! transportBarBounds.isEmpty())
        {
            const auto transport = transportBarBounds.toFloat();
            g.setColour (appBackground().withAlpha (0.72f));
            g.fillRoundedRectangle (transport, 7.0f);
            g.setColour (subtleStroke().withAlpha (0.68f));
            g.drawRoundedRectangle (transport.reduced (0.5f), 7.0f, 1.0f);
            if (transportSeparatorX > 0)
                g.drawVerticalLine (transportSeparatorX, transport.getY() + 7.0f, transport.getBottom() - 7.0f);
        }

        if (dataPaneOpen)
        {
            const auto pane = getDataPaneBounds().toFloat();
            g.setColour (surfaceColour());
            g.fillRect (pane);
            g.setColour (subtleStroke().withAlpha (0.72f));
            g.drawVerticalLine (static_cast<int> (pane.getX()), pane.getY(), pane.getBottom());

            const auto info = canvas.getSelectedDiscInfo();
            paintPaneRow (g, nestedWorldRowBounds, worldElementColour(), info.valid && info.nestedWorldCount > 0);
            paintPaneRow (g, scCodeRowBounds, scCodeElementColour(), info.valid && info.scCodeCount > 0);
            paintPaneRow (g, pdPatchRowBounds, pdPatchElementColour(), info.valid && info.pdPatchCount > 0);
            paintPaneRow (g, scSheetRowBounds, scSheetElementColour(), info.valid && info.scSheetCount > 0);
            paintPaneRow (g, orcaGridRowBounds, orcaGridElementColour(), info.valid && info.orcaGridCount > 0);
            paintPaneRow (g, carouselRowBounds, carouselElementColour(), info.valid && info.carouselCount > 0);
            paintPaneRow (g, pipeWorldRowBounds, pipeElementColour(), info.valid && info.pipeWorldCount > 0);
        }

    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto toolbar = bounds.removeFromTop (toolbarHeight).reduced (16, 9);

        const auto buttonSize = 34;
        constexpr int transportWidth = 700;
        transportBarBounds = { (getWidth() - transportWidth) / 2, toolbar.getY(), transportWidth, toolbar.getHeight() };
        auto transport = transportBarBounds.reduced (5, 2);
        flowButton.setBounds (transport.removeFromLeft (52));
        transport.removeFromLeft (4);
        pauseButton.setBounds (transport.removeFromLeft (58));
        transport.removeFromLeft (4);
        stopButton.setBounds (transport.removeFromLeft (52));
        transport.removeFromLeft (5);
        elapsedTimeLabel.setBounds (transport.removeFromLeft (60));
        transport.removeFromLeft (12);
        transportSeparatorX = transport.getX() - 6;
        tempoEditor.setBounds (transport.removeFromLeft (54));
        transport.removeFromLeft (5);
        tempoLabel.setBounds (transport.removeFromLeft (32));
        transport.removeFromLeft (12);
        gridUnitBox.setBounds (transport.removeFromLeft (116));
        transport.removeFromLeft (10);
        triggerQuantizeSlider.setBounds (transport.removeFromLeft (124));
        transport.removeFromLeft (8);
        clockButton.setBounds (transport.removeFromLeft (72));
        statusLabel.setBounds (toolbar.removeFromRight (120));
        masterMeter.setBounds (toolbar.removeFromRight (48).reduced (3, 13));
        auto dataPane = getDataPaneBounds();

        if (dataPaneOpen)
        {
            bounds.removeFromRight (dataPane.getWidth());
            dataPane = getDataPaneBounds().reduced (18, 18);
            auto bottomControls = dataPane.removeFromBottom (78);

            auto titleRow = dataPane.removeFromTop (30);
            dataPaneTitle.setBounds (titleRow.removeFromLeft (74));
            triggerModeLabel.setBounds (titleRow.removeFromLeft (48));
            triggerModeBox.setBounds (titleRow);
            dataPane.removeFromTop (4);
            dataPaneSummary.setBounds (dataPane.removeFromTop (22));
            dataPane.removeFromTop (5);
            holdDropsToggle.setBounds (dataPane.removeFromTop (28));
            dataPane.removeFromTop (5);
            auto timelineRow = dataPane.removeFromTop (30);
            elementModeLabel.setBounds (timelineRow.removeFromLeft (58));
            if (elementProbabilityBox.isVisible())
            {
                elementProbabilityBox.setBounds (timelineRow.removeFromRight (66));
                timelineRow.removeFromRight (7);
            }
            elementModeBox.setBounds (timelineRow);
            dataPane.removeFromTop (7);
            auto mixRow = dataPane.removeFromTop (28);
            discLevelSlider.setBounds (mixRow.removeFromLeft (juce::jmax (130, mixRow.getWidth() / 2)));
            mixRow.removeFromLeft (7); discPanSlider.setBounds (mixRow.removeFromLeft (juce::jmax (110, mixRow.getWidth() - 70)));
            mixRow.removeFromLeft (7); discMuteButton.setBounds (mixRow.removeFromLeft (28)); mixRow.removeFromLeft (4); discSoloButton.setBounds (mixRow.removeFromLeft (28));
            dataPane.removeFromTop (9);

            const auto dotColumns = juce::jmax (1, dataPane.getWidth() / 35);
            const auto dotRows = elementDots.empty() ? 1
                                                     : juce::jlimit (1, 3, (static_cast<int> (elementDots.size()) + dotColumns - 1) / dotColumns);
            auto dotRow = dataPane.removeFromTop (dotRows * 29);

            for (int i = 0; i < static_cast<int> (elementDots.size()); ++i)
            {
                const auto column = i % dotColumns;
                const auto rowIndex = i / dotColumns;
                elementDots[static_cast<size_t> (i)]->setBounds (dotRow.getX() + column * 35,
                                                                 dotRow.getY() + rowIndex * 29,
                                                                 29,
                                                                 29);
            }
            dataPane.removeFromTop (14);

            nestedWorldRowBounds = dataPane.removeFromTop (56);
            layoutPaneRow (nestedWorldRowBounds, worldLabel, worldInfoLabel, &worldEnterButton, worldRemoveButton, worldAddButton);
            dataPane.removeFromTop (8);

            scCodeRowBounds = dataPane.removeFromTop (56);
            layoutPaneRow (scCodeRowBounds, scCodeLabel, scCodeInfoLabel, &scCodeOpenButton, scCodeMinusButton, scCodePlusButton);
            dataPane.removeFromTop (8);

            pdPatchRowBounds = dataPane.removeFromTop (56);
            layoutPaneRow (pdPatchRowBounds, pdPatchLabel, pdPatchInfoLabel, &pdPatchOpenButton, pdPatchMinusButton, pdPatchPlusButton);
            dataPane.removeFromTop (8);

            scSheetRowBounds = dataPane.removeFromTop (56);
            layoutPaneRow (scSheetRowBounds, scSheetLabel, scSheetInfoLabel, &scSheetOpenButton, scSheetMinusButton, scSheetPlusButton);
            dataPane.removeFromTop (8);

            orcaGridRowBounds = dataPane.removeFromTop (56);
            layoutPaneRow (orcaGridRowBounds, orcaGridLabel, orcaGridInfoLabel, &orcaGridOpenButton, orcaGridMinusButton, orcaGridPlusButton);
            dataPane.removeFromTop (8);
            carouselRowBounds = dataPane.removeFromTop (56);
            layoutPaneRow (carouselRowBounds, carouselLabel, carouselInfoLabel, &carouselOpenButton, carouselMinusButton, carouselPlusButton);
            dataPane.removeFromTop (8);
            pipeWorldRowBounds = dataPane.removeFromTop (56);
            layoutPaneRow (pipeWorldRowBounds, pipeWorldLabel, pipeWorldInfoLabel, &pipeWorldOpenButton, pipeWorldMinusButton, pipeWorldPlusButton);

            fireDiscButton.setBounds (bottomControls.removeFromTop (32));
            bottomControls.removeFromTop (10);
            dataPaneCloseButton.setBounds (bottomControls.removeFromTop (32));
        }

        constexpr int toolPanelWidth = 58;
        constexpr int toolPanelPadding = 8;
        constexpr int toolGap = 5;
        const auto modulationLayer = canvas.isModulationLayerVisible();
        const auto toolCount = modulationLayer ? 4 : 15;
        constexpr int snapSectionGap = 13;
        const auto toolPanelHeight = toolPanelPadding * 2 + buttonSize * (toolCount + 1)
                                   + toolGap * (toolCount - 1) + snapSectionGap;
        leftToolPanelBounds = { bounds.getX() + 14, bounds.getY() + 14, toolPanelWidth, toolPanelHeight };
        leftToolPanelBackground.setBounds (leftToolPanelBounds);
        auto toolSlot = leftToolPanelBounds.reduced ((toolPanelWidth - buttonSize) / 2, toolPanelPadding);
        const auto placeTool = [&toolSlot] (juce::Component& component)
        {
            component.setBounds (toolSlot.removeFromTop (buttonSize));
            toolSlot.removeFromTop (toolGap);
        };
        placeTool (selectButton);
        if (modulationLayer)
        {
            placeTool (modulatorButton);
            placeTool (modConnectButton);
            placeTool (eraseButton);
        }
        else
        {
            placeTool (drawButton);
            placeTool (warpPipeButton);
            placeTool (editButton);
            placeTool (discButton);
            placeTool (tapButton);
            placeTool (drainButton);
            placeTool (cloneButton);
            placeTool (speedLimitButton);
            placeTool (waitButton);
            placeTool (strikeButton);
            placeTool (teleportButton);
            placeTool (filterButton);
            placeTool (logicButton);
            placeTool (eraseButton);
        }
        toolSlot.removeFromTop (snapSectionGap - toolGap);
        snapToggle.setBounds (leftToolPanelBounds.getX() + 4, toolSlot.getY(), toolPanelWidth - 8, buttonSize);
        leftToolPanelBackground.setDividerY (snapToggle.getY() - leftToolPanelBounds.getY() - 7);

        const auto hasLayerActions = layersRootButton.isVisible() || layersUpButton.isVisible()
                                  || layersEnterButton.isVisible();
        const auto layersPanelHeight = hasLayerActions ? 138 : 100;
        layersPanelBounds = { bounds.getRight() - 14 - 248, bounds.getY() + 14, 248, layersPanelHeight };
        layersPanelBackground.setBounds (layersPanelBounds);
        auto layerPanelInner = layersPanelBounds.reduced (12, 10);
        layersTitleLabel.setBounds (layerPanelInner.removeFromTop (16));
        layersPathLabel.setBounds (layerPanelInner.removeFromTop (17));
        layerPanelInner.removeFromTop (5);
        auto layerSwitch = layerPanelInner.removeFromTop (32);
        const auto switchGap = 5;
        const auto switchWidth = (layerSwitch.getWidth() - switchGap) / 2;
        layersMainButton.setBounds (layerSwitch.removeFromLeft (switchWidth));
        layerSwitch.removeFromLeft (switchGap);
        layersModulationButton.setBounds (layerSwitch);
        layersPanelBackground.setDividerY (hasLayerActions ? 82 : -1);
        if (hasLayerActions)
        {
            layerPanelInner.removeFromTop (13);
            auto layerButtons = layerPanelInner.removeFromTop (28);
            if (layersRootButton.isVisible())
            {
                layersRootButton.setBounds (layerButtons.removeFromLeft (56));
                layerButtons.removeFromLeft (6);
            }
            if (layersUpButton.isVisible())
            {
                layersUpButton.setBounds (layerButtons.removeFromLeft (56));
                layerButtons.removeFromLeft (6);
            }
            if (layersEnterButton.isVisible())
                layersEnterButton.setBounds (layerButtons);
        }

        setPaneComponentsVisible (dataPaneOpen);
        canvas.setBounds (bounds);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        const auto command = key.getModifiers().isCommandDown();
        if (command && key.getKeyCode() == 'N') { newProject(); return true; }
        if (command && key.getKeyCode() == 'O') { openProject(); return true; }
        if (command && key.getKeyCode() == 'S') { key.getModifiers().isShiftDown() ? saveProjectAs() : saveProject(); return true; }
        if (command && key.getKeyCode() == 'W') { closeApplicationWindow(); return true; }
        if (command && key.getKeyCode() == 'Z') { canvas.undo(); updateStatus(); return true; }
        if (key.getKeyCode() == 'B' && ! command) { if (canvas.toggleSelectedDeviceBypass()) updateStatus(); return true; }
        if (key == juce::KeyPress::spaceKey) { setTransportRunning (! transportRunning); return true; }
        if (key == juce::KeyPress::backspaceKey || key == juce::KeyPress::deleteKey)
        {
            if (deleteSelectedElementDot())
                return true;
        }

        return juce::AudioAppComponent::keyPressed (key);
    }

    juce::StringArray getMenuBarNames() override { return { "File", "Edit", "View" }; }

    juce::PopupMenu getMenuForIndex (int index, const juce::String&) override
    {
        juce::PopupMenu menu;
        if (index == 0)
        {
            menu.addItem (menuNewProject, "New Project\tCmd+N");
            menu.addItem (menuOpenProject, "Open...\tCmd+O");
            menu.addSeparator();
            menu.addItem (menuSaveProject, "Save\tCmd+S");
            menu.addItem (menuSaveProjectAs, "Save As...\tShift+Cmd+S");
            menu.addSeparator();
            menu.addItem (menuClose, "Close\tCmd+W");
        }
        else if (index == 1)
        {
            menu.addItem (menuUndo, "Undo\tCmd+Z");
            menu.addSeparator();
            menu.addItem (menuCopy, "Copy\tCmd+C");
            menu.addItem (menuPaste, "Paste\tCmd+V");
            menu.addItem (menuDuplicate, "Duplicate\tCmd+D");
            menu.addSeparator();
            menu.addItem (menuToggleBypass, "Toggle Device Bypass\tB");
            menu.addSeparator();
            menu.addItem (menuClear, "Clear");
        }
        else if (index == 2)
        {
            menu.addItem (menuMixer, "Mixer\tCmd+M");
            menu.addItem (menuClocks, "Multiple Clocks...");
            menu.addSeparator();
            menu.addItem (menuDimOrbitElements, "Dim Orbit Elements", true, orbitElementsDimmed);
            menu.addItem (menuCompactDiscs, "Compact Discs", true, compactDiscs);
            menu.addSeparator();
            menu.addItem (menuFlowDebug, "Flow Debug Overlay", true, flowDebugVisible);
        }
        return menu;
    }

    void menuItemSelected (int itemId, int) override
    {
        if (itemId == menuNewProject) newProject();
        else if (itemId == menuOpenProject) openProject();
        else if (itemId == menuSaveProject) saveProject();
        else if (itemId == menuSaveProjectAs) saveProjectAs();
        else if (itemId == menuClose) closeApplicationWindow();
        else if (itemId == menuUndo) { canvas.undo(); updateStatus(); }
        else if (itemId == menuCopy) canvas.copySelectedItems();
        else if (itemId == menuPaste) canvas.pasteSelectedItems();
        else if (itemId == menuDuplicate) canvas.duplicateSelectedItems();
        else if (itemId == menuToggleBypass) { canvas.toggleSelectedDeviceBypass(); updateStatus(); }
        else if (itemId == menuClear) { canvas.clear(); updateStatus(); }
        else if (itemId == menuMixer) openMixerWindow();
        else if (itemId == menuClocks) openClockSettings();
        else if (itemId == menuDimOrbitElements)
        {
            orbitElementsDimmed = ! orbitElementsDimmed;
            canvas.setOrbitElementsDimmed (orbitElementsDimmed);
            menuItemsChanged();
        }
        else if (itemId == menuCompactDiscs)
        {
            compactDiscs = ! compactDiscs;
            canvas.setCompactDiscs (compactDiscs);
            menuItemsChanged();
        }
        else if (itemId == menuFlowDebug)
        {
            flowDebugVisible = ! flowDebugVisible;
            canvas.setFlowDebugVisible (flowDebugVisible);
            menuItemsChanged();
        }
        else if (itemId == menuAbout)
        {
            juce::NativeMessageBox::showAsync (
                juce::MessageBoxOptions()
                    .withIconType (juce::MessageBoxIconType::InfoIcon)
                    .withTitle ("About Blendings")
                    .withMessage ("Blendings\n\nCreated by matd.space\n\nVersion "
                                  + juce::JUCEApplication::getInstance()->getApplicationVersion())
                    .withButton ("OK"),
                nullptr);
        }
    }

    void requestApplicationClose (std::function<void()> closeAction)
    {
        confirmDiscardChanges (std::move (closeAction));
    }

private:
    void closeApplicationWindow()
    {
        requestApplicationClose ([]
        {
            if (auto* application = juce::JUCEApplication::getInstance())
                application->quit();
        });
    }

    enum { menuNewProject = 10001, menuOpenProject, menuSaveProject, menuSaveProjectAs, menuClose,
           menuUndo, menuCopy, menuPaste, menuDuplicate, menuToggleBypass, menuClear, menuMixer, menuClocks, menuDimOrbitElements, menuCompactDiscs, menuFlowDebug };
    static constexpr int menuAbout = 10100;
    juce::PopupMenu applicationMenu;
    MinimalLookAndFeel minimalLookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 500 };
    RoadCanvas canvas;
    LayersPanelBackground leftToolPanelBackground { false };
    LayersPanelBackground layersPanelBackground;
    IconButton selectButton;
    IconButton drawButton;
    IconButton warpPipeButton;
    IconButton editButton;
    IconButton discButton;
    IconButton addElementButton;
    IconButton eraseButton;
    IconButton tapButton;
    IconButton drainButton;
    IconButton cloneButton;
    IconButton speedLimitButton;
    IconButton waitButton;
    IconButton strikeButton;
    IconButton teleportButton;
    IconButton filterButton;
    IconButton logicButton;
    IconButton modulatorButton;
    IconButton modConnectButton;
    juce::TextButton flowButton;
    juce::TextButton pauseButton;
    juce::TextButton stopButton;
    juce::Label elapsedTimeLabel;
    juce::ToggleButton snapToggle;
    juce::Label tempoLabel;
    juce::TextEditor tempoEditor;
    juce::Label gridUnitLabel;
    juce::ComboBox gridUnitBox;
    juce::Slider triggerQuantizeSlider;
    juce::TextButton clockButton;
    juce::Label statusLabel;
    StereoMeter masterMeter;
    juce::Label layersTitleLabel;
    juce::Label layersPathLabel;
    juce::TextButton layersMainButton;
    juce::TextButton layersRootButton;
    juce::TextButton layersUpButton;
    juce::TextButton layersEnterButton;
    juce::TextButton layersModulationButton;
    juce::Label dataPaneTitle;
    juce::Label dataPaneSummary;
    juce::Label triggerModeLabel;
    juce::ComboBox triggerModeBox;
    juce::ToggleButton holdDropsToggle;
    juce::Slider discLevelSlider, discPanSlider;
    juce::TextButton discMuteButton, discSoloButton;
    juce::Label elementModeLabel;
    juce::ComboBox elementModeBox;
    juce::TextEditor elementProbabilityBox;
    ElementDotButton nestedWorldDot;
    ElementDotButton scCodeDot;
    ElementDotButton pdPatchDot;
    ElementDotButton scSheetDot;
    ElementDotButton orcaGridDot;
    ElementDotButton carouselDot;
    ElementDotButton pipeWorldDot;
    juce::Label soundLabel;
    juce::TextButton soundMinusButton;
    juce::TextButton soundPlusButton;
    juce::Label worldLabel;
    juce::Label worldInfoLabel;
    juce::TextButton worldRemoveButton;
    juce::TextButton worldAddButton;
    juce::TextButton worldEnterButton;
    juce::Label scCodeLabel;
    juce::TextButton scCodeOpenButton;
    juce::TextButton scCodeMinusButton;
    juce::TextButton scCodePlusButton;
    juce::Label scCodeInfoLabel;
    juce::Label pdPatchLabel;
    juce::TextButton pdPatchOpenButton;
    juce::TextButton pdPatchMinusButton;
    juce::TextButton pdPatchPlusButton;
    juce::Label pdPatchInfoLabel;
    juce::Label scSheetLabel;
    juce::TextButton scSheetOpenButton;
    juce::TextButton scSheetMinusButton;
    juce::TextButton scSheetPlusButton;
    juce::Label scSheetInfoLabel;
    juce::Label orcaGridLabel;
    juce::TextButton orcaGridOpenButton;
    juce::TextButton orcaGridMinusButton;
    juce::TextButton orcaGridPlusButton;
    juce::Label orcaGridInfoLabel;
    juce::Label carouselLabel;
    juce::TextButton carouselOpenButton;
    juce::TextButton carouselMinusButton;
    juce::TextButton carouselPlusButton;
    juce::Label carouselInfoLabel;
    juce::Label pipeWorldLabel;
    juce::TextButton pipeWorldOpenButton;
    juce::TextButton pipeWorldMinusButton;
    juce::TextButton pipeWorldPlusButton;
    juce::Label pipeWorldInfoLabel;
    juce::TextButton fireDiscButton;
    juce::TextButton dataPaneCloseButton;
    bool dataPaneOpen = false;
    ScDiscAudioEngine scAudio;
    juce::AudioBuffer<float> scratchAudio;
    juce::AudioBuffer<float> scratchInput;
    std::atomic<float> masterLevelLeft { 0.0f }, masterLevelRight { 0.0f };
    std::atomic<float> masterGain { 1.0f };
    juce::StringArray midiInputIdentifiers;
    std::vector<std::unique_ptr<juce::MidiOutput>> midiOutputs;
    std::unique_ptr<FloatingEditorWindow> scCodeWindow;
    std::unique_ptr<FloatingEditorWindow> pdPatchWindow;
    std::unique_ptr<FloatingEditorWindow> scSheetWindow;
    std::unique_ptr<FloatingEditorWindow> orcaGridWindow;
    std::unique_ptr<FloatingEditorWindow> carouselWindow;
    std::unique_ptr<FloatingEditorWindow> pipeElementWindow;
    std::unique_ptr<FloatingEditorWindow> mixerWindow;
    juce::Component* pipeElementComponent = nullptr;
    RoadCanvas::DiscHandle pipeElementHandle;
    int pipeElementIndex = -1;
    CarouselEditorPanel* carouselPanel = nullptr;
    std::vector<std::unique_ptr<CarouselEditorComponent>> carouselRuntimes;
    std::vector<std::unique_ptr<juce::Component>> pipeRuntimes;
    struct ActiveDisc { juce::String key; double untilMs = 0.0; };
    std::vector<ActiveDisc> activeDiscs;
    struct DiscSequenceState { juce::String key; int nextElement = 0; };
    std::vector<DiscSequenceState> discSequenceStates;
    struct PendingElementChain
    {
        juce::String key;
        std::vector<DiscAudioTrigger> triggers;
        std::vector<CarouselDocument> carousels;
        std::vector<juce::String> pipeStates;
        int nextElement = 0;
        double nextTimeMs = 0.0;
    };
    std::vector<PendingElementChain> pendingElementChains;
    std::unique_ptr<juce::FileChooser> projectFileChooser;
    juce::ScopedMessageBox projectPrompt;
    juce::File currentProjectFile;
    bool projectDirty = false;
    bool suppressProjectDirty = false;
    double globalTempoBpm = 120.0;
    int gridUnitChoice = 5;
    struct ElementSelection
    {
        ElementDotButton::Kind kind;
        int index = 0;
    };

    std::optional<ElementSelection> selectedElement;
    std::vector<std::unique_ptr<ElementDotButton>> elementDots;
    struct ElementDotSignature
    {
        bool visible = false;
        int nestedWorlds = 0;
        int scCode = 0;
        int pdPatches = 0;
        int scSheets = 0;
        int orcaGrids = 0;
        int carousels = 0;
        int pipeWorlds = 0;

        bool operator== (const ElementDotSignature& other) const noexcept
        {
            return visible == other.visible
                && nestedWorlds == other.nestedWorlds
                && scCode == other.scCode
                && pdPatches == other.pdPatches
                && scSheets == other.scSheets
                && orcaGrids == other.orcaGrids
                && carousels == other.carousels
                && pipeWorlds == other.pipeWorlds;
        }
    };

    ElementDotSignature elementDotSignature;
    juce::Rectangle<int> nestedWorldRowBounds;
    juce::Rectangle<int> scCodeRowBounds;
    juce::Rectangle<int> pdPatchRowBounds;
    juce::Rectangle<int> scSheetRowBounds;
    juce::Rectangle<int> orcaGridRowBounds;
    juce::Rectangle<int> carouselRowBounds;
    juce::Rectangle<int> pipeWorldRowBounds;
    juce::Rectangle<int> layersPanelBounds;
    juce::Rectangle<int> leftToolPanelBounds;
    juce::Rectangle<int> transportBarBounds;
    int transportSeparatorX = 0;
    bool transportRunning = false;
    bool orbitElementsDimmed = false;
    bool compactDiscs = false;
    bool flowDebugVisible = false;
    int triggerQuantizeChoice = 0;
    double transportElapsedSeconds = 0.0;
    double lastTransportTimeMs = 0.0;
    struct PendingDiscTrigger { int discIndex = -1; double dueBeat = 0.0; };
    std::vector<PendingDiscTrigger> pendingDiscTriggers;

    void setTool (RoadCanvas::Tool tool)
    {
        canvas.setTool (tool);
    }

    void refreshToolVisibility()
    {
        const auto modulation = canvas.isModulationLayerVisible();
        for (auto* component : std::array<juce::Component*, 13> {
                 &drawButton, &warpPipeButton, &editButton, &discButton, &tapButton, &drainButton, &cloneButton,
                 &speedLimitButton, &waitButton, &strikeButton, &teleportButton, &filterButton, &logicButton })
            component->setVisible (! modulation);
        modulatorButton.setVisible (modulation);
        modConnectButton.setVisible (modulation);
        eraseButton.setVisible (true);
        selectButton.setVisible (true);
    }

    void toggleModulationLayer()
    {
        if (canvas.isModulationLayerVisible()) canvas.exitModulationLayer();
        else canvas.enterModulationLayer();
        dataPaneOpen = false;
        selectedElement = {};
        selectButton.setToggleState (true, juce::dontSendNotification);
        refreshDataPane();
        refreshToolVisibility();
        refreshLayersPanel();
        updateStatus();
        resized();
        repaint();
    }

    void setTransportRunning (bool shouldRun)
    {
        if (transportRunning == shouldRun)
            return;

        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (transportRunning && lastTransportTimeMs > 0.0)
            transportElapsedSeconds += juce::jmax (0.0, (now - lastTransportTimeMs) / 1000.0);

        transportRunning = shouldRun;
        lastTransportTimeMs = now;
        canvas.setFlowRunning (transportRunning);
        flowButton.setToggleState (transportRunning, juce::dontSendNotification);
        pauseButton.setToggleState (! transportRunning && transportElapsedSeconds > 0.0, juce::dontSendNotification);
        updateElapsedTimeLabel();
    }

    double currentTransportBeat() const
    {
        auto elapsed = transportElapsedSeconds;
        if (transportRunning && lastTransportTimeMs > 0.0)
            elapsed += juce::jmax (0.0, (juce::Time::getMillisecondCounterHiRes() - lastTransportTimeMs) / 1000.0);
        return elapsed * globalTempoBpm / 60.0;
    }

    void fireFlowDiscNow (int discIndex)
    {
        canvas.flashDiscForFlow (discIndex);
        if (canvas.selectDiscForFlow (discIndex)) fireSelectedDisc();
    }

    void scheduleDiscTrigger (int discIndex)
    {
        static constexpr std::array<double, 6> quantizeBeats { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0 };
        const auto interval = quantizeBeats[static_cast<size_t> (juce::jlimit (0, 5, triggerQuantizeChoice))];
        if (interval <= 0.0) { fireFlowDiscNow (discIndex); return; }
        const auto beat = currentTransportBeat();
        const auto boundary = std::ceil ((beat - 0.0001) / interval) * interval;
        if (boundary <= beat + 0.0001) { fireFlowDiscNow (discIndex); return; }
        pendingDiscTriggers.push_back ({ discIndex, boundary });
    }

    void resetTransport()
    {
        transportRunning = false;
        transportElapsedSeconds = 0.0;
        lastTransportTimeMs = juce::Time::getMillisecondCounterHiRes();
        canvas.resetFlowToStart();
        flowButton.setToggleState (false, juce::dontSendNotification);
        pauseButton.setToggleState (false, juce::dontSendNotification);
        stopButton.setToggleState (false, juce::dontSendNotification);
        discSequenceStates.clear();
        pendingElementChains.clear();
        pendingDiscTriggers.clear();
        stopPreviewAudio();
        updateElapsedTimeLabel();
    }

    void timerCallback() override
    {
        updateElapsedTimeLabel();
        masterMeter.setLevels (masterLevelLeft.load(), masterLevelRight.load());
        masterLevelLeft.store (masterLevelLeft.load() * 0.82f); masterLevelRight.store (masterLevelRight.load() * 0.82f);
        const auto now = juce::Time::getMillisecondCounterHiRes();
        const auto beat = currentTransportBeat();
        for (auto& pending : pendingDiscTriggers)
            if (pending.discIndex >= 0 && pending.dueBeat <= beat + 0.0001)
            { fireFlowDiscNow (pending.discIndex); pending.discIndex = -1; }
        pendingDiscTriggers.erase (std::remove_if (pendingDiscTriggers.begin(), pendingDiscTriggers.end(), [] (const auto& pending) { return pending.discIndex < 0; }), pendingDiscTriggers.end());
        for (auto& chain : pendingElementChains)
        {
            if (chain.nextElement < 0 || chain.nextTimeMs > now) continue;
            const auto duration = dispatchChainElement (chain);
            if (! std::isfinite (duration) || chain.nextElement >= static_cast<int> (chain.triggers.size() + chain.carousels.size() + chain.pipeStates.size()))
                chain.nextElement = -1;
            else
                chain.nextTimeMs = now + duration * 1000.0;
        }
        pendingElementChains.erase (std::remove_if (pendingElementChains.begin(), pendingElementChains.end(), [] (const auto& chain)
        {
            return chain.nextElement < 0;
        }), pendingElementChains.end());
    }

    void captureMasterLevels (const juce::AudioBuffer<float>& buffer, int start, int count)
    {
        if (buffer.getNumChannels() <= 0 || count <= 0) return;
        masterLevelLeft.store (juce::jmax (masterLevelLeft.load(), buffer.getMagnitude (0, start, count)));
        const auto rightChannel = juce::jmin (1, buffer.getNumChannels() - 1);
        masterLevelRight.store (juce::jmax (masterLevelRight.load(), buffer.getMagnitude (rightChannel, start, count)));
    }

    static double triggerDurationSeconds (const DiscAudioTrigger& trigger)
    {
        auto duration = 0.45;
        if (trigger.hasScCode())
        {
            if (trigger.scDurationSeconds < 0.0f) return std::numeric_limits<double>::infinity();
            duration = juce::jmax (duration, static_cast<double> (trigger.scDurationSeconds));
        }
        if (trigger.hasPdPatch())
        {
            if (trigger.pdDurationSeconds < 0.0f) return std::numeric_limits<double>::infinity();
            duration = juce::jmax (duration, static_cast<double> (trigger.pdDurationSeconds));
        }
        if (trigger.hasScSheetScore() || trigger.hasOrcaGridScore()) duration = juce::jmax (duration, 1.0);
        return duration;
    }

    double dispatchChainElement (PendingElementChain& chain)
    {
        auto index = chain.nextElement++;
        if (index < static_cast<int> (chain.triggers.size()))
        {
            scAudio.trigger (chain.triggers[static_cast<size_t> (index)]);
            return triggerDurationSeconds (chain.triggers[static_cast<size_t> (index)]);
        }
        index -= static_cast<int> (chain.triggers.size());
        if (index < static_cast<int> (chain.carousels.size()))
        {
            carouselRuntimes.clear();
            auto runtime = std::make_unique<CarouselEditorComponent>();
            runtime->setDocument (chain.carousels[static_cast<size_t> (index)]);
            runtime->onNote = [this] (int midi) { scAudio.triggerMidiNote (midi); };
            runtime->setRunning (true);
            carouselRuntimes.push_back (std::move (runtime));
            return std::numeric_limits<double>::infinity();
        }
        index -= static_cast<int> (chain.carousels.size());
        if (index < static_cast<int> (chain.pipeStates.size()))
        {
            pipeRuntimes.clear();
            auto runtime = otherware::createPipeWorkspaceComponent();
            otherware::setPipeWorkspaceTempo (*runtime, globalTempoBpm);
            const auto& stateText = chain.pipeStates[static_cast<size_t> (index)];
            if (stateText.isNotEmpty()) otherware::applyPipeWorkspaceState (*runtime, juce::JSON::parse (stateText));
            otherware::setPipeWorkspaceRunning (*runtime, true);
            pipeRuntimes.push_back (std::move (runtime));
        }
        return std::numeric_limits<double>::infinity();
    }

    void updateElapsedTimeLabel()
    {
        auto elapsed = transportElapsedSeconds;
        if (transportRunning && lastTransportTimeMs > 0.0)
            elapsed += juce::jmax (0.0, (juce::Time::getMillisecondCounterHiRes() - lastTransportTimeMs) / 1000.0);

        const auto totalSeconds = juce::jmax (0, static_cast<int> (std::floor (elapsed)));
        const auto minutes = totalSeconds / 60;
        const auto seconds = totalSeconds % 60;
        elapsedTimeLabel.setText (juce::String (minutes).paddedLeft ('0', 2) + ":"
                                      + juce::String (seconds).paddedLeft ('0', 2),
                                  juce::dontSendNotification);
    }

    void stopPreviewAudio()
    {
        carouselRuntimes.clear();
        pipeRuntimes.clear();
        activeDiscs.clear();
        pendingElementChains.clear();
        scAudio.stopPreview();
        updateStatus();
    }

    juce::Rectangle<int> getDataPaneBounds() const
    {
        return getLocalBounds().withTrimmedTop (toolbarHeight).removeFromRight (dataPaneWidth);
    }

    static void configurePaneLabel (juce::Label& label, float height, bool bold)
    {
        label.setColour (juce::Label::textColourId, bold ? textPrimary() : textMuted());
        label.setJustificationType (juce::Justification::centredLeft);
        label.setFont (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    static void configurePaneButton (juce::TextButton& button, const juce::String& text)
    {
        button.setButtonText (text);
        button.setColour (juce::TextButton::buttonColourId, raisedSurface());
        button.setColour (juce::TextButton::buttonOnColourId, accentColour());
        button.setColour (juce::TextButton::textColourOffId, textPrimary());
        button.setColour (juce::TextButton::textColourOnId, appBackground());
    }

    static void paintPaneRow (juce::Graphics& g, juce::Rectangle<int> bounds, juce::Colour accent, bool active)
    {
        if (bounds.isEmpty())
            return;

        const auto area = bounds.toFloat().reduced (0.0f, 1.0f);
        g.setColour (raisedSurface().withAlpha (active ? 0.55f : 0.32f));
        g.fillRoundedRectangle (area, 7.0f);

        g.setColour (subtleStroke().withAlpha (active ? 0.58f : 0.34f));
        g.drawRoundedRectangle (area.reduced (0.5f), 7.0f, 1.0f);

        g.setColour (accent.withAlpha (active ? 0.90f : 0.28f));
        g.fillRoundedRectangle (area.withWidth (3.0f).reduced (0.0f, 8.0f), 1.5f);
    }

    static void layoutPaneRow (juce::Rectangle<int> bounds,
                               juce::Label& title,
                               juce::Label& info,
                               juce::TextButton* primaryButton,
                               juce::TextButton& minusButton,
                               juce::TextButton& plusButton)
    {
        auto inner = bounds.reduced (12, 8);
        auto buttons = inner.removeFromRight (primaryButton != nullptr ? 146 : 82).withSizeKeepingCentre (primaryButton != nullptr ? 146 : 82, 34);

        plusButton.setBounds (buttons.removeFromRight (34));
        buttons.removeFromRight (8);
        minusButton.setBounds (buttons.removeFromRight (34));

        if (primaryButton != nullptr)
        {
            buttons.removeFromRight (8);
            primaryButton->setBounds (buttons.removeFromRight (62));
        }

        inner.removeFromRight (10);
        title.setBounds (inner.removeFromTop (20));
        info.setBounds (inner.removeFromTop (18));
    }

    void refreshLayersPanel()
    {
        const auto modulation = canvas.isModulationLayerVisible();
        const auto nested = ! modulation && canvas.getWorldDepth() > 0;
        const auto canEnter = canvas.canEnterSelectedDiscWorld();
        layersTitleLabel.setText ("LAYERS", juce::dontSendNotification);
        layersPathLabel.setText ((modulation ? "Modulation layer  /  " : "Main layer  /  ")
                                   + (modulation ? juce::String ("Root") : canvas.getLayerPathText()),
                                 juce::dontSendNotification);
        const auto visibilityChanged = layersRootButton.isVisible() != nested
                                    || layersUpButton.isVisible() != nested
                                    || layersEnterButton.isVisible() != canEnter;
        layersRootButton.setVisible (nested);
        layersUpButton.setVisible (nested);
        layersEnterButton.setVisible (canEnter);
        layersMainButton.setToggleState (! modulation, juce::dontSendNotification);
        layersModulationButton.setToggleState (modulation, juce::dontSendNotification);
        layersMainButton.setEnabled (true);
        layersModulationButton.setEnabled (! nested);
        layersModulationButton.setTooltip (nested ? "Return to the root world to edit modulation"
                                                   : "Edit modulation above the main layer");
        layersPanelBackground.setAccent (modulation ? juce::Colour (0xffa96de8) : accentColour());
        if (visibilityChanged)
            resized();
    }

    void exitToRootWorld()
    {
        if (canvas.exitToRootWorld())
        {
            dataPaneOpen = false;
            selectedElement = {};
            refreshDataPane();
            refreshLayersPanel();
            updateStatus();
            resized();
            repaint();
        }
        else
            statusLabel.setText ("Already at root world", juce::dontSendNotification);
    }

    void setPaneComponentsVisible (bool shouldBeVisible)
    {
        const auto info = canvas.getSelectedDiscInfo();
        dataPaneTitle.setVisible (shouldBeVisible);
        dataPaneSummary.setVisible (shouldBeVisible);
        triggerModeLabel.setVisible (shouldBeVisible);
        triggerModeBox.setVisible (shouldBeVisible);
        holdDropsToggle.setVisible (shouldBeVisible);
        discLevelSlider.setVisible (shouldBeVisible); discPanSlider.setVisible (shouldBeVisible);
        discMuteButton.setVisible (shouldBeVisible); discSoloButton.setVisible (shouldBeVisible);
        elementModeLabel.setVisible (shouldBeVisible);
        elementModeBox.setVisible (shouldBeVisible);
        elementProbabilityBox.setVisible (shouldBeVisible
                                          && elementModeBox.getSelectedId() - 1 == static_cast<int> (Disc::ElementMode::probability));
        nestedWorldDot.setVisible (false);
        scCodeDot.setVisible (false);
        pdPatchDot.setVisible (false);
        scSheetDot.setVisible (false);
        orcaGridDot.setVisible (false);
        carouselDot.setVisible (false);
        pipeWorldDot.setVisible (false);

        for (auto& dot : elementDots)
            dot->setVisible (shouldBeVisible);

        worldLabel.setVisible (shouldBeVisible);
        worldInfoLabel.setVisible (shouldBeVisible);
        worldRemoveButton.setVisible (shouldBeVisible && info.hasNestedWorld);
        worldAddButton.setVisible (shouldBeVisible);
        worldEnterButton.setVisible (shouldBeVisible && info.hasNestedWorld);
        scCodeLabel.setVisible (shouldBeVisible);
        scCodeOpenButton.setVisible (shouldBeVisible && info.hasScCode);
        scCodeMinusButton.setVisible (shouldBeVisible && info.hasScCode);
        scCodePlusButton.setVisible (shouldBeVisible);
        scCodeInfoLabel.setVisible (shouldBeVisible);
        pdPatchLabel.setVisible (shouldBeVisible);
        pdPatchOpenButton.setVisible (shouldBeVisible && info.hasPdPatch);
        pdPatchMinusButton.setVisible (shouldBeVisible && info.hasPdPatch);
        pdPatchPlusButton.setVisible (shouldBeVisible);
        pdPatchInfoLabel.setVisible (shouldBeVisible);
        scSheetLabel.setVisible (shouldBeVisible);
        scSheetOpenButton.setVisible (shouldBeVisible && info.hasScSheet);
        scSheetMinusButton.setVisible (shouldBeVisible && info.hasScSheet);
        scSheetPlusButton.setVisible (shouldBeVisible);
        scSheetInfoLabel.setVisible (shouldBeVisible);
        orcaGridLabel.setVisible (shouldBeVisible);
        orcaGridOpenButton.setVisible (shouldBeVisible && info.hasOrcaGrid);
        orcaGridMinusButton.setVisible (shouldBeVisible && info.hasOrcaGrid);
        orcaGridPlusButton.setVisible (shouldBeVisible);
        orcaGridInfoLabel.setVisible (shouldBeVisible);
        carouselLabel.setVisible (shouldBeVisible);
        carouselOpenButton.setVisible (shouldBeVisible && info.hasCarousel);
        carouselMinusButton.setVisible (shouldBeVisible && info.hasCarousel);
        carouselPlusButton.setVisible (shouldBeVisible);
        carouselInfoLabel.setVisible (shouldBeVisible);
        pipeWorldLabel.setVisible (shouldBeVisible);
        pipeWorldOpenButton.setVisible (shouldBeVisible && info.hasPipeWorld);
        pipeWorldMinusButton.setVisible (shouldBeVisible && info.hasPipeWorld);
        pipeWorldPlusButton.setVisible (shouldBeVisible);
        pipeWorldInfoLabel.setVisible (shouldBeVisible);
        const auto canFire = info.soundElementCount > 0 || info.hasNestedWorld || info.hasScCode
                          || info.hasPdPatch || info.hasScSheet || info.hasOrcaGrid
                          || info.hasCarousel || info.hasPipeWorld;
        fireDiscButton.setVisible (shouldBeVisible && canFire);
        dataPaneCloseButton.setVisible (shouldBeVisible);
    }

    void refreshDataPane()
    {
        const auto info = canvas.getSelectedDiscInfo();

        if (! info.valid)
        {
            dataPaneTitle.setText ("Disc", juce::dontSendNotification);
            dataPaneSummary.setText ("No disc selected", juce::dontSendNotification);
            worldLabel.setText ("Nested worlds", juce::dontSendNotification);
            worldInfoLabel.setText ("none", juce::dontSendNotification);
            scCodeLabel.setText ("SC code", juce::dontSendNotification);
            scCodeInfoLabel.setText ("none", juce::dontSendNotification);
            pdPatchLabel.setText ("Pd patch", juce::dontSendNotification);
            pdPatchInfoLabel.setText ("none", juce::dontSendNotification);
            scSheetLabel.setText ("SCsheet", juce::dontSendNotification);
            scSheetInfoLabel.setText ("none", juce::dontSendNotification);
            orcaGridLabel.setText ("Orca", juce::dontSendNotification);
            orcaGridInfoLabel.setText ("none", juce::dontSendNotification);
            carouselLabel.setText ("Carousel", juce::dontSendNotification);
            carouselInfoLabel.setText ("none", juce::dontSendNotification);
            pipeWorldLabel.setText ("Pipe world", juce::dontSendNotification);
            pipeWorldInfoLabel.setText ("none", juce::dontSendNotification);
            selectedElement = {};
            triggerModeBox.setSelectedId (1, juce::dontSendNotification);
            holdDropsToggle.setToggleState (false, juce::dontSendNotification);
            discLevelSlider.setValue (1.0, juce::dontSendNotification); discPanSlider.setValue (0.0, juce::dontSendNotification);
            discMuteButton.setToggleState (false, juce::dontSendNotification); discSoloButton.setToggleState (false, juce::dontSendNotification);
            elementModeBox.setSelectedId (1, juce::dontSendNotification);
            elementProbabilityBox.setText ("50%", false);
            refreshElementDots();
            return;
        }

        const auto totalElements = info.nestedWorldCount
                                 + info.scCodeCount
                                 + info.pdPatchCount
                                 + info.scSheetCount
                                 + info.orcaGridCount
                                 + info.carouselCount
                                 + info.pipeWorldCount;

        dataPaneTitle.setText ("Disc", juce::dontSendNotification);
        triggerModeBox.setSelectedId (info.triggerMode + 1, juce::dontSendNotification);
        holdDropsToggle.setToggleState (info.holdDropsUntilFinished, juce::dontSendNotification);
        discLevelSlider.setValue (info.level, juce::dontSendNotification); discPanSlider.setValue (info.pan, juce::dontSendNotification);
        discMuteButton.setToggleState (info.muted, juce::dontSendNotification); discSoloButton.setToggleState (info.solo, juce::dontSendNotification);
        elementModeBox.setSelectedId (info.elementMode + 1, juce::dontSendNotification);
        elementProbabilityBox.setText (juce::String (info.elementProbability * 100.0, 0) + "%", false);
        elementProbabilityBox.setVisible (dataPaneOpen && info.elementMode == static_cast<int> (Disc::ElementMode::probability));
        dataPaneSummary.setText (juce::String (totalElements) + " elements    "
                                     + juce::String (info.nestedRouteCount) + " inner pipes    "
                                     + juce::String (info.nestedDiscCount) + " inner discs",
                                 juce::dontSendNotification);
        worldLabel.setText ("Nested worlds", juce::dontSendNotification);
        worldInfoLabel.setText (info.nestedWorldCount > 0
                                    ? juce::String (info.nestedWorldCount) + " inside"
                                    : "none",
                                juce::dontSendNotification);
        scCodeLabel.setText ("SC code", juce::dontSendNotification);
        scCodeInfoLabel.setText (info.hasScCode ? juce::String (info.scCodeCount) + " blocks  /  " + durationLabelText (info.scDurationSeconds)
                                                : "none",
                                 juce::dontSendNotification);
        pdPatchLabel.setText ("Pd patch", juce::dontSendNotification);
        pdPatchInfoLabel.setText (info.hasPdPatch ? juce::String (info.pdPatchCount) + " patches  /  " + durationLabelText (info.pdDurationSeconds)
                                                  : "none",
                                  juce::dontSendNotification);
        scSheetLabel.setText ("SCsheet", juce::dontSendNotification);
        scSheetInfoLabel.setText (info.hasScSheet
                                      ? juce::String (info.scSheetCount) + " sheets  /  " + juce::String (info.scSheetRowCount) + " rows"
                                      : "none",
                                  juce::dontSendNotification);
        orcaGridLabel.setText ("Orca", juce::dontSendNotification);
        orcaGridInfoLabel.setText (info.hasOrcaGrid
                                       ? juce::String (info.orcaGridCount) + " grids  /  " + juce::String (info.orcaGridWidth) + " x " + juce::String (info.orcaGridHeight)
                                       : "none",
                                   juce::dontSendNotification);
        carouselLabel.setText ("Carousel", juce::dontSendNotification);
        carouselInfoLabel.setText (info.hasCarousel ? juce::String (info.carouselCount) + " fields  /  " + juce::String (info.carouselItemCount) + " objects" : "none", juce::dontSendNotification);
        pipeWorldLabel.setText ("Pipe world", juce::dontSendNotification);
        pipeWorldInfoLabel.setText (info.hasPipeWorld ? juce::String (info.pipeWorldCount) + " worlds" : "none", juce::dontSendNotification);

        worldRemoveButton.setEnabled (info.hasNestedWorld);
        worldAddButton.setEnabled (true);
        worldEnterButton.setEnabled (info.hasNestedWorld);
        scCodeOpenButton.setEnabled (info.hasScCode);
        scCodeMinusButton.setEnabled (info.hasScCode);
        scCodePlusButton.setEnabled (true);
        pdPatchOpenButton.setEnabled (info.hasPdPatch);
        pdPatchMinusButton.setEnabled (info.hasPdPatch);
        pdPatchPlusButton.setEnabled (true);
        pdPatchInfoLabel.setEnabled (info.hasPdPatch);
        scSheetOpenButton.setEnabled (info.hasScSheet);
        scSheetMinusButton.setEnabled (info.hasScSheet);
        scSheetPlusButton.setEnabled (true);
        scSheetInfoLabel.setEnabled (info.hasScSheet);
        orcaGridOpenButton.setEnabled (info.hasOrcaGrid);
        orcaGridMinusButton.setEnabled (info.hasOrcaGrid);
        orcaGridPlusButton.setEnabled (true);
        orcaGridInfoLabel.setEnabled (info.hasOrcaGrid);
        carouselOpenButton.setEnabled (info.hasCarousel);
        carouselMinusButton.setEnabled (info.hasCarousel);
        carouselPlusButton.setEnabled (true);
        carouselInfoLabel.setEnabled (info.hasCarousel);
        pipeWorldOpenButton.setEnabled (info.hasPipeWorld);
        pipeWorldMinusButton.setEnabled (info.hasPipeWorld);
        pipeWorldPlusButton.setEnabled (true);
        pipeWorldInfoLabel.setEnabled (info.hasPipeWorld);
        fireDiscButton.setEnabled (info.soundElementCount > 0 || info.hasNestedWorld || info.hasScCode
                                   || info.hasPdPatch || info.hasScSheet || info.hasOrcaGrid
                                   || info.hasCarousel || info.hasPipeWorld);
        refreshElementDots();
    }

    void refreshElementDots()
    {
        const auto info = canvas.getSelectedDiscInfo();
        ElementDotSignature nextSignature;

        if (dataPaneOpen && info.valid)
        {
            nextSignature.visible = true;
            nextSignature.nestedWorlds = info.nestedWorldCount;
            nextSignature.scCode = info.scCodeCount;
            nextSignature.pdPatches = info.pdPatchCount;
            nextSignature.scSheets = info.scSheetCount;
            nextSignature.orcaGrids = info.orcaGridCount;
            nextSignature.carousels = info.carouselCount;
            nextSignature.pipeWorlds = info.pipeWorldCount;
        }

        if (! info.valid)
            selectedElement = {};
        else if (selectedElement.has_value() && ! hasElementDot (info, selectedElement->kind, selectedElement->index))
            selectedElement = {};

        nestedWorldDot.setVisible (false);
        scCodeDot.setVisible (false);
        pdPatchDot.setVisible (false);
        scSheetDot.setVisible (false);
        orcaGridDot.setVisible (false);
        carouselDot.setVisible (false);
        pipeWorldDot.setVisible (false);

        if (nextSignature == elementDotSignature)
        {
            updateElementDotSelection();
            return;
        }

        for (auto& dot : elementDots)
            removeChildComponent (dot.get());

        elementDots.clear();
        elementDotSignature = nextSignature;

        if (nextSignature.visible)
        {
            elementDots.reserve (static_cast<size_t> (nextSignature.nestedWorlds + nextSignature.scCode
                                                    + nextSignature.pdPatches + nextSignature.scSheets
                                                    + nextSignature.orcaGrids
                                                    + nextSignature.carousels
                                                    + nextSignature.pipeWorlds));
            addElementDots (ElementDotButton::Kind::nestedWorld, nextSignature.nestedWorlds, worldElementColour(), "Nested world");
            addElementDots (ElementDotButton::Kind::scCode, nextSignature.scCode, scCodeElementColour(), "SC code");
            addElementDots (ElementDotButton::Kind::pdPatch, nextSignature.pdPatches, pdPatchElementColour(), "Pd patch");
            addElementDots (ElementDotButton::Kind::scSheet, nextSignature.scSheets, scSheetElementColour(), "SCsheet");
            addElementDots (ElementDotButton::Kind::orcaGrid, nextSignature.orcaGrids, orcaGridElementColour(), "Orca grid");
            addElementDots (ElementDotButton::Kind::carousel, nextSignature.carousels, carouselElementColour(), "Carousel");
            addElementDots (ElementDotButton::Kind::pipeWorld, nextSignature.pipeWorlds, pipeElementColour(), "Pipe world");
        }

        resized();
    }

    void updateElementDotSelection()
    {
        for (auto& dot : elementDots)
        {
            dot->setVisible (elementDotSignature.visible);
            dot->setSelected (selectedElement.has_value()
                              && selectedElement->kind == dot->getKind()
                              && selectedElement->index == dot->getIndex());
        }
    }

    void addElementDots (ElementDotButton::Kind kind, int count, juce::Colour colour, const juce::String& name)
    {
        for (int i = 0; i < count; ++i)
        {
            auto dot = std::make_unique<ElementDotButton> (kind, i, colour, name + " " + juce::String (i + 1));
            dot->onSingleClick = [this] (ElementDotButton::Kind clickedKind, int clickedIndex)
            {
                selectedElement = ElementSelection { clickedKind, clickedIndex };
                refreshElementDots();
                grabKeyboardFocus();
            };
            dot->onDoubleClick = [this] (ElementDotButton::Kind clickedKind, int clickedIndex)
            {
                selectedElement = ElementSelection { clickedKind, clickedIndex };
                refreshElementDots();
                openSelectedElementDot();
            };
            dot->setSelected (selectedElement.has_value()
                              && selectedElement->kind == kind
                              && selectedElement->index == i);
            addAndMakeVisible (*dot);
            elementDots.push_back (std::move (dot));
        }
    }

    static bool hasElementDot (const RoadCanvas::DiscInfo& info, ElementDotButton::Kind kind, int index)
    {
        if (index < 0)
            return false;

        switch (kind)
        {
            case ElementDotButton::Kind::nestedWorld: return index < info.nestedWorldCount;
            case ElementDotButton::Kind::scCode:      return index < info.scCodeCount;
            case ElementDotButton::Kind::pdPatch:     return index < info.pdPatchCount;
            case ElementDotButton::Kind::scSheet:     return index < info.scSheetCount;
            case ElementDotButton::Kind::orcaGrid:    return index < info.orcaGridCount;
            case ElementDotButton::Kind::carousel:    return index < info.carouselCount;
            case ElementDotButton::Kind::pipeWorld:   return index < info.pipeWorldCount;
        }

        return false;
    }

    void openSelectedElementDot()
    {
        if (! selectedElement.has_value())
            return;

        switch (selectedElement->kind)
        {
            case ElementDotButton::Kind::nestedWorld: enterNestedWorldForSelectedDisc(); break;
            case ElementDotButton::Kind::scCode:      openScCodeWindow(); break;
            case ElementDotButton::Kind::pdPatch:     openPdPatchWindow(); break;
            case ElementDotButton::Kind::scSheet:     openScSheetWindow(); break;
            case ElementDotButton::Kind::orcaGrid:    openOrcaGridWindow(); break;
            case ElementDotButton::Kind::carousel:    openCarouselWindow(); break;
            case ElementDotButton::Kind::pipeWorld:   openPipeElementWindow(); break;
        }
    }

    bool deleteSelectedElementDot()
    {
        if (! selectedElement.has_value())
            return false;

        switch (selectedElement->kind)
        {
            case ElementDotButton::Kind::nestedWorld:
                canvas.removeNestedWorldFromSelectedDisc (selectedElement->index);
                break;

            case ElementDotButton::Kind::scCode:
                canvas.removeScCodeFromSelectedDisc (selectedElement->index);
                scCodeWindow = nullptr;
                break;

            case ElementDotButton::Kind::pdPatch:
                canvas.removePdPatchFromSelectedDisc (selectedElement->index);
                pdPatchWindow = nullptr;
                break;

            case ElementDotButton::Kind::scSheet:
                canvas.removeScSheetFromSelectedDisc (selectedElement->index);
                scSheetWindow = nullptr;
                break;

            case ElementDotButton::Kind::orcaGrid:
                canvas.removeOrcaGridFromSelectedDisc (selectedElement->index);
                orcaGridWindow = nullptr;
                break;
            case ElementDotButton::Kind::carousel:
                canvas.removeCarouselFromSelectedDisc (selectedElement->index);
                carouselWindow = nullptr;
                break;
            case ElementDotButton::Kind::pipeWorld:
                canvas.removePipeWorldFromSelectedDisc (selectedElement->index);
                pipeElementWindow = nullptr;
                pipeElementComponent = nullptr;
                break;
        }

        selectedElement = {};
        refreshDataPane();
        updateStatus();
        resized();
        repaint();
        return true;
    }

    void openScCodeWindow()
    {
        const auto handle = canvas.getSelectedDiscHandle();
        const auto info = canvas.getDiscInfo (handle);
        auto codeIndex = 0;

        if (selectedElement.has_value() && selectedElement->kind == ElementDotButton::Kind::scCode)
            codeIndex = selectedElement->index;

        codeIndex = juce::jlimit (0, juce::jmax (0, info.scCodeCount - 1), codeIndex);

        if (! info.valid || ! info.hasScCode)
        {
            statusLabel.setText ("Add SC code to this disc first", juce::dontSendNotification);
            return;
        }

        const auto codeInfo = canvas.getDiscScCodeInfo (handle, codeIndex);
        if (! codeInfo.valid)
        {
            statusLabel.setText ("That SC code block is no longer available", juce::dontSendNotification);
            return;
        }

        const auto safeThis = juce::Component::SafePointer<MainComponent> (this);
        auto refreshOwner = [safeThis]
        {
            if (safeThis != nullptr)
            {
                safeThis->refreshDataPane();
                safeThis->updateStatus();
            }
        };
        auto testCode = [safeThis, handle, codeIndex]() -> juce::String
        {
            if (safeThis == nullptr)
                return {};

            const auto triggers = safeThis->canvas.getDiscScCodeTrigger (handle, codeIndex);
            if (triggers.empty())
                return "Nothing to test in this block";

            safeThis->scAudio.triggerMany (triggers);
            safeThis->updateStatus();
            return "Test fired  /  " + safeThis->scAudio.getStatusText();
        };

        scCodeWindow = std::make_unique<FloatingEditorWindow> (
            "Disc SC Code " + juce::String (codeIndex + 1),
            new ScCodeEditorPanel (canvas, handle, codeIndex, refreshOwner, testCode),
            this,
            720,
            520,
            [safeThis]
            {
                if (safeThis != nullptr)
                {
                    safeThis->stopPreviewAudio();
                    safeThis->scCodeWindow = nullptr;
                }
            });
    }

    void openPdPatchWindow()
    {
        const auto handle = canvas.getSelectedDiscHandle();
        const auto info = canvas.getDiscInfo (handle);
        auto patchIndex = 0;

        if (selectedElement.has_value() && selectedElement->kind == ElementDotButton::Kind::pdPatch)
            patchIndex = selectedElement->index;

        patchIndex = juce::jlimit (0, juce::jmax (0, info.pdPatchCount - 1), patchIndex);

        if (! info.valid || ! info.hasPdPatch)
        {
            statusLabel.setText ("Add a Pd patch to this disc first", juce::dontSendNotification);
            return;
        }

        const auto patchInfo = canvas.getDiscPdPatchInfo (handle, patchIndex);
        if (! patchInfo.valid)
        {
            statusLabel.setText ("That Pd patch is no longer available", juce::dontSendNotification);
            return;
        }

        const auto safeThis = juce::Component::SafePointer<MainComponent> (this);
        auto refreshOwner = [safeThis]
        {
            if (safeThis != nullptr)
            {
                safeThis->refreshDataPane();
                safeThis->updateStatus();
            }
        };
        auto testPatch = [safeThis, handle, patchIndex]() -> juce::String
        {
            if (safeThis == nullptr)
                return {};

            const auto triggers = safeThis->canvas.getDiscPdPatchTrigger (handle, patchIndex);
            if (triggers.empty())
                return "Nothing to test in this patch";

            safeThis->scAudio.triggerMany (triggers);
            safeThis->updateStatus();
            return "Test fired  /  " + safeThis->scAudio.getStatusText();
        };
        auto triggerGui = [safeThis, handle, patchIndex] (const juce::String& receiver,
                                                          const juce::String& selector,
                                                          const juce::StringArray& atoms,
                                                          float value,
                                                          bool bangOnly,
                                                          const juce::String& triggerPatch) -> juce::String
        {
            if (safeThis == nullptr)
                return {};

            const auto livePatchInfo = safeThis->canvas.getDiscPdPatchInfo (handle, patchIndex);
            const auto patchToTrigger = triggerPatch.trim().isNotEmpty() ? triggerPatch : livePatchInfo.patch;

            if (! livePatchInfo.valid || patchToTrigger.trim().isEmpty())
                return "No Pd patch to trigger";

            if (receiver.trim().isEmpty())
                return "No Pd receiver on this control";

            const auto didSend = selector.trim().isNotEmpty()
                ? safeThis->scAudio.triggerPdMessage (patchToTrigger,
                                                      receiver,
                                                      selector,
                                                      atoms,
                                                      livePatchInfo.durationSeconds,
                                                      livePatchInfo.searchPath)
                : safeThis->scAudio.triggerPdGui (patchToTrigger,
                                                  receiver,
                                                  value,
                                                  bangOnly,
                                                  livePatchInfo.durationSeconds,
                                                  livePatchInfo.searchPath);

            if (didSend)
            {
                safeThis->updateStatus();
                return "Sent " + receiver + "  /  " + safeThis->scAudio.getStatusText();
            }

            const auto triggers = safeThis->canvas.getDiscPdPatchTrigger (handle, patchIndex);
            if (! triggers.empty())
                safeThis->scAudio.triggerMany (triggers);

            safeThis->updateStatus();
            return "GUI send failed; fired patch  /  " + safeThis->scAudio.getStatusText();
        };

        pdPatchWindow = std::make_unique<FloatingEditorWindow> (
            "Disc Pd Patch " + juce::String (patchIndex + 1),
            new PdPatchEditorPanel (canvas, handle, patchIndex, refreshOwner, testPatch, triggerGui, scAudio),
            this,
            1180,
            780,
            [safeThis]
            {
                if (safeThis != nullptr)
                {
                    safeThis->stopPreviewAudio();
                    safeThis->pdPatchWindow = nullptr;
                }
            });
    }

    void openScSheetWindow()
    {
        const auto handle = canvas.getSelectedDiscHandle();
        const auto info = canvas.getDiscInfo (handle);

        if (! info.valid || ! info.hasScSheet)
        {
            statusLabel.setText ("Add an SCsheet to this disc first", juce::dontSendNotification);
            return;
        }

        const auto safeThis = juce::Component::SafePointer<MainComponent> (this);
        auto refreshOwner = [safeThis]
        {
            if (safeThis != nullptr)
            {
                safeThis->refreshDataPane();
                safeThis->updateStatus();
            }
        };

        scSheetWindow = std::make_unique<FloatingEditorWindow> (
            "Disc SCsheet",
            new ScSheetEditorPanel (canvas, handle, refreshOwner),
            this,
            920,
            620,
            [safeThis]
            {
                if (safeThis != nullptr)
                {
                    safeThis->stopPreviewAudio();
                    safeThis->scSheetWindow = nullptr;
                }
            });
    }

    void openOrcaGridWindow()
    {
        const auto handle = canvas.getSelectedDiscHandle();
        const auto info = canvas.getDiscInfo (handle);

        if (! info.valid || ! info.hasOrcaGrid)
        {
            statusLabel.setText ("Add an Orca grid to this disc first", juce::dontSendNotification);
            return;
        }

        const auto safeThis = juce::Component::SafePointer<MainComponent> (this);
        auto refreshOwner = [safeThis]
        {
            if (safeThis != nullptr)
            {
                safeThis->refreshDataPane();
                safeThis->updateStatus();
            }
        };

        orcaGridWindow = std::make_unique<FloatingEditorWindow> (
            "Disc Orca Grid",
            new OrcaGridEditorPanel (canvas, handle, refreshOwner),
            this,
            760,
            560,
            [safeThis]
            {
                if (safeThis != nullptr)
                {
                    safeThis->stopPreviewAudio();
                    safeThis->orcaGridWindow = nullptr;
                }
            });
    }

    void openCarouselWindow()
    {
        const auto handle = canvas.getSelectedDiscHandle();
        const auto info = canvas.getDiscInfo (handle);
        int index = selectedElement && selectedElement->kind == ElementDotButton::Kind::carousel ? selectedElement->index : 0;
        if (! info.valid || ! juce::isPositiveAndBelow (index, info.carouselCount))
        { statusLabel.setText ("Add a Carousel to this disc first", juce::dontSendNotification); return; }
        const auto safeThis = juce::Component::SafePointer<MainComponent> (this);
        auto* panel = new CarouselEditorPanel (canvas, handle, index, scAudio, [safeThis]
        { if (safeThis != nullptr) { safeThis->refreshDataPane(); safeThis->updateStatus(); } });
        carouselPanel = panel;
        carouselWindow = std::make_unique<FloatingEditorWindow> ("Disc Carousel", panel, this, 1040, 700, [safeThis]
        { if (safeThis != nullptr) { safeThis->carouselPanel = nullptr; safeThis->carouselWindow = nullptr; } });
        carouselWindow->setResizeLimits (1040, 560, 2200, 1600);
    }

    void openMixerWindow()
    {
        if (mixerWindow != nullptr) { mixerWindow->toFront (true); return; }
        const auto safeThis = juce::Component::SafePointer<MainComponent> (this);
        auto* panel = new DiscMixerPanel (canvas, masterGain.load(),
            [safeThis] (float gain)
            {
                if (safeThis == nullptr) return;
                safeThis->masterGain.store (gain); safeThis->markProjectDirty();
            },
            [safeThis]
            {
                if (safeThis == nullptr) return;
                safeThis->markProjectDirty(); safeThis->refreshDataPane();
            });
        const auto width = juce::jlimit (520, 1500, 80 + (static_cast<int> (canvas.getMixerChannels().size()) + 1) * 120);
        mixerWindow = std::make_unique<FloatingEditorWindow> ("Mixer", panel, this, width, 520, [safeThis]
        { if (safeThis != nullptr) safeThis->mixerWindow = nullptr; });
        mixerWindow->setResizeLimits (520, 420, 1800, 900);
    }

    void refreshClockButton()
    {
        auto activeClocks = 1;
        for (const auto& clock : canvas.getSequencingClocks())
            if (clock.enabled) ++activeClocks;
        clockButton.setButtonText ("Clocks " + juce::String (activeClocks));
        clockButton.setTooltip (activeClocks == 1 ? "Configure auxiliary clocks"
                                                  : juce::String (activeClocks) + " clocks active");
    }

    void openClockSettings()
    {
        const auto safeThis = juce::Component::SafePointer<MainComponent> (this);
        auto content = std::make_unique<ClockSettingsComponent> (
            canvas.getSequencingClocks(),
            [safeThis] (const ClockSettingsComponent::ClockBank& clocks)
            {
                if (safeThis == nullptr) return;
                safeThis->canvas.setSequencingClocks (clocks);
                safeThis->refreshClockButton();
            });
        juce::CallOutBox::launchAsynchronously (std::move (content), clockButton.getBounds(), this);
    }

    void openPipeElementWindow()
    {
        const auto handle = canvas.getSelectedDiscHandle();
        const auto info = canvas.getDiscInfo (handle);
        const auto index = selectedElement && selectedElement->kind == ElementDotButton::Kind::pipeWorld ? selectedElement->index : 0;
        if (! info.valid || ! juce::isPositiveAndBelow (index, info.pipeWorldCount))
        {
            statusLabel.setText ("Add a Pipe world to this disc first", juce::dontSendNotification);
            return;
        }

        auto workspace = otherware::createPipeWorkspaceComponent();
        pipeElementComponent = workspace.get();
        pipeElementHandle = handle;
        pipeElementIndex = index;
        otherware::setPipeWorkspaceTempo (*pipeElementComponent, globalTempoBpm);
        const auto savedState = canvas.getDiscPipeWorld (handle, index);
        if (savedState.isNotEmpty()) otherware::applyPipeWorkspaceState (*pipeElementComponent, juce::JSON::parse (savedState));

        const auto safeThis = juce::Component::SafePointer<MainComponent> (this);
        otherware::setPipeWorkspaceChangeCallback (*pipeElementComponent, [safeThis]
        {
            if (safeThis == nullptr || safeThis->pipeElementComponent == nullptr) return;
            const auto state = otherware::createPipeWorkspaceState (*safeThis->pipeElementComponent);
            safeThis->canvas.setDiscPipeWorld (safeThis->pipeElementHandle, safeThis->pipeElementIndex, juce::JSON::toString (state, false));
        });
        pipeElementWindow = std::make_unique<FloatingEditorWindow> ("Disc Pipe World", workspace.release(), this, 1180, 760, [safeThis]
        {
            if (safeThis == nullptr) return;
            if (safeThis->pipeElementComponent != nullptr)
            {
                const auto state = otherware::createPipeWorkspaceState (*safeThis->pipeElementComponent);
                safeThis->canvas.setDiscPipeWorld (safeThis->pipeElementHandle, safeThis->pipeElementIndex, juce::JSON::toString (state, false));
            }
            safeThis->pipeElementComponent = nullptr;
            safeThis->pipeElementWindow = nullptr;
        });
        pipeElementWindow->setResizeLimits (900, 620, 2200, 1600);
    }

    void addElementToSelectedDisc()
    {
        if (canvas.addElementToSelectedDisc())
        {
            refreshDataPane();
            updateStatus();
        }
        else
            statusLabel.setText ("Select a disc first", juce::dontSendNotification);
    }

    void enterNestedWorldForSelectedDisc()
    {
        if (canvas.enterNestedWorldForSelectedDisc())
        {
            dataPaneOpen = false;
            selectedElement = {};
            refreshDataPane();
            refreshLayersPanel();
            updateStatus();
            resized();
            repaint();
        }
        else
            statusLabel.setText ("Select a disc first", juce::dontSendNotification);
    }

    void exitNestedWorld()
    {
        if (canvas.exitNestedWorld())
        {
            dataPaneOpen = false;
            selectedElement = {};
            refreshDataPane();
            refreshLayersPanel();
            updateStatus();
            resized();
            repaint();
        }
        else
            statusLabel.setText ("Already at root world", juce::dontSendNotification);
    }

    void fireSelectedDisc()
    {
        const auto info = canvas.getSelectedDiscInfo();
        if (! canvas.isSelectedDiscAudible())
        {
            statusLabel.setText (info.muted ? "Disc is muted" : "Another disc is soloed", juce::dontSendNotification);
            return;
        }
        const auto triggers = canvas.getSelectedDiscTriggers();
        const auto handle = canvas.getSelectedDiscHandle();
        juce::String discKey;
        for (const auto index : handle.worldPath) discKey << index << "/";
        discKey << handle.discIndex;
        const auto now = juce::Time::getMillisecondCounterHiRes();
        activeDiscs.erase (std::remove_if (activeDiscs.begin(), activeDiscs.end(), [now] (const auto& active)
        {
            return std::isfinite (active.untilMs) && active.untilMs <= now;
        }), activeDiscs.end());

        if (info.triggerMode == static_cast<int> (Disc::TriggerMode::whenFinished))
        {
            const auto active = std::find_if (activeDiscs.begin(), activeDiscs.end(), [&discKey] (const auto& item)
            {
                return item.key == discKey;
            });
            if (active != activeDiscs.end())
            {
                statusLabel.setText ("Disc is still active", juce::dontSendNotification);
                return;
            }
        }

        const auto carouselDocuments = canvas.getSelectedDiscCarousels();
        const auto pipeStates = canvas.getSelectedDiscPipeWorlds();
        const auto elementCount = static_cast<int> (triggers.size() + carouselDocuments.size() + pipeStates.size());
        if (elementCount == 0)
        {
            statusLabel.setText ("Selected disc has no triggerable elements", juce::dontSendNotification);
            return;
        }

        std::vector<bool> shouldFire (static_cast<size_t> (elementCount), false);
        const auto mode = static_cast<Disc::ElementMode> (info.elementMode);
        if (mode == Disc::ElementMode::together)
            std::fill (shouldFire.begin(), shouldFire.end(), true);
        else if (mode == Disc::ElementMode::chain)
            shouldFire.front() = true;
        else if (mode == Disc::ElementMode::random)
            shouldFire[static_cast<size_t> (juce::Random::getSystemRandom().nextInt (elementCount))] = true;
        else if (mode == Disc::ElementMode::probability)
            for (auto&& fire : shouldFire)
                fire = juce::Random::getSystemRandom().nextDouble() <= info.elementProbability;
        else
        {
            auto state = std::find_if (discSequenceStates.begin(), discSequenceStates.end(), [&discKey] (const auto& item)
            {
                return item.key == discKey;
            });
            if (state == discSequenceStates.end())
            {
                discSequenceStates.push_back ({ discKey, 0 });
                state = std::prev (discSequenceStates.end());
            }
            const auto selected = state->nextElement % elementCount;
            shouldFire[static_cast<size_t> (selected)] = true;
            state->nextElement = (selected + 1) % elementCount;
        }

        if (std::none_of (shouldFire.begin(), shouldFire.end(), [] (bool fire) { return fire; }))
        {
            statusLabel.setText ("No elements passed the probability check", juce::dontSendNotification);
            return;
        }

        if (info.triggerMode == static_cast<int> (Disc::TriggerMode::exclusive))
            stopPreviewAudio();

        std::vector<DiscAudioTrigger> selectedTriggers;
        for (size_t i = 0; i < triggers.size(); ++i)
            if (shouldFire[i]) selectedTriggers.push_back (triggers[i]);
        scAudio.triggerMany (selectedTriggers);
        carouselRuntimes.clear();
        for (size_t i = 0; i < carouselDocuments.size(); ++i)
        {
            if (! shouldFire[triggers.size() + i]) continue;
            auto runtime = std::make_unique<CarouselEditorComponent>();
            runtime->setDocument (carouselDocuments[i]);
            runtime->onNote = [this] (int midi) { scAudio.triggerMidiNote (midi); };
            runtime->setRunning (true);
            carouselRuntimes.push_back (std::move (runtime));
        }
        if (carouselPanel != nullptr && ! carouselRuntimes.empty()) carouselPanel->start();
        pipeRuntimes.clear();
        for (size_t i = 0; i < pipeStates.size(); ++i)
        {
            if (! shouldFire[triggers.size() + carouselDocuments.size() + i]) continue;
            const auto& stateText = pipeStates[i];
            auto runtime = otherware::createPipeWorkspaceComponent();
            otherware::setPipeWorkspaceTempo (*runtime, globalTempoBpm);
            if (stateText.isNotEmpty()) otherware::applyPipeWorkspaceState (*runtime, juce::JSON::parse (stateText));
            otherware::setPipeWorkspaceRunning (*runtime, true);
            pipeRuntimes.push_back (std::move (runtime));
        }
        if (pipeElementComponent != nullptr && ! pipeRuntimes.empty()) otherware::setPipeWorkspaceRunning (*pipeElementComponent, true);

        if (mode == Disc::ElementMode::chain && elementCount > 1)
        {
            const auto firstDuration = ! triggers.empty() ? triggerDurationSeconds (triggers.front())
                                                          : std::numeric_limits<double>::infinity();
            pendingElementChains.erase (std::remove_if (pendingElementChains.begin(), pendingElementChains.end(), [&discKey] (const auto& chain)
            {
                return chain.key == discKey;
            }), pendingElementChains.end());
            if (std::isfinite (firstDuration))
                pendingElementChains.push_back ({ discKey, triggers, carouselDocuments, pipeStates, 1,
                                                  now + firstDuration * 1000.0 });
        }

        auto durationSeconds = 0.45;
        auto indefinite = ! carouselRuntimes.empty() || ! pipeRuntimes.empty();
        for (const auto& trigger : selectedTriggers)
        {
            if (trigger.hasScCode())
            {
                indefinite = indefinite || trigger.scDurationSeconds < 0.0f;
                if (trigger.scDurationSeconds >= 0.0f) durationSeconds = juce::jmax (durationSeconds, static_cast<double> (trigger.scDurationSeconds));
            }
            if (trigger.hasPdPatch())
            {
                indefinite = indefinite || trigger.pdDurationSeconds < 0.0f;
                if (trigger.pdDurationSeconds >= 0.0f) durationSeconds = juce::jmax (durationSeconds, static_cast<double> (trigger.pdDurationSeconds));
            }
            if (trigger.hasScSheetScore() || trigger.hasOrcaGridScore())
                durationSeconds = juce::jmax (durationSeconds, 1.0);
        }
        if (mode == Disc::ElementMode::chain)
        {
            durationSeconds = 0.0;
            indefinite = ! carouselDocuments.empty() || ! pipeStates.empty();
            for (const auto& trigger : triggers)
            {
                const auto duration = triggerDurationSeconds (trigger);
                if (! std::isfinite (duration)) { indefinite = true; break; }
                durationSeconds += duration;
            }
        }
        activeDiscs.erase (std::remove_if (activeDiscs.begin(), activeDiscs.end(), [&discKey] (const auto& item)
        {
            return item.key == discKey;
        }), activeDiscs.end());
        activeDiscs.push_back ({ discKey, indefinite ? std::numeric_limits<double>::infinity()
                                                     : now + durationSeconds * 1000.0 });
        statusLabel.setText ("Fired disc  /  " + scAudio.getStatusText(), juce::dontSendNotification);
    }

    void updateStatus()
    {
        juce::StringArray status;
        if (canvas.isModulationLayerVisible())
        {
            if (canvas.getModulatorCount() > 0) status.add (juce::String (canvas.getModulatorCount()) + " modulators");
            if (canvas.getModulationConnectionCount() > 0) status.add (juce::String (canvas.getModulationConnectionCount()) + " routings");
        }
        if (canvas.getRouteCount() > 0) status.add (juce::String (canvas.getRouteCount()) + " pipes");
        if (canvas.getDiscCount() > 0) status.add (juce::String (canvas.getDiscCount()) + " discs");
        if (canvas.getElementCount() > 0) status.add (juce::String (canvas.getElementCount()) + " elements");
        if (canvas.getWorldDepth() > 0) status.add ("Layer " + juce::String (canvas.getWorldDepth()));
        status.add (scAudio.isReady() ? "Ready" : "Audio offline");
        statusLabel.setText (status.joinIntoString ("  /  "), juce::dontSendNotification);
        refreshLayersPanel();
        refreshToolVisibility();
    }

    void markProjectDirty()
    {
        if (suppressProjectDirty) return;
        projectDirty = true;
        updateProjectPresentation();
    }

    static double beatsForGridChoice (int choice)
    {
        static constexpr std::array<double, 8> values { 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 2.0, 1.0, 4.0, 8.0, 16.0 };
        return values[static_cast<size_t> (juce::jlimit (1, 8, choice) - 1)];
    }

    void commitGlobalTempo()
    {
        const auto enteredTempo = tempoEditor.getText().getDoubleValue();
        const auto newTempo = juce::jlimit (20.0, 400.0, enteredTempo > 0.0 ? enteredTempo : globalTempoBpm);
        tempoEditor.setText (juce::String (newTempo, newTempo == std::round (newTempo) ? 0 : 1), false);
        if (std::abs (newTempo - globalTempoBpm) < 0.001) return;
        globalTempoBpm = newTempo;
        canvas.setFlowTiming (globalTempoBpm, beatsForGridChoice (gridUnitChoice));
        markProjectDirty();
    }

    void applyGlobalTiming (const juce::ValueTree& project)
    {
        globalTempoBpm = juce::jlimit (20.0, 400.0, static_cast<double> (project.getProperty ("globalTempo", 120.0)));
        gridUnitChoice = juce::jlimit (1, 8, static_cast<int> (project.getProperty ("gridUnit", 5)));
        triggerQuantizeChoice = juce::jlimit (0, 5, static_cast<int> (project.getProperty ("triggerQuantize", 0)));
        tempoEditor.setText (juce::String (globalTempoBpm, globalTempoBpm == std::round (globalTempoBpm) ? 0 : 1), false);
        gridUnitBox.setSelectedId (gridUnitChoice, juce::dontSendNotification);
        triggerQuantizeSlider.setValue (triggerQuantizeChoice, juce::dontSendNotification);
        canvas.setFlowTiming (globalTempoBpm, beatsForGridChoice (gridUnitChoice));
        masterGain.store (juce::jlimit (0.0f, 1.5f, static_cast<float> (project.getProperty ("masterGain", 1.0f))));
        refreshClockButton();
    }

    void updateProjectPresentation()
    {
#if JUCE_MAC
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
            window->setName ({});
#else
        const auto name = currentProjectFile == juce::File() ? "Untitled" : currentProjectFile.getFileNameWithoutExtension();
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
            window->setName ("Blendings - " + name + (projectDirty ? " *" : ""));
#endif
    }

    void closeElementWindows()
    {
        stopPreviewAudio();
        if (pipeElementComponent != nullptr)
            canvas.setDiscPipeWorld (pipeElementHandle, pipeElementIndex, juce::JSON::toString (otherware::createPipeWorkspaceState (*pipeElementComponent), false));
        pipeElementComponent = nullptr;
        pipeElementWindow = nullptr;
        mixerWindow = nullptr;
        carouselPanel = nullptr;
        scCodeWindow = nullptr; pdPatchWindow = nullptr; scSheetWindow = nullptr; orcaGridWindow = nullptr; carouselWindow = nullptr;
    }

    void showProjectError (const juce::String& message)
    {
        juce::NativeMessageBox::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                                .withTitle ("Project Error").withMessage (message).withButton ("OK"), nullptr);
    }

    bool writeProjectToFile (const juce::File& file)
    {
        auto project = canvas.createProjectState();
        project.setProperty ("globalTempo", globalTempoBpm, nullptr);
        project.setProperty ("gridUnit", gridUnitChoice, nullptr);
        project.setProperty ("triggerQuantize", triggerQuantizeChoice, nullptr);
        project.setProperty ("masterGain", masterGain.load(), nullptr);
        const auto xml = project.createXml();
        if (xml == nullptr || ! file.replaceWithText (xml->toString())) return false;
        currentProjectFile = file; projectDirty = false; updateProjectPresentation();
        statusLabel.setText ("Saved " + file.getFileName(), juce::dontSendNotification);
        return true;
    }

    void saveProject (std::function<void(bool)> completion = {})
    {
        if (currentProjectFile == juce::File()) { saveProjectAs (std::move (completion)); return; }
        const auto ok = writeProjectToFile (currentProjectFile);
        if (! ok) showProjectError ("The project could not be saved.");
        if (completion) completion (ok);
    }

    void saveProjectAs (std::function<void(bool)> completionHandler = {})
    {
        const auto safeThis = juce::Component::SafePointer<MainComponent> (this);
        const auto suggested = (currentProjectFile == juce::File()
                                    ? juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Untitled.otherware")
                                    : currentProjectFile);
        projectFileChooser = std::make_unique<juce::FileChooser> ("Save Project", suggested, "*.otherware");
        projectFileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                                         [safeThis, savedCallback = std::move (completionHandler)] (const juce::FileChooser& chooser) mutable
        {
            if (safeThis == nullptr) return;
            auto file = chooser.getResult();
            if (file != juce::File() && file.getFileExtension().isEmpty()) file = file.withFileExtension (".otherware");
            const auto ok = file != juce::File() && safeThis->writeProjectToFile (file);
            if (file != juce::File() && ! ok) safeThis->showProjectError ("The project could not be saved.");
            safeThis->projectFileChooser = nullptr;
            if (savedCallback) savedCallback (ok);
        });
    }

    void confirmDiscardChanges (std::function<void()> pendingAction)
    {
        if (! projectDirty) { pendingAction(); return; }
        const auto safeThis = juce::Component::SafePointer<MainComponent> (this);
        projectPrompt = juce::AlertWindow::showScopedAsync (
            juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon).withTitle ("Save changes?")
                .withMessage ("Your current project has unsaved changes.").withButton ("Save").withButton ("Discard").withButton ("Cancel"),
            [safeThis, confirmedAction = std::move (pendingAction)] (int result) mutable
            {
                if (safeThis == nullptr) return;
                if (result == 1) safeThis->saveProject ([actionAfterSave = std::move (confirmedAction)] (bool saved) mutable { if (saved) actionAfterSave(); });
                else if (result == 2) confirmedAction();
            });
    }

    void newProject()
    {
        const auto safeThis = juce::Component::SafePointer<MainComponent> (this);
        confirmDiscardChanges ([safeThis]
        {
            if (safeThis == nullptr) return;
            safeThis->closeElementWindows();
            juce::ValueTree project ("otherwareProject"); project.setProperty ("version", 1, nullptr);
            project.addChild (juce::ValueTree ("routes"), -1, nullptr); project.addChild (juce::ValueTree ("discs"), -1, nullptr);
            const juce::ScopedValueSetter<bool> suppress (safeThis->suppressProjectDirty, true);
            safeThis->canvas.applyProjectState (project);
            safeThis->resetTransport();
            safeThis->applyGlobalTiming (project);
            safeThis->currentProjectFile = juce::File(); safeThis->projectDirty = false; safeThis->dataPaneOpen = false; safeThis->selectedElement = {};
            safeThis->refreshDataPane(); safeThis->resized(); safeThis->updateStatus(); safeThis->updateProjectPresentation();
        });
    }

    void openProject()
    {
        const auto safeThis = juce::Component::SafePointer<MainComponent> (this);
        confirmDiscardChanges ([safeThis]
        {
            if (safeThis == nullptr) return;
            safeThis->projectFileChooser = std::make_unique<juce::FileChooser> ("Open Project", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.otherware");
            safeThis->projectFileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safeThis] (const juce::FileChooser& chooser)
            {
                if (safeThis == nullptr) return;
                const auto file = chooser.getResult();
                if (file != juce::File())
                {
                    const auto xml = juce::XmlDocument::parse (file);
                    const auto state = xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();
                    const juce::ScopedValueSetter<bool> suppress (safeThis->suppressProjectDirty, true);
                    if (safeThis->canvas.applyProjectState (state))
                    {
                        safeThis->resetTransport();
                        safeThis->applyGlobalTiming (state);
                        safeThis->closeElementWindows();
                        safeThis->currentProjectFile = file; safeThis->projectDirty = false; safeThis->dataPaneOpen = false; safeThis->selectedElement = {};
                        safeThis->refreshDataPane(); safeThis->resized(); safeThis->updateStatus(); safeThis->updateProjectPresentation();
                    }
                    else safeThis->showProjectError ("This is not a valid OTHERWARE project.");
                }
                safeThis->projectFileChooser = nullptr;
            });
        });
    }
};

class BlendingsApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "Blendings"; }
    const juce::String getApplicationVersion() override    { return "0.2.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->requestClose();
        else
            quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (juce::String name)
            : DocumentWindow (std::move (name),
                              juce::Colour (0xff101614),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
#if JUCE_MAC
            setName ({});
#endif
            setResizable (true, true);
            fitToScreen();
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            requestClose();
        }

        void requestClose()
        {
            if (auto* mainComponent = dynamic_cast<MainComponent*> (getContentComponent()))
            {
                mainComponent->requestApplicationClose ([]
                {
                    if (auto* application = juce::JUCEApplication::getInstance())
                        application->quit();
                });
                return;
            }

            if (auto* application = juce::JUCEApplication::getInstance())
                application->quit();
        }

    private:
        void fitToScreen()
        {
            const auto& displays = juce::Desktop::getInstance().getDisplays();
            const auto* display = displays.getDisplayForRect (getBounds());

            if (display == nullptr)
                display = displays.getPrimaryDisplay();

            if (display == nullptr)
            {
                centreWithSize (getWidth(), getHeight());
                return;
            }

            auto screenArea = display->userArea;

            if (screenArea.isEmpty())
                screenArea = display->totalArea;

            if (screenArea.isEmpty())
            {
                centreWithSize (getWidth(), getHeight());
                return;
            }

            auto contentArea = screenArea;

            if (auto* peer = getPeer())
                if (const auto frameSize = peer->getFrameSizeIfPresent())
                    frameSize->subtractFrom (contentArea);

            if (contentArea.getWidth() < 700 || contentArea.getHeight() < 500)
                contentArea = screenArea;

            setBounds (contentArea);
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (BlendingsApplication)
