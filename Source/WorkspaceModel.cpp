#include "WorkspaceModel.h"

namespace blendings
{
float RoadRoute::getLength() const
{
    float length = 0.0f;
    for (size_t i = 1; i < points.size(); ++i)
        length += points[i - 1].getDistanceFrom (points[i]);
    return length;
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
        "..1D4...........", "..*:05Cf4.......", "................",
        "..2D8...........", "..*:15Ge6.......", "................",
        "# edit like ORCA#", "................", "................", "................"
    };
    for (int y = 0; y < rows.size(); ++y)
        for (int x = 0; x < rows[y].length(); ++x)
            grid.setGlyph (x, y, static_cast<char> (rows[y][x]));
    return grid.createSnapshot();
}

std::array<SequencingClock, 4> defaultSequencingClocks()
{
    return {{
        { "Half time", 0.5, 0.0, 0.0, false },
        { "Double time", 2.0, 0.0, 0.0, false },
        { "Three over two", 1.5, 0.0, 0.0, false },
        { "Offset", 1.0, 0.5, 0.0, false }
    }};
}

juce::String modulationTargetName (ModulationTargetKind kind)
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

juce::StringArray modulationParameterNames (ModulationTargetKind kind)
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

int Disc::getElementCount() const noexcept
{
    return soundElementCount + nestedWorldCount + static_cast<int> (scCodeElements.size())
         + static_cast<int> (pdPatches.size()) + static_cast<int> (scSheets.size())
         + static_cast<int> (orcaGrids.size()) + static_cast<int> (carousels.size())
         + static_cast<int> (pipeWorlds.size()) + static_cast<int> (orbits.size());
}
}
