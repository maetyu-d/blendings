#include "OrbitsElement.h"
#include "AppTheme.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace blendings
{
namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;

juce::String starterCode (int pitch)
{
    return "var env = EnvGen.kr(Env.perc(0.006, sustain.max(0.18)), doneAction: 2);\n"
           "var sig = Saw.ar(" + juce::String (pitch) + ".midicps * [1, 1.004]);\n"
           "sig = RLPF.ar(sig, EnvGen.kr(Env([700, 5200, 900], [0.025, 0.42])), 0.24);\n"
           "Out.ar(out, Balance2.ar(sig[0], sig[1], pan, amp * env * 0.20));";
}

juce::String starterPdPatch (int pitch)
{
    return "#N canvas 20 30 520 320 10;\n"
           "#X obj 30 30 r trigger;\n"
           "#X obj 30 62 t b b;\n"
           "#X msg 30 94 1 \\, 0 380;\n"
           "#X obj 30 126 line~;\n"
           "#X obj 150 94 mtof;\n"
           "#X obj 150 126 osc~;\n"
           "#X obj 150 158 *~;\n"
           "#X obj 150 190 dac~;\n"
           "#X msg 150 62 " + juce::String (pitch) + ";\n"
           "#X connect 0 0 1 0;\n#X connect 1 0 2 0;\n#X connect 1 1 8 0;\n"
           "#X connect 2 0 3 0;\n#X connect 3 0 6 1;\n#X connect 8 0 4 0;\n"
           "#X connect 4 0 5 0;\n#X connect 5 0 6 0;\n#X connect 6 0 7 0;\n#X connect 6 0 7 1;";
}

juce::Colour trackColour (int index)
{
    static constexpr juce::uint32 colours[] {
        0xff38d8c5, 0xffff5a9d, 0xffffb33f, 0xff39a9ff,
        0xff9d69ff, 0xff82e85e, 0xffff7657, 0xff58d3ff
    };
    return juce::Colour (colours[static_cast<size_t> ((index % 8 + 8) % 8)]);
}

struct SpiralGeometry
{
    int turns = 4;
    float outerRadius = 0.42f;
    float innerScale = 0.12f;
    float exponent = 1.0f;
};

SpiralGeometry geometryForTrack (const OrbitsTrack& track)
{
    const auto beatsPerBar = track.timeSigNumerator * (4.0 / juce::jmax (1, track.timeSigDenominator));
    const auto totalBeats = beatsPerBar * juce::jmax (0.25, track.loopBars);
    SpiralGeometry geometry;
    geometry.turns = juce::jlimit (2, 18, static_cast<int> (std::round (totalBeats * 0.5)));
    const auto bpmNorm = juce::jlimit (0.0f, 1.0f, static_cast<float> ((track.bpm - 40.0) / 240.0));
    geometry.outerRadius = juce::jmap (bpmNorm, 0.48f, 0.30f);
    const auto denominatorNorm = juce::jlimit (0.0f, 1.0f, static_cast<float> ((track.timeSigDenominator - 1) / 15.0));
    geometry.innerScale = juce::jmap (denominatorNorm, 0.08f, 0.22f);
    const auto numeratorNorm = juce::jlimit (0.0f, 1.0f, static_cast<float> ((track.timeSigNumerator - 1) / 14.0));
    geometry.exponent = juce::jmap (numeratorNorm, 0.72f, 1.65f);
    return geometry;
}

juce::Point<float> spiralBodyPoint (const OrbitsTrack& track, double phase)
{
    const auto geometry = geometryForTrack (track);
    const auto bodyPhase = juce::jlimit (0.0, 1.0, phase);
    const auto shaped = std::pow (static_cast<float> (bodyPhase), geometry.exponent);
    const auto anglePhase = bodyPhase + track.phaseOffsetDegrees / 360.0;
    const auto theta = anglePhase * geometry.turns * twoPi;
    const auto innerRadius = geometry.outerRadius * geometry.innerScale;
    const auto radiusBase = innerRadius + shaped * (geometry.outerRadius - innerRadius);
    const auto warp = 1.0 + juce::jlimit (0.0, 1.0, track.orbitWarpAmount) * std::sin (3.0 * theta);
    const auto radius = juce::jmax (static_cast<double> (innerRadius * 0.15f), radiusBase * warp);
    const auto xScale = std::cos (track.yRotationDegrees * juce::MathConstants<double>::pi / 180.0);
    const auto yScale = std::cos (track.xRotationDegrees * juce::MathConstants<double>::pi / 180.0);
    auto x = radius * std::cos (theta) * xScale;
    auto y = radius * std::sin (theta) * yScale;
    const auto bend = juce::jlimit (-1.0, 1.0, track.spiralTwistAmount) * (0.35 + 0.75 * shaped);
    const auto yNorm = y / juce::jmax (0.001, static_cast<double> (geometry.outerRadius));
    const auto xNorm = x / juce::jmax (0.001, static_cast<double> (geometry.outerRadius));
    x += bend * yNorm * std::abs (yNorm) * geometry.outerRadius;
    y *= 1.0 - 0.22 * std::abs (bend) * xNorm * xNorm;
    return { static_cast<float> (0.5 + track.xOffset + x),
             static_cast<float> (0.5 + track.yOffset + y) };
}

juce::Point<float> spiralPoint (const OrbitsTrack& track, double phase)
{
    const auto wrapped = phase - std::floor (phase);
    const auto geometry = geometryForTrack (track);
    const auto start = spiralBodyPoint (track, 0.0);
    const auto end = spiralBodyPoint (track, 1.0);
    const auto closingLength = start.getDistanceFrom (end);
    const auto innerRadius = geometry.outerRadius * geometry.innerScale;
    const auto estimatedSpiralLength = juce::MathConstants<float>::pi * static_cast<float> (geometry.turns)
                                     * (geometry.outerRadius + innerRadius);
    const auto closingShare = juce::jlimit (0.01, 0.25,
        static_cast<double> (closingLength / juce::jmax (0.0001f, estimatedSpiralLength + closingLength)));
    const auto spiralShare = 1.0 - closingShare;
    if (wrapped <= spiralShare)
        return spiralBodyPoint (track, wrapped / spiralShare);

    return end + (start - end) * static_cast<float> ((wrapped - spiralShare) / closingShare);
}

}

OrbitsDocument OrbitsDocument::createDefault()
{
    OrbitsDocument document;
    OrbitsTrack first;
    first.name = "Track 1";
    first.colourIndex = 0;
    first.bpm = 120.0;
    first.loopBars = 2.0;
    document.tracks.push_back (first);

    OrbitsTrack second;
    second.name = "Track 2";
    second.colourIndex = 1;
    second.bpm = 132.0;
    second.loopBars = 3.0;
    second.phaseOffsetDegrees = 110.0;
    second.xOffset = 0.04;
    document.tracks.push_back (second);
    return document;
}

juce::ValueTree OrbitsDocument::toValueTree() const
{
    juce::ValueTree root ("orbits");
    root.setProperty ("version", 4, nullptr);
    root.setProperty ("projectTempo", projectTempoBpm, nullptr);
    root.setProperty ("snap", snapToMusicalGrid, nullptr);
    root.setProperty ("snapDivisions", snapDivisionsPerBeat, nullptr);
    for (const auto& track : tracks)
    {
        juce::ValueTree node ("track");
        node.setProperty ("name", track.name, nullptr);
        node.setProperty ("colour", track.colourIndex, nullptr);
        node.setProperty ("bpm", track.bpm, nullptr);
        node.setProperty ("numerator", track.timeSigNumerator, nullptr);
        node.setProperty ("denominator", track.timeSigDenominator, nullptr);
        node.setProperty ("bars", track.loopBars, nullptr);
        node.setProperty ("thickness", track.thickness, nullptr);
        node.setProperty ("warp", track.orbitWarpAmount, nullptr);
        node.setProperty ("twist", track.spiralTwistAmount, nullptr);
        node.setProperty ("phase", track.phaseOffsetDegrees, nullptr);
        node.setProperty ("xRotation", track.xRotationDegrees, nullptr);
        node.setProperty ("yRotation", track.yRotationDegrees, nullptr);
        node.setProperty ("xOffset", track.xOffset, nullptr);
        node.setProperty ("yOffset", track.yOffset, nullptr);
        node.setProperty ("hidden", track.hidden, nullptr);
        node.setProperty ("muted", track.muted, nullptr);
        node.setProperty ("solo", track.solo, nullptr);
        node.setProperty ("gain", track.gain, nullptr);
        node.setProperty ("pan", track.pan, nullptr);
        node.setProperty ("outputRoute", static_cast<int> (track.outputRoute), nullptr);
        node.setProperty ("clockMode", static_cast<int> (track.clockMode), nullptr);
        node.setProperty ("tempoRatio", track.tempoRatio, nullptr);
        node.setProperty ("resetPhase", track.resetPhaseOnStart, nullptr);
        node.setProperty ("relationshipAction", static_cast<int> (track.relationshipAction), nullptr);
        node.setProperty ("relationshipTarget", track.relationshipTarget, nullptr);
        for (const auto& line : track.lines)
        {
            juce::ValueTree lineNode ("line");
            lineNode.setProperty ("id", line.id, nullptr);
            lineNode.setProperty ("x1", line.start.x, nullptr);
            lineNode.setProperty ("y1", line.start.y, nullptr);
            lineNode.setProperty ("x2", line.end.x, nullptr);
            lineNode.setProperty ("y2", line.end.y, nullptr);
            lineNode.setProperty ("name", line.sound.name, nullptr);
            lineNode.setProperty ("code", line.sound.scCode, nullptr);
            lineNode.setProperty ("pdPatch", line.sound.pdPatch, nullptr);
            lineNode.setProperty ("playback", static_cast<int> (line.sound.playback), nullptr);
            lineNode.setProperty ("duration", line.sound.durationSeconds, nullptr);
            lineNode.setProperty ("probability", line.sound.probability, nullptr);
            lineNode.setProperty ("gain", line.sound.gain, nullptr);
            lineNode.setProperty ("pan", line.sound.pan, nullptr);
            lineNode.setProperty ("lineColour", line.sound.colourIndex, nullptr);
            lineNode.setProperty ("enabled", line.sound.enabled, nullptr);
            node.addChild (lineNode, -1, nullptr);
        }
        root.addChild (node, -1, nullptr);
    }
    return root;
}

OrbitsDocument OrbitsDocument::fromValueTree (const juce::ValueTree& root)
{
    if (! root.hasType ("orbits")) return createDefault();
    OrbitsDocument document;
    document.projectTempoBpm = juce::jlimit (20.0, 400.0, static_cast<double> (root.getProperty ("projectTempo", 120.0)));
    document.snapToMusicalGrid = static_cast<bool> (root.getProperty ("snap", false));
    document.snapDivisionsPerBeat = juce::jlimit (1, 16, static_cast<int> (root.getProperty ("snapDivisions", 4)));
    for (const auto& child : root)
    {
        if (! child.hasType ("track")) continue;
        OrbitsTrack track;
        track.name = child.getProperty ("name", "Track").toString();
        track.colourIndex = static_cast<int> (child.getProperty ("colour", static_cast<int> (document.tracks.size())));
        track.bpm = juce::jlimit (20.0, 400.0, static_cast<double> (child.getProperty ("bpm", 120.0)));
        track.timeSigNumerator = juce::jlimit (1, 15, static_cast<int> (child.getProperty ("numerator", 4)));
        track.timeSigDenominator = juce::jlimit (1, 16, static_cast<int> (child.getProperty ("denominator", 4)));
        track.loopBars = juce::jlimit (0.25, 64.0, static_cast<double> (child.getProperty ("bars", 2.0)));
        track.thickness = juce::jlimit (0.25, 4.0, static_cast<double> (child.getProperty ("thickness", 1.0)));
        track.orbitWarpAmount = juce::jlimit (0.0, 1.0, static_cast<double> (child.getProperty ("warp", 0.0)));
        track.spiralTwistAmount = juce::jlimit (-1.0, 1.0, static_cast<double> (child.getProperty ("twist", 0.0)));
        track.phaseOffsetDegrees = static_cast<double> (child.getProperty ("phase", 0.0));
        track.xRotationDegrees = juce::jlimit (-85.0, 85.0, static_cast<double> (child.getProperty ("xRotation", 0.0)));
        track.yRotationDegrees = juce::jlimit (-85.0, 85.0, static_cast<double> (child.getProperty ("yRotation", 0.0)));
        track.xOffset = juce::jlimit (-0.5, 0.5, static_cast<double> (child.getProperty ("xOffset", 0.0)));
        track.yOffset = juce::jlimit (-0.5, 0.5, static_cast<double> (child.getProperty ("yOffset", 0.0)));
        track.hidden = static_cast<bool> (child.getProperty ("hidden", false));
        track.muted = static_cast<bool> (child.getProperty ("muted", false));
        track.solo = static_cast<bool> (child.getProperty ("solo", false));
        track.gain = juce::jlimit (0.0f, 1.5f, static_cast<float> (child.getProperty ("gain", 1.0f)));
        track.pan = juce::jlimit (-1.0f, 1.0f, static_cast<float> (child.getProperty ("pan", 0.0f)));
        track.outputRoute = static_cast<OrbitsTrack::OutputRoute> (juce::jlimit (0, 2, static_cast<int> (child.getProperty ("outputRoute", 0))));
        track.clockMode = static_cast<OrbitsTrack::ClockMode> (juce::jlimit (0, 2, static_cast<int> (child.getProperty ("clockMode", 0))));
        track.tempoRatio = juce::jlimit (0.125, 8.0, static_cast<double> (child.getProperty ("tempoRatio", 1.0)));
        track.resetPhaseOnStart = static_cast<bool> (child.getProperty ("resetPhase", true));
        track.relationshipAction = static_cast<OrbitsTrack::RelationshipAction> (
            juce::jlimit (0, 4, static_cast<int> (child.getProperty ("relationshipAction", 0))));
        track.relationshipTarget = static_cast<int> (child.getProperty ("relationshipTarget", -1));
        for (const auto& lineNode : child)
        {
            if (! lineNode.hasType ("line")) continue;
            OrbitsTriggerLine line;
            line.id = lineNode.getProperty ("id", juce::Uuid().toString()).toString();
            line.start = { static_cast<float> (lineNode.getProperty ("x1", 0.25f)),
                           static_cast<float> (lineNode.getProperty ("y1", 0.5f)) };
            line.end = { static_cast<float> (lineNode.getProperty ("x2", 0.75f)),
                         static_cast<float> (lineNode.getProperty ("y2", 0.5f)) };
            line.sound.name = lineNode.getProperty ("name", "Sound").toString();
            line.sound.scCode = lineNode.getProperty ("code", starterCode (60)).toString();
            line.sound.pdPatch = lineNode.getProperty ("pdPatch", starterPdPatch (60)).toString();
            line.sound.playback = static_cast<OrbitsLane::PlaybackType> (juce::jlimit (0, 1, static_cast<int> (lineNode.getProperty ("playback", 0))));
            line.sound.durationSeconds = static_cast<float> (lineNode.getProperty ("duration", 1.0f));
            line.sound.probability = juce::jlimit (0.0f, 1.0f, static_cast<float> (lineNode.getProperty ("probability", 1.0f)));
            line.sound.gain = juce::jlimit (0.0f, 1.5f, static_cast<float> (lineNode.getProperty ("gain", 1.0f)));
            line.sound.pan = juce::jlimit (-1.0f, 1.0f, static_cast<float> (lineNode.getProperty ("pan", 0.0f)));
            line.sound.colourIndex = juce::jlimit (-1, 7, static_cast<int> (lineNode.getProperty ("lineColour", -1)));
            line.sound.enabled = static_cast<bool> (lineNode.getProperty ("enabled", true));
            track.lines.push_back (std::move (line));
        }
        document.tracks.push_back (std::move (track));
    }
    if (document.tracks.empty()) return createDefault();
    for (int i = 0; i < static_cast<int> (document.tracks.size()); ++i)
    {
        auto& track = document.tracks[static_cast<size_t> (i)];
        if (! juce::isPositiveAndBelow (track.relationshipTarget, static_cast<int> (document.tracks.size()))
            || track.relationshipTarget == i)
        {
            track.relationshipTarget = -1;
            track.relationshipAction = OrbitsTrack::RelationshipAction::none;
        }
    }
    document.refreshTriggerPhases();
    return document;
}

double OrbitsDocument::loopDurationSeconds (const OrbitsTrack& track) const
{
    const auto beatsPerBar = track.timeSigNumerator * (4.0 / juce::jmax (1, track.timeSigDenominator));
    return juce::jmax (0.01, beatsPerBar * track.loopBars * 60.0 / juce::jmax (1.0, effectiveBpm (track)));
}

double OrbitsDocument::effectiveBpm (const OrbitsTrack& track) const
{
    if (track.clockMode == OrbitsTrack::ClockMode::project) return projectTempoBpm;
    if (track.clockMode == OrbitsTrack::ClockMode::ratio) return projectTempoBpm * track.tempoRatio;
    return track.bpm;
}

std::vector<double> OrbitsDocument::phasesForLine (const OrbitsTrack& track, const OrbitsTriggerLine& line) const
{
    std::vector<double> intersections;
    const juce::Line<float> triggerLine (line.start, line.end);
    auto previous = spiralPoint (track, 0.0);
    constexpr int samples = 1800;
    for (int i = 1; i <= samples; ++i)
    {
        const auto phase = static_cast<double> (i) / samples;
        const auto current = spiralPoint (track, phase);
        juce::Point<float> hit;
        const juce::Line<float> segment (previous, current);
        if (segment.intersects (triggerLine, hit))
        {
            const auto fraction = segment.getLength() > 0.000001f
                                    ? hit.getDistanceFrom (previous) / segment.getLength() : 0.0f;
            const auto hitPhase = juce::jlimit (0.0, 0.999999,
                (static_cast<double> (i - 1) + fraction) / samples);
            const auto duplicate = std::any_of (intersections.begin(), intersections.end(), [hitPhase] (double existing)
            {
                return std::abs (existing - hitPhase) < 0.002;
            });
            if (! duplicate) intersections.push_back (hitPhase);
        }
        previous = current;
    }
    std::sort (intersections.begin(), intersections.end());
    return intersections;
}

void OrbitsDocument::refreshTriggerPhases()
{
    for (auto& track : tracks)
        for (auto& line : track.lines)
            line.triggerPhases = phasesForLine (track, line);
}

void OrbitsDocument::snapLineToMusicalGrid (const OrbitsTrack& track, OrbitsTriggerLine& line) const
{
    if (! snapToMusicalGrid) return;
    const auto phases = phasesForLine (track, line);
    if (phases.empty()) return;
    const auto beatsPerBar = track.timeSigNumerator * (4.0 / juce::jmax (1, track.timeSigDenominator));
    const auto divisions = juce::jmax (1, juce::roundToInt (beatsPerBar * track.loopBars * snapDivisionsPerBeat));
    const auto current = phases.front();
    const auto target = std::round (current * divisions) / divisions;
    const auto delta = spiralPoint (track, target) - spiralPoint (track, current);
    line.start += delta;
    line.end += delta;
}

class OrbitsEditorComponent::SpiralCanvas final : public juce::Component
{
public:
    OrbitsDocument* document = nullptr;
    int* selectedTrack = nullptr;
    int* selectedLine = nullptr;
    std::vector<double>* playheadPhases = nullptr;
    std::map<juce::String, std::pair<bool, double>>* triggerFeedback = nullptr;
    std::function<void()> onChanged;
    std::function<void()> onBeginEdit;
    std::function<void()> onUndo;
    std::function<void()> onRedo;
    std::function<void()> onSelectionChanged;
    std::function<void (int)> onLineCreated;

    SpiralCanvas() { setWantsKeyboardFocus (true); }

    void paint (juce::Graphics& graphics) override
    {
        graphics.fillAll (juce::Colour (0xff09130f));
        auto bounds = getLocalBounds().toFloat().reduced (24.0f);
        graphics.setColour (juce::Colour (0xff1a342b).withAlpha (0.64f));
        for (int i = 1; i < 12; ++i)
        {
            const auto x = bounds.getX() + bounds.getWidth() * static_cast<float> (i) / 12.0f;
            const auto y = bounds.getY() + bounds.getHeight() * static_cast<float> (i) / 12.0f;
            graphics.drawVerticalLine (juce::roundToInt (x), bounds.getY(), bounds.getBottom());
            graphics.drawHorizontalLine (juce::roundToInt (y), bounds.getX(), bounds.getRight());
        }
        if (document == nullptr) return;

        auto drawTrack = [&] (int trackIndex)
        {
            const auto& track = document->tracks[static_cast<size_t> (trackIndex)];
            if (track.hidden) return;
            const auto colour = trackColour (track.colourIndex);
            const auto active = selectedTrack != nullptr && *selectedTrack == trackIndex;
            juce::Path path;
            for (int i = 0; i <= 900; ++i)
            {
                auto point = toScreen (spiralPoint (track, static_cast<double> (i) / 900.0));
                if (i == 0) path.startNewSubPath (point); else path.lineTo (point);
            }
            const auto width = static_cast<float> (juce::jlimit (0.25, 4.0, track.thickness));
            graphics.setColour (colour.darker (0.58f).withAlpha (active ? 0.86f : 0.48f));
            graphics.strokePath (path, juce::PathStrokeType ((active ? 6.4f : 4.2f) * width,
                                                             juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded));
            graphics.setColour (colour.withAlpha (active ? 0.96f : 0.58f));
            graphics.strokePath (path, juce::PathStrokeType ((active ? 2.6f : 1.7f) * width,
                                                             juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded));

            const auto divisions = juce::jmax (1, static_cast<int> (std::floor (track.loopBars * track.timeSigNumerator * 4.0)));
            for (int division = 0; division < divisions; ++division)
            {
                if (division % 4 != 0) continue;
                const auto phase = static_cast<double> (division) / divisions;
                const auto point = toScreen (spiralPoint (track, phase));
                graphics.setColour (juce::Colours::white.withAlpha (active ? 0.58f : 0.26f));
                graphics.fillEllipse (point.x - 1.8f, point.y - 1.8f, 3.6f, 3.6f);
            }

            for (int lineIndex = 0; lineIndex < static_cast<int> (track.lines.size()); ++lineIndex)
            {
                const auto& line = track.lines[static_cast<size_t> (lineIndex)];
                const auto selected = active && selectedLine != nullptr && *selectedLine == lineIndex;
                const juce::Line<float> screenLine (toScreen (line.start), toScreen (line.end));
                const auto configuredColour = line.sound.colourIndex < 0 ? colour : trackColour (line.sound.colourIndex);
                auto lineColour = selected ? juce::Colours::white : configuredColour.brighter (0.28f);
                auto lineWidth = selected ? 3.0f : 1.8f;
                if (triggerFeedback != nullptr)
                    if (const auto feedback = triggerFeedback->find (line.id); feedback != triggerFeedback->end())
                    {
                        lineColour = feedback->second.first ? juce::Colour (0xff7cffbd) : juce::Colour (0xffff6b74);
                        lineWidth = 4.6f;
                    }
                graphics.setColour (lineColour.withAlpha (active ? 0.96f : 0.48f));
                graphics.drawLine (screenLine, lineWidth);
                for (const auto phase : line.triggerPhases)
                {
                    const auto point = toScreen (spiralPoint (track, phase));
                    graphics.setColour (juce::Colour (0xffffd45c).withAlpha (active ? 1.0f : 0.55f));
                    graphics.fillEllipse (point.x - 4.0f, point.y - 4.0f, 8.0f, 8.0f);
                }
                if (selected)
                {
                    graphics.setColour (juce::Colours::white);
                    for (auto point : { screenLine.getStart(), screenLine.getEnd() })
                    {
                        graphics.fillEllipse (point.x - 5.0f, point.y - 5.0f, 10.0f, 10.0f);
                        graphics.setColour (juce::Colour (0xff111916));
                        graphics.fillEllipse (point.x - 2.4f, point.y - 2.4f, 4.8f, 4.8f);
                        graphics.setColour (juce::Colours::white);
                    }
                }
            }

            if (playheadPhases != nullptr && juce::isPositiveAndBelow (trackIndex, static_cast<int> (playheadPhases->size())))
            {
                const auto point = toScreen (spiralPoint (track, (*playheadPhases)[static_cast<size_t> (trackIndex)]));
                graphics.setColour (colour.withAlpha (0.24f));
                graphics.fillEllipse (point.x - 13.0f, point.y - 13.0f, 26.0f, 26.0f);
                graphics.setColour (juce::Colours::white.withAlpha (track.muted ? 0.35f : 0.96f));
                graphics.fillEllipse (point.x - 5.0f, point.y - 5.0f, 10.0f, 10.0f);
            }
        };

        for (int i = 0; i < static_cast<int> (document->tracks.size()); ++i)
            if (selectedTrack == nullptr || i != *selectedTrack) drawTrack (i);
        if (selectedTrack != nullptr && juce::isPositiveAndBelow (*selectedTrack, static_cast<int> (document->tracks.size())))
            drawTrack (*selectedTrack);

        if (drawing)
        {
            graphics.setColour (juce::Colours::white.withAlpha (0.92f));
            graphics.drawLine ({ toScreen (drawStart), toScreen (drawEnd) }, 2.2f);
        }

    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        grabKeyboardFocus();
        if (document == nullptr || selectedTrack == nullptr || selectedLine == nullptr || document->tracks.empty()) return;
        const auto point = fromScreen (event.position);
        if (event.mods.isMiddleButtonDown() || event.mods.isCommandDown())
        {
            panning = true; lastMouse = event.position; return;
        }
        if (event.getNumberOfClicks() >= 2)
        {
            drawing = true; drawStart = drawEnd = point; return;
        }

        auto& currentTrack = document->tracks[static_cast<size_t> (*selectedTrack)];
        const auto tolerance = 9.0f / juce::jmax (1.0f, static_cast<float> (getWidth()) * viewScale);
        for (int i = static_cast<int> (currentTrack.lines.size()); --i >= 0;)
        {
            auto& candidate = currentTrack.lines[static_cast<size_t> (i)];
            const auto hitsLine = candidate.start.getDistanceFrom (point) <= tolerance
                               || candidate.end.getDistanceFrom (point) <= tolerance
                               || juce::Line<float> (candidate.start, candidate.end).findNearestPointTo (point).getDistanceFrom (point) <= tolerance;
            if (event.mods.isAltDown() && hitsLine)
            {
                if (onBeginEdit) onBeginEdit();
                auto duplicate = candidate;
                duplicate.id = juce::Uuid().toString();
                duplicate.sound.name = candidate.sound.name + " copy";
                currentTrack.lines.push_back (std::move (duplicate));
                *selectedLine = static_cast<int> (currentTrack.lines.size()) - 1;
                dragMode = 1; lastWorld = point; editCheckpointStarted = true;
                document->refreshTriggerPhases(); selectionChanged(); repaint(); return;
            }
            if (candidate.start.getDistanceFrom (point) <= tolerance) { *selectedLine = i; dragMode = 2; lastWorld = point; selectionChanged(); return; }
            if (candidate.end.getDistanceFrom (point) <= tolerance) { *selectedLine = i; dragMode = 3; lastWorld = point; selectionChanged(); return; }
            if (juce::Line<float> (candidate.start, candidate.end).findNearestPointTo (point).getDistanceFrom (point) <= tolerance)
            { *selectedLine = i; dragMode = 1; lastWorld = point; selectionChanged(); return; }
        }

        auto nearestTrack = *selectedTrack;
        auto nearestDistance = std::numeric_limits<float>::max();
        for (int trackIndex = 0; trackIndex < static_cast<int> (document->tracks.size()); ++trackIndex)
        {
            if (document->tracks[static_cast<size_t> (trackIndex)].hidden) continue;
            for (int i = 0; i <= 600; ++i)
            {
                const auto distance = spiralPoint (document->tracks[static_cast<size_t> (trackIndex)], static_cast<double> (i) / 600.0).getDistanceFrom (point);
                if (distance < nearestDistance) { nearestDistance = distance; nearestTrack = trackIndex; }
            }
        }
        if (nearestDistance < tolerance * 1.8f)
        {
            *selectedTrack = nearestTrack; *selectedLine = -1; selectionChanged(); repaint();
        }
        else { *selectedLine = -1; selectionChanged(); repaint(); }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (panning)
        {
            viewOffset += event.position - lastMouse; lastMouse = event.position; repaint(); return;
        }
        if (document == nullptr || selectedTrack == nullptr || selectedLine == nullptr) return;
        const auto point = fromScreen (event.position);
        if (drawing) { drawEnd = point; repaint(); return; }
        auto& track = document->tracks[static_cast<size_t> (*selectedTrack)];
        if (! juce::isPositiveAndBelow (*selectedLine, static_cast<int> (track.lines.size()))) return;
        if (dragMode != 0 && ! editCheckpointStarted)
        {
            if (onBeginEdit) onBeginEdit();
            editCheckpointStarted = true;
        }
        auto& line = track.lines[static_cast<size_t> (*selectedLine)];
        if (dragMode == 1) { const auto delta = point - lastWorld; line.start += delta; line.end += delta; }
        else if (dragMode == 2) line.start = point;
        else if (dragMode == 3) line.end = point;
        lastWorld = point;
        if (dragMode != 0) { document->refreshTriggerPhases(); repaint(); }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (panning) { panning = false; return; }
        if (document == nullptr || selectedTrack == nullptr || selectedLine == nullptr) return;
        if (drawing)
        {
            drawing = false;
            if (drawStart.getDistanceFrom (drawEnd) > 0.018f)
            {
                if (onBeginEdit) onBeginEdit();
                auto& track = document->tracks[static_cast<size_t> (*selectedTrack)];
                OrbitsTriggerLine line;
                line.id = juce::Uuid().toString();
                line.start = drawStart; line.end = drawEnd;
                line.sound.name = "Sound " + juce::String (track.lines.size() + 1);
                line.sound.scCode = starterCode (60 + (*selectedTrack * 5 + static_cast<int> (track.lines.size()) * 3) % 24);
                line.sound.pdPatch = starterPdPatch (60 + (*selectedTrack * 5 + static_cast<int> (track.lines.size()) * 3) % 24);
                track.lines.push_back (std::move (line));
                *selectedLine = static_cast<int> (track.lines.size()) - 1;
                document->snapLineToMusicalGrid (track, track.lines.back());
                document->refreshTriggerPhases();
                if (onChanged) onChanged();
                selectionChanged();
                if (onLineCreated) onLineCreated (*selectedLine);
            }
            repaint(); return;
        }
        if (dragMode != 0)
        {
            auto& currentTrack = document->tracks[static_cast<size_t> (*selectedTrack)];
            if (juce::isPositiveAndBelow (*selectedLine, static_cast<int> (currentTrack.lines.size())))
                document->snapLineToMusicalGrid (currentTrack, currentTrack.lines[static_cast<size_t> (*selectedLine)]);
            dragMode = 0; document->refreshTriggerPhases(); if (onChanged) onChanged(); repaint();
        }
        editCheckpointStarted = false;
    }

    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        const auto oldScale = viewScale;
        viewScale = juce::jlimit (0.35f, 4.0f, viewScale * (wheel.deltaY > 0.0f ? 1.1f : 0.9f));
        const auto before = (event.position - viewOffset) / oldScale;
        viewOffset = event.position - before * viewScale;
        repaint();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Z')
        {
            if (key.getModifiers().isShiftDown()) { if (onRedo) onRedo(); }
            else if (onUndo) onUndo();
            return true;
        }
        if (key != juce::KeyPress::backspaceKey && key != juce::KeyPress::deleteKey) return false;
        if (document == nullptr || selectedTrack == nullptr || selectedLine == nullptr) return false;
        auto& lines = document->tracks[static_cast<size_t> (*selectedTrack)].lines;
        if (! juce::isPositiveAndBelow (*selectedLine, static_cast<int> (lines.size()))) return false;
        if (onBeginEdit) onBeginEdit();
        lines.erase (lines.begin() + *selectedLine); *selectedLine = -1;
        document->refreshTriggerPhases(); if (onChanged) onChanged(); selectionChanged(); repaint(); return true;
    }

private:
    bool drawing = false, panning = false, editCheckpointStarted = false;
    int dragMode = 0;
    float viewScale = 1.0f;
    juce::Point<float> viewOffset, lastMouse, lastWorld, drawStart, drawEnd;

    juce::Point<float> toScreen (juce::Point<float> point) const
    {
        auto bounds = getLocalBounds().toFloat().reduced (24.0f);
        auto mapped = juce::Point<float> (bounds.getX() + point.x * bounds.getWidth(), bounds.getY() + point.y * bounds.getHeight());
        return mapped * viewScale + viewOffset;
    }

    juce::Point<float> fromScreen (juce::Point<float> point) const
    {
        auto bounds = getLocalBounds().toFloat().reduced (24.0f);
        const auto unscaled = (point - viewOffset) / viewScale;
        return { (unscaled.x - bounds.getX()) / juce::jmax (1.0f, bounds.getWidth()),
                 (unscaled.y - bounds.getY()) / juce::jmax (1.0f, bounds.getHeight()) };
    }

    void selectionChanged() { if (onSelectionChanged) onSelectionChanged(); }
};

OrbitsEditorComponent::OrbitsEditorComponent (OrbitsDocument initialDocument, Commit commitCallback,
                                              EditSc editCallback, EditPd editPdCallback, Audition auditionCallback)
    : document (std::move (initialDocument)), commit (std::move (commitCallback)),
      editSc (std::move (editCallback)), editPd (std::move (editPdCallback)), audition (std::move (auditionCallback)),
      canvas (std::make_unique<SpiralCanvas>())
{
    if (document.tracks.empty()) document = OrbitsDocument::createDefault();
    document.refreshTriggerPhases();
    configure();
    refresh();
    startTimerHz (60);
}

OrbitsEditorComponent::~OrbitsEditorComponent() { stopTimer(); }

void OrbitsEditorComponent::configureSlider (juce::Slider& slider, double minimum, double maximum, double interval)
{
    inspectorContent.addAndMakeVisible (slider);
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 24);
    slider.setRange (minimum, maximum, interval);
    slider.onDragStart = [this] { if (! refreshing) { pushUndoState(); sliderGestureActive = true; } };
    slider.onDragEnd = [this] { sliderGestureActive = false; };
    slider.onValueChange = [this]
    {
        if (refreshing) return;
        if (! sliderGestureActive) pushUndoState();
        if (auto* current = track())
        {
            current->bpm = bpmSlider.getValue();
            current->timeSigNumerator = juce::roundToInt (numeratorSlider.getValue());
            current->timeSigDenominator = juce::roundToInt (denominatorSlider.getValue());
            current->loopBars = barsSlider.getValue();
            current->thickness = thicknessSlider.getValue();
            current->orbitWarpAmount = warpSlider.getValue();
            current->spiralTwistAmount = twistSlider.getValue();
            current->phaseOffsetDegrees = phaseSlider.getValue();
            current->xRotationDegrees = xRotationSlider.getValue();
            current->yRotationDegrees = yRotationSlider.getValue();
            current->xOffset = xOffsetSlider.getValue();
            current->yOffset = yOffsetSlider.getValue();
            current->tempoRatio = ratioSlider.getValue();
            current->gain = static_cast<float> (trackGainSlider.getValue());
            current->pan = static_cast<float> (trackPanSlider.getValue());
            if (auto* selected = line())
            {
                selected->sound.durationSeconds = static_cast<float> (durationSlider.getValue());
                selected->sound.probability = static_cast<float> (probabilitySlider.getValue());
                selected->sound.gain = static_cast<float> (gainSlider.getValue());
                selected->sound.pan = static_cast<float> (panSlider.getValue());
            }
            changed();
        }
    };
}

void OrbitsEditorComponent::configure()
{
    addAndMakeVisible (*canvas);
    addAndMakeVisible (inspectorViewport);
    inspectorViewport.setViewedComponent (&inspectorContent, false);
    inspectorViewport.setScrollBarsShown (true, false);
    inspectorViewport.setScrollBarThickness (7);
    inspectorViewport.setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff2997ff).withAlpha (0.72f));
    canvas->document = &document;
    canvas->selectedTrack = &selectedTrack;
    canvas->selectedLine = &selectedLine;
    canvas->playheadPhases = &previewPhases;
    canvas->triggerFeedback = &triggerFeedback;
    canvas->onChanged = [this] { changed (false); };
    canvas->onBeginEdit = [this] { pushUndoState(); };
    canvas->onUndo = [this] { undo(); };
    canvas->onRedo = [this] { redo(); };
    canvas->onSelectionChanged = [this]
    {
        if (line() != nullptr) inspectorTab = InspectorTab::sound;
        refresh();
    };
    canvas->onLineCreated = [this] (int)
    {
        inspectorTab = InspectorTab::sound;
        refresh();
        editSelectedLine();
    };

    for (auto* button : { &addTrackButton, &removeTrackButton, &playButton, &stopButton, &undoButton, &redoButton }) addAndMakeVisible (*button);
    for (auto* button : { &editSoundButton, &auditionButton, &deleteLineButton, &distributeButton, &rotateButton, &repeatButton,
                          &reverseButton, &randomiseButton, &euclideanButton }) inspectorContent.addAndMakeVisible (*button);
    for (auto* button : { &trackTabButton, &shapeTabButton, &soundTabButton }) addAndMakeVisible (*button);
    inspectorContent.addAndMakeVisible (trackList);
    trackList.setRowHeight (36);
    trackList.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff111916));
    trackList.setColour (juce::ListBox::outlineColourId, juce::Colour (0xff2a3a34));
    trackList.setOutlineThickness (1);
    for (auto* combo : { &clockModeBox, &relationshipActionBox, &relationshipTargetBox, &outputRouteBox, &snapDivisionBox, &playbackBox, &lineColourBox }) inspectorContent.addAndMakeVisible (*combo);
    inspectorContent.addAndMakeVisible (lineNameEditor);
    for (auto* toggle : { &hideButton, &muteButton, &soloButton, &resetPhaseButton, &snapButton, &lineEnabledButton }) inspectorContent.addAndMakeVisible (*toggle);
    for (auto* label : { &trackLabel, &bpmLabel, &meterLabel, &barsLabel, &shapeLabel, &soundLineLabel,
                         &numeratorLabel, &denominatorLabel, &thicknessLabel, &warpLabel,
                         &twistLabel, &phaseLabel, &xRotationLabel, &yRotationLabel,
                         &xOffsetLabel, &yOffsetLabel, &clockModeLabel, &ratioLabel, &mixLabel, &trackGainLabel, &trackPanLabel, &outputRouteLabel,
                         &relationshipLabel, &relationshipTargetLabel, &snapLabel, &patternLabel, &patternStepsLabel, &patternPulsesLabel,
                         &lineNameLabel, &playbackLabel, &lineColourLabel, &durationLabel, &probabilityLabel,
                         &gainLabel, &panLabel }) inspectorContent.addAndMakeVisible (*label);
    trackLabel.setText ("TRACK", juce::dontSendNotification);
    bpmLabel.setText ("TEMPO", juce::dontSendNotification);
    meterLabel.setText ("TIME SIGNATURE", juce::dontSendNotification);
    barsLabel.setText ("LOOP LENGTH", juce::dontSendNotification);
    shapeLabel.setText ("SPIRAL SHAPE", juce::dontSendNotification);
    soundLineLabel.setText ("SELECTED SOUND LINE", juce::dontSendNotification);
    numeratorLabel.setText ("Beats", juce::dontSendNotification);
    denominatorLabel.setText ("Note value", juce::dontSendNotification);
    thicknessLabel.setText ("Thickness", juce::dontSendNotification);
    warpLabel.setText ("Warp", juce::dontSendNotification);
    twistLabel.setText ("Twist", juce::dontSendNotification);
    phaseLabel.setText ("Phase", juce::dontSendNotification);
    xRotationLabel.setText ("X rotation", juce::dontSendNotification);
    yRotationLabel.setText ("Y rotation", juce::dontSendNotification);
    xOffsetLabel.setText ("X offset", juce::dontSendNotification);
    yOffsetLabel.setText ("Y offset", juce::dontSendNotification);
    clockModeLabel.setText ("CLOCK", juce::dontSendNotification);
    ratioLabel.setText ("Tempo ratio", juce::dontSendNotification);
    mixLabel.setText ("MIX", juce::dontSendNotification);
    trackGainLabel.setText ("Gain", juce::dontSendNotification);
    trackPanLabel.setText ("Pan", juce::dontSendNotification);
    outputRouteLabel.setText ("Output", juce::dontSendNotification);
    relationshipLabel.setText ("TRACK RELATIONSHIP", juce::dontSendNotification);
    relationshipTargetLabel.setText ("Target", juce::dontSendNotification);
    snapLabel.setText ("SNAPPING", juce::dontSendNotification);
    patternLabel.setText ("PATTERN TOOLS", juce::dontSendNotification);
    patternStepsLabel.setText ("Steps", juce::dontSendNotification);
    patternPulsesLabel.setText ("Pulses", juce::dontSendNotification);
    lineNameLabel.setText ("Name", juce::dontSendNotification);
    playbackLabel.setText ("Engine", juce::dontSendNotification);
    lineColourLabel.setText ("Colour", juce::dontSendNotification);
    durationLabel.setText ("Duration", juce::dontSendNotification);
    probabilityLabel.setText ("Probability", juce::dontSendNotification);
    gainLabel.setText ("Gain", juce::dontSendNotification);
    panLabel.setText ("Pan", juce::dontSendNotification);

    for (auto* label : { &trackLabel, &bpmLabel, &meterLabel, &barsLabel, &shapeLabel, &soundLineLabel, &clockModeLabel,
                         &mixLabel, &relationshipLabel, &snapLabel, &patternLabel })
    {
        label->setFont (juce::FontOptions (12.0f, juce::Font::bold));
        label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.62f));
    }
    for (auto* label : { &numeratorLabel, &denominatorLabel, &thicknessLabel, &warpLabel,
                         &twistLabel, &phaseLabel, &xRotationLabel, &yRotationLabel,
                         &xOffsetLabel, &yOffsetLabel, &ratioLabel, &trackGainLabel, &trackPanLabel, &outputRouteLabel,
                         &relationshipTargetLabel, &patternStepsLabel, &patternPulsesLabel, &lineNameLabel, &playbackLabel, &lineColourLabel,
                         &durationLabel, &probabilityLabel, &gainLabel, &panLabel })
    {
        label->setFont (juce::FontOptions (11.0f));
        label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.48f));
        label->setJustificationType (juce::Justification::centredLeft);
    }

    configureSlider (bpmSlider, 20.0, 320.0, 0.1);
    configureSlider (numeratorSlider, 1.0, 15.0, 1.0);
    configureSlider (denominatorSlider, 1.0, 16.0, 1.0);
    configureSlider (barsSlider, 0.25, 64.0, 0.25);
    configureSlider (thicknessSlider, 0.25, 4.0, 0.05);
    configureSlider (warpSlider, 0.0, 1.0, 0.01);
    configureSlider (twistSlider, -1.0, 1.0, 0.01);
    configureSlider (phaseSlider, 0.0, 360.0, 1.0);
    configureSlider (xRotationSlider, -85.0, 85.0, 1.0);
    configureSlider (yRotationSlider, -85.0, 85.0, 1.0);
    configureSlider (xOffsetSlider, -0.5, 0.5, 0.005);
    configureSlider (yOffsetSlider, -0.5, 0.5, 0.005);
    configureSlider (ratioSlider, 0.125, 8.0, 0.125);
    configureSlider (trackGainSlider, 0.0, 1.5, 0.01);
    configureSlider (trackPanSlider, -1.0, 1.0, 0.01);
    configureSlider (patternStepsSlider, 2.0, 32.0, 1.0);
    configureSlider (patternPulsesSlider, 1.0, 32.0, 1.0);
    patternStepsSlider.setValue (8.0, juce::dontSendNotification);
    patternPulsesSlider.setValue (4.0, juce::dontSendNotification);
    patternStepsSlider.onValueChange = [this]
    {
        patternPulsesSlider.setRange (1.0, patternStepsSlider.getValue(), 1.0);
        patternPulsesSlider.setValue (juce::jmin (patternPulsesSlider.getValue(), patternStepsSlider.getValue()), juce::dontSendNotification);
    };
    patternPulsesSlider.onValueChange = [] {};
    configureSlider (durationSlider, -1.0, 30.0, 0.05);
    configureSlider (probabilitySlider, 0.0, 1.0, 0.01);
    configureSlider (gainSlider, 0.0, 1.5, 0.01);
    configureSlider (panSlider, -1.0, 1.0, 0.01);

    clockModeBox.addItemList ({ "Free", "Project tempo", "Tempo ratio" }, 1);
    relationshipActionBox.addItemList ({ "None", "Reset target", "Start target", "Stop target", "Phase-lock target" }, 1);
    outputRouteBox.addItemList ({ "Stereo", "Left", "Right" }, 1);
    snapDivisionBox.addItemList ({ "1 per beat", "2 per beat", "4 per beat", "8 per beat", "16 per beat" }, 1);
    playbackBox.addItemList ({ "SuperCollider", "Pure Data (Pd)" }, 1);
    lineColourBox.addItemList ({ "Track colour", "Aqua", "Pink", "Gold", "Violet", "Mint", "Coral", "Blue", "Lime" }, 1);

    for (auto* button : { &trackTabButton, &shapeTabButton, &soundTabButton })
    {
        button->setClickingTogglesState (false);
        button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff202a27));
        button->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff1688f8));
        button->setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.72f));
        button->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    }
    trackTabButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    shapeTabButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    soundTabButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    trackTabButton.setTooltip ("Track timing and playback settings");
    shapeTabButton.setTooltip ("Spiral shape and snapping settings");
    soundTabButton.setTooltip ("Selected sound line settings");
    trackTabButton.onClick = [this] { setInspectorTab (InspectorTab::track); };
    shapeTabButton.onClick = [this] { setInspectorTab (InspectorTab::shape); };
    soundTabButton.onClick = [this] { setInspectorTab (InspectorTab::sound); };

    addTrackButton.onClick = [this] { addTrack(); };
    removeTrackButton.onClick = [this] { removeTrack(); };
    addTrackButton.setTooltip ("Add spiral track");
    removeTrackButton.setTooltip ("Remove selected track");
    playButton.setTooltip ("Preview all spiral tracks");
    stopButton.setTooltip ("Stop preview and return playheads to the start");
    editSoundButton.setTooltip ("Open this line in the SuperCollider editor");
    auditionButton.setTooltip ("Preview this sound line");
    deleteLineButton.setTooltip ("Delete the selected sound line");
    undoButton.setTooltip ("Undo the last Orbits edit");
    redoButton.setTooltip ("Redo the last Orbits edit");
    distributeButton.setTooltip ("Space every sound line evenly around this spiral");
    rotateButton.setTooltip ("Rotate the pattern forward by one step");
    repeatButton.setTooltip ("Repeat the selected sound line across the chosen steps");
    reverseButton.setTooltip ("Reverse the order of sound lines around the spiral");
    randomiseButton.setTooltip ("Randomise all sound-line positions");
    euclideanButton.setTooltip ("Fill the chosen steps with an even Euclidean rhythm using the selected sound");
    relationshipActionBox.onChange = [this]
    {
        if (refreshing) return;
        if (auto* current = track()) { pushUndoState(); current->relationshipAction = static_cast<OrbitsTrack::RelationshipAction> (relationshipActionBox.getSelectedItemIndex()); changed (false); refresh(); }
    };
    relationshipTargetBox.onChange = [this]
    {
        if (refreshing) return;
        if (auto* current = track()) { pushUndoState(); current->relationshipTarget = relationshipTargetBox.getSelectedId() - 1; changed (false); refresh(); }
    };
    hideButton.onClick = [this] { if (auto* current = track()) { pushUndoState(); current->hidden = hideButton.getToggleState(); changed (false); refresh(); } };
    muteButton.onClick = [this] { if (auto* current = track()) { pushUndoState(); current->muted = muteButton.getToggleState(); changed (false); refresh(); } };
    soloButton.onClick = [this] { if (auto* current = track()) { pushUndoState(); current->solo = soloButton.getToggleState(); changed (false); refresh(); } };
    outputRouteBox.onChange = [this]
    {
        if (refreshing) return;
        if (auto* current = track()) { pushUndoState(); current->outputRoute = static_cast<OrbitsTrack::OutputRoute> (outputRouteBox.getSelectedItemIndex()); changed (false); refresh(); }
    };
    distributeButton.onClick = [this] { applyPattern (0); };
    rotateButton.onClick = [this] { applyPattern (1); };
    repeatButton.onClick = [this] { applyPattern (2); };
    reverseButton.onClick = [this] { applyPattern (3); };
    randomiseButton.onClick = [this] { applyPattern (4); };
    euclideanButton.onClick = [this] { applyPattern (5); };
    resetPhaseButton.onClick = [this] { if (auto* current = track()) { pushUndoState(); current->resetPhaseOnStart = resetPhaseButton.getToggleState(); changed (false); refresh(); } };
    clockModeBox.onChange = [this]
    {
        if (refreshing) return;
        if (auto* current = track()) { pushUndoState(); current->clockMode = static_cast<OrbitsTrack::ClockMode> (clockModeBox.getSelectedItemIndex()); changed(); refresh(); }
    };
    snapButton.onClick = [this] { pushUndoState(); document.snapToMusicalGrid = snapButton.getToggleState(); changed(); refresh(); };
    snapDivisionBox.onChange = [this]
    {
        if (refreshing) return;
        static constexpr int divisions[] { 1, 2, 4, 8, 16 };
        pushUndoState(); document.snapDivisionsPerBeat = divisions[juce::jlimit (0, 4, snapDivisionBox.getSelectedItemIndex())]; changed(); refresh();
    };
    playbackBox.onChange = [this]
    {
        if (refreshing) return;
        if (auto* selected = line()) { pushUndoState(); selected->sound.playback = static_cast<OrbitsLane::PlaybackType> (playbackBox.getSelectedItemIndex()); changed (false); refresh(); }
    };
    lineColourBox.onChange = [this]
    {
        if (refreshing) return;
        if (auto* selected = line()) { pushUndoState(); selected->sound.colourIndex = lineColourBox.getSelectedItemIndex() - 1; changed (false); refresh(); }
    };
    lineEnabledButton.onClick = [this] { if (auto* selected = line()) { pushUndoState(); selected->sound.enabled = lineEnabledButton.getToggleState(); changed (false); refresh(); } };
    lineNameEditor.onReturnKey = [this]
    {
        if (auto* selected = line(); selected != nullptr && selected->sound.name != lineNameEditor.getText()) { pushUndoState(); selected->sound.name = lineNameEditor.getText(); changed (false); refresh(); }
    };
    lineNameEditor.onFocusLost = lineNameEditor.onReturnKey;
    editSoundButton.onClick = [this] { editSelectedLine(); };
    auditionButton.onClick = [this] { if (auto* selected = line(); selected != nullptr && audition) audition (selected->sound); };
    deleteLineButton.onClick = [this]
    {
        if (auto* current = track(); current != nullptr && juce::isPositiveAndBelow (selectedLine, static_cast<int> (current->lines.size())))
        {
            pushUndoState();
            current->lines.erase (current->lines.begin() + selectedLine); selectedLine = -1; changed(); refresh();
        }
    };
    playButton.onClick = [this]
    {
        previewRunning = ! previewRunning;
        if (previewRunning) { lastPreviewTime = juce::Time::getMillisecondCounterHiRes() * 0.001; }
        playButton.setButtonText (previewRunning ? "Pause" : "Play");
    };
    stopButton.onClick = [this]
    {
        previewRunning = false; previewSeconds = 0.0; std::fill (previewPhases.begin(), previewPhases.end(), 0.0);
        previewTrackPositions.assign (document.tracks.size(), 0.0);
        previewTrackRunning.assign (document.tracks.size(), true);
        playButton.setButtonText ("Play"); canvas->repaint();
    };
    undoButton.onClick = [this] { undo(); };
    redoButton.onClick = [this] { redo(); };
}

OrbitsTrack* OrbitsEditorComponent::track()
{
    return juce::isPositiveAndBelow (selectedTrack, static_cast<int> (document.tracks.size()))
           ? &document.tracks[static_cast<size_t> (selectedTrack)] : nullptr;
}

OrbitsTriggerLine* OrbitsEditorComponent::line()
{
    auto* current = track();
    return current != nullptr && juce::isPositiveAndBelow (selectedLine, static_cast<int> (current->lines.size()))
           ? &current->lines[static_cast<size_t> (selectedLine)] : nullptr;
}

void OrbitsEditorComponent::addTrack()
{
    pushUndoState();
    OrbitsTrack newTrack;
    newTrack.name = "Track " + juce::String (document.tracks.size() + 1);
    newTrack.colourIndex = static_cast<int> (document.tracks.size());
    newTrack.bpm = 120.0 + static_cast<int> (document.tracks.size()) * 6.0;
    newTrack.loopBars = 2.0 + static_cast<int> (document.tracks.size()) % 3;
    newTrack.phaseOffsetDegrees = std::fmod (static_cast<double> (document.tracks.size()) * 83.0, 360.0);
    document.tracks.push_back (std::move (newTrack));
    selectedTrack = static_cast<int> (document.tracks.size()) - 1; selectedLine = -1;
    changed(); refresh();
}

void OrbitsEditorComponent::removeTrack()
{
    if (document.tracks.size() <= 1) return;
    pushUndoState();
    const auto removed = selectedTrack;
    document.tracks.erase (document.tracks.begin() + selectedTrack);
    for (auto& item : document.tracks)
    {
        if (item.relationshipTarget == removed) { item.relationshipTarget = -1; item.relationshipAction = OrbitsTrack::RelationshipAction::none; }
        else if (item.relationshipTarget > removed) --item.relationshipTarget;
    }
    selectedTrack = juce::jlimit (0, static_cast<int> (document.tracks.size()) - 1, selectedTrack);
    selectedLine = -1; changed(); refresh();
}

void OrbitsEditorComponent::applyPattern (int operation)
{
    auto* current = track();
    if (current == nullptr || current->lines.empty()) return;
    const auto steps = juce::jlimit (2, 32, juce::roundToInt (patternStepsSlider.getValue()));
    const auto pulses = juce::jlimit (1, steps, juce::roundToInt (patternPulsesSlider.getValue()));
    if ((operation == 2 || operation == 5)
        && ! juce::isPositiveAndBelow (selectedLine, static_cast<int> (current->lines.size()))) return;

    pushUndoState();
    auto placeAtPhase = [current] (OrbitsTriggerLine& item, double phase)
    {
        phase -= std::floor (phase);
        const auto point = spiralPoint (*current, phase);
        auto tangent = spiralPoint (*current, phase + 0.001) - spiralPoint (*current, phase - 0.001);
        if (tangent.getDistanceFromOrigin() < 0.0001f) tangent = { 1.0f, 0.0f };
        tangent /= tangent.getDistanceFromOrigin();
        const juce::Point<float> normal { -tangent.y, tangent.x };
        const auto halfLength = juce::jlimit (0.025f, 0.16f, item.start.getDistanceFrom (item.end) * 0.5f);
        item.start = point - normal * halfLength;
        item.end = point + normal * halfLength;
    };

    if (operation == 2 || operation == 5)
    {
        const auto source = current->lines[static_cast<size_t> (selectedLine)];
        std::vector<OrbitsTriggerLine> generated;
        for (int step = 0; step < steps; ++step)
        {
            const auto include = operation == 2 || ((step * pulses) % steps < pulses);
            if (! include) continue;
            auto copy = source;
            copy.id = juce::Uuid().toString();
            copy.sound.name = source.sound.name + " " + juce::String (generated.size() + 1);
            placeAtPhase (copy, static_cast<double> (step) / steps);
            generated.push_back (std::move (copy));
        }
        current->lines = std::move (generated);
        selectedLine = current->lines.empty() ? -1 : 0;
    }
    else
    {
        document.refreshTriggerPhases();
        std::vector<std::pair<double, OrbitsTriggerLine*>> ordered;
        ordered.reserve (current->lines.size());
        for (size_t i = 0; i < current->lines.size(); ++i)
            ordered.push_back ({ current->lines[i].triggerPhases.empty() ? static_cast<double> (i) / static_cast<double> (current->lines.size())
                                                                        : current->lines[i].triggerPhases.front(),
                                 &current->lines[i] });
        std::sort (ordered.begin(), ordered.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
        for (size_t i = 0; i < ordered.size(); ++i)
        {
            auto phase = ordered[i].first;
            if (operation == 0) phase = static_cast<double> (i) / static_cast<double> (ordered.size());
            else if (operation == 1) phase += 1.0 / steps;
            else if (operation == 3) phase = 1.0 - phase;
            else if (operation == 4) phase = juce::Random::getSystemRandom().nextDouble();
            placeAtPhase (*ordered[i].second, phase);
        }
    }
    document.refreshTriggerPhases();
    changed (false); refresh();
}

int OrbitsEditorComponent::getNumRows() { return static_cast<int> (document.tracks.size()); }

void OrbitsEditorComponent::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (document.tracks.size()))) return;
    const auto& item = document.tracks[static_cast<size_t> (row)];
    if (selected) { g.setColour (juce::Colour (0xff1688f8).withAlpha (0.28f)); g.fillRect (0, 0, width, height); }
    g.setColour (trackColour (item.colourIndex));
    g.fillEllipse (10.0f, static_cast<float> (height) * 0.5f - 4.0f, 8.0f, 8.0f);
    g.setColour (juce::Colours::white.withAlpha (item.hidden ? 0.35f : 0.9f));
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText (item.name, 26, 2, width - 150, height - 4, juce::Justification::centredLeft);
    auto detail = juce::String (document.effectiveBpm (item), 0) + " BPM  "
                + juce::String (item.timeSigNumerator) + "/" + juce::String (item.timeSigDenominator)
                + "  " + juce::String (item.lines.size()) + " sounds";
    if (item.muted) detail = "MUTED  " + detail;
    g.setColour (juce::Colours::white.withAlpha (0.48f));
    g.setFont (juce::FontOptions (10.5f));
    g.drawText (detail, juce::jmax (100, width - 205), 2, juce::jmin (195, width - 105), height - 4, juce::Justification::centredRight);
}

void OrbitsEditorComponent::selectedRowsChanged (int row)
{
    if (refreshing || ! juce::isPositiveAndBelow (row, static_cast<int> (document.tracks.size()))) return;
    selectedTrack = row; selectedLine = -1; refresh();
}

void OrbitsEditorComponent::editSelectedLine()
{
    auto* selected = line();
    if (selected == nullptr) return;
    const auto lineId = selected->id;
    const auto playback = selected->sound.playback;
    const auto safeThis = juce::Component::SafePointer<OrbitsEditorComponent> (this);
    auto save = [safeThis, lineId, playback] (juce::String code, float duration)
    {
        if (safeThis == nullptr) return;
        for (auto& track : safeThis->document.tracks)
            for (auto& candidate : track.lines)
                if (candidate.id == lineId)
                {
                    safeThis->pushUndoState();
                    if (playback == OrbitsLane::PlaybackType::pureData) candidate.sound.pdPatch = std::move (code);
                    else candidate.sound.scCode = std::move (code);
                    candidate.sound.durationSeconds = duration;
                    safeThis->changed (false); safeThis->refresh();
                    return;
                }
    };
    if (selected->sound.playback == OrbitsLane::PlaybackType::pureData)
    {
        if (editPd) editPd (selected->sound.pdPatch, selected->sound.durationSeconds, std::move (save));
    }
    else if (editSc)
    {
        editSc (selected->sound.scCode, selected->sound.durationSeconds, std::move (save));
    }
}

void OrbitsEditorComponent::pushUndoState()
{
    if (restoringHistory) return;
    if (undoHistory.empty() || ! undoHistory.back().toValueTree().isEquivalentTo (document.toValueTree()))
        undoHistory.push_back (document);
    if (undoHistory.size() > 100) undoHistory.erase (undoHistory.begin());
    redoHistory.clear();
    undoButton.setEnabled (! undoHistory.empty());
    redoButton.setEnabled (false);
}

void OrbitsEditorComponent::restoreHistory (const OrbitsDocument& state)
{
    const juce::ScopedValueSetter<bool> guard (restoringHistory, true);
    document = state;
    document.refreshTriggerPhases();
    selectedTrack = juce::jlimit (0, static_cast<int> (document.tracks.size()) - 1, selectedTrack);
    selectedLine = -1;
    changed (false);
    refresh();
}

void OrbitsEditorComponent::undo()
{
    if (undoHistory.empty()) return;
    redoHistory.push_back (document);
    const auto state = undoHistory.back(); undoHistory.pop_back(); restoreHistory (state);
}

void OrbitsEditorComponent::redo()
{
    if (redoHistory.empty()) return;
    undoHistory.push_back (document);
    const auto state = redoHistory.back(); redoHistory.pop_back(); restoreHistory (state);
}

void OrbitsEditorComponent::changed (bool recomputeIntersections)
{
    if (recomputeIntersections) document.refreshTriggerPhases();
    if (commit) commit (document);
    previewPhases.resize (document.tracks.size(), 0.0);
    canvas->repaint();
}

void OrbitsEditorComponent::refresh()
{
    const juce::ScopedValueSetter<bool> guard (refreshing, true);
    selectedTrack = juce::jlimit (0, static_cast<int> (document.tracks.size()) - 1, selectedTrack);
    trackList.updateContent();
    trackList.selectRow (selectedTrack, false, false);
    previewPhases.resize (document.tracks.size(), 0.0);
    if (auto* current = track())
    {
        bpmSlider.setValue (current->bpm, juce::dontSendNotification);
        numeratorSlider.setValue (current->timeSigNumerator, juce::dontSendNotification);
        denominatorSlider.setValue (current->timeSigDenominator, juce::dontSendNotification);
        barsSlider.setValue (current->loopBars, juce::dontSendNotification);
        thicknessSlider.setValue (current->thickness, juce::dontSendNotification);
        warpSlider.setValue (current->orbitWarpAmount, juce::dontSendNotification);
        twistSlider.setValue (current->spiralTwistAmount, juce::dontSendNotification);
        phaseSlider.setValue (current->phaseOffsetDegrees, juce::dontSendNotification);
        xRotationSlider.setValue (current->xRotationDegrees, juce::dontSendNotification);
        yRotationSlider.setValue (current->yRotationDegrees, juce::dontSendNotification);
        xOffsetSlider.setValue (current->xOffset, juce::dontSendNotification);
        yOffsetSlider.setValue (current->yOffset, juce::dontSendNotification);
        ratioSlider.setValue (current->tempoRatio, juce::dontSendNotification);
        trackGainSlider.setValue (current->gain, juce::dontSendNotification);
        trackPanSlider.setValue (current->pan, juce::dontSendNotification);
        outputRouteBox.setSelectedItemIndex (static_cast<int> (current->outputRoute), juce::dontSendNotification);
        clockModeBox.setSelectedItemIndex (static_cast<int> (current->clockMode), juce::dontSendNotification);
        resetPhaseButton.setToggleState (current->resetPhaseOnStart, juce::dontSendNotification);
        relationshipActionBox.setSelectedItemIndex (static_cast<int> (current->relationshipAction), juce::dontSendNotification);
        relationshipTargetBox.clear (juce::dontSendNotification);
        for (int i = 0; i < static_cast<int> (document.tracks.size()); ++i)
            if (i != selectedTrack) relationshipTargetBox.addItem (document.tracks[static_cast<size_t> (i)].name, i + 1);
        relationshipTargetBox.setSelectedId (current->relationshipTarget + 1, juce::dontSendNotification);
        const auto hasRelationship = current->relationshipAction != OrbitsTrack::RelationshipAction::none;
        relationshipTargetBox.setEnabled (hasRelationship);
        bpmSlider.setEnabled (current->clockMode == OrbitsTrack::ClockMode::free);
        ratioSlider.setEnabled (current->clockMode == OrbitsTrack::ClockMode::ratio);
        hideButton.setToggleState (current->hidden, juce::dontSendNotification);
        muteButton.setToggleState (current->muted, juce::dontSendNotification);
        soloButton.setToggleState (current->solo, juce::dontSendNotification);
    }
    const auto hasLine = line() != nullptr;
    snapButton.setToggleState (document.snapToMusicalGrid, juce::dontSendNotification);
    const auto snapIndex = document.snapDivisionsPerBeat <= 1 ? 0 : document.snapDivisionsPerBeat <= 2 ? 1
                         : document.snapDivisionsPerBeat <= 4 ? 2 : document.snapDivisionsPerBeat <= 8 ? 3 : 4;
    snapDivisionBox.setSelectedItemIndex (snapIndex, juce::dontSendNotification);
    soundLineLabel.setText (hasLine ? "SOUND LINE  /  " + line()->sound.name.toUpperCase()
                                    : "SOUND LINE  /  NONE SELECTED",
                                juce::dontSendNotification);
    if (auto* selected = line())
    {
        lineNameEditor.setText (selected->sound.name, false);
        playbackBox.setSelectedItemIndex (static_cast<int> (selected->sound.playback), juce::dontSendNotification);
        lineColourBox.setSelectedItemIndex (selected->sound.colourIndex + 1, juce::dontSendNotification);
        lineEnabledButton.setToggleState (selected->sound.enabled, juce::dontSendNotification);
        durationSlider.setValue (selected->sound.durationSeconds, juce::dontSendNotification);
        probabilitySlider.setValue (selected->sound.probability, juce::dontSendNotification);
        gainSlider.setValue (selected->sound.gain, juce::dontSendNotification);
        panSlider.setValue (selected->sound.pan, juce::dontSendNotification);
        editSoundButton.setButtonText (selected->sound.playback == OrbitsLane::PlaybackType::pureData ? "Edit Pd" : "Edit SC");
        editSoundButton.setTooltip (selected->sound.playback == OrbitsLane::PlaybackType::pureData
                                    ? "Open this line in the Pure Data editor"
                                    : "Open this line in the SuperCollider editor");
    }
    const std::array<juce::Component*, 11> lineControls { &lineNameEditor, &playbackBox, &lineColourBox, &lineEnabledButton,
                                                          &durationSlider, &probabilitySlider, &gainSlider, &panSlider,
                                                          &editSoundButton, &auditionButton, &deleteLineButton };
    for (auto* component : lineControls)
        component->setEnabled (hasLine);
    removeTrackButton.setEnabled (document.tracks.size() > 1);
    undoButton.setEnabled (! undoHistory.empty());
    redoButton.setEnabled (! redoHistory.empty());
    updateInspectorVisibility();
    canvas->repaint();
}

void OrbitsEditorComponent::setInspectorTab (InspectorTab tab)
{
    inspectorTab = tab;
    inspectorViewport.setViewPosition (0, 0);
    updateInspectorVisibility();
    resized();
    repaint();
}

void OrbitsEditorComponent::updateInspectorVisibility()
{
    trackTabButton.setToggleState (inspectorTab == InspectorTab::track, juce::dontSendNotification);
    shapeTabButton.setToggleState (inspectorTab == InspectorTab::shape, juce::dontSendNotification);
    soundTabButton.setToggleState (inspectorTab == InspectorTab::sound, juce::dontSendNotification);
    for (auto* button : { &trackTabButton, &shapeTabButton, &soundTabButton })
        button->setAlpha (button->getToggleState() ? 1.0f : 0.82f);

    const std::array<juce::Component*, 20> trackControls {
        &trackLabel, &trackList, &hideButton, &muteButton, &soloButton, &mixLabel, &trackGainLabel, &trackGainSlider,
        &trackPanLabel, &trackPanSlider, &outputRouteLabel, &outputRouteBox, &clockModeLabel, &clockModeBox,
        &ratioLabel, &ratioSlider, &resetPhaseButton, &bpmLabel, &bpmSlider, &meterLabel
    };
    const std::array<juce::Component*, 9> trackControlsMore {
        &numeratorLabel, &numeratorSlider, &denominatorLabel, &denominatorSlider, &barsLabel,
        &relationshipLabel, &relationshipTargetLabel, &relationshipActionBox, &relationshipTargetBox
    };
    const std::array<juce::Component*, 1> trackControlsLast { &barsSlider };
    const std::array<juce::Component*, 12> shapeControls {
        &shapeLabel, &thicknessLabel, &thicknessSlider, &warpLabel, &warpSlider, &twistLabel,
        &twistSlider, &phaseLabel, &phaseSlider, &xRotationLabel, &xRotationSlider, &yRotationLabel
    };
    const std::array<juce::Component*, 19> shapeControlsMore {
        &yRotationSlider, &xOffsetLabel, &xOffsetSlider, &yOffsetLabel, &yOffsetSlider,
        &snapLabel, &snapButton, &snapDivisionBox, &patternLabel, &patternStepsLabel, &patternStepsSlider,
        &patternPulsesLabel, &patternPulsesSlider, &distributeButton, &rotateButton, &repeatButton,
        &reverseButton, &randomiseButton, &euclideanButton
    };
    const std::array<juce::Component*, 19> soundControls {
        &soundLineLabel, &lineNameLabel, &lineNameEditor, &playbackLabel, &playbackBox,
        &lineColourLabel, &lineColourBox, &lineEnabledButton, &durationLabel, &durationSlider,
        &probabilityLabel, &probabilitySlider, &gainLabel, &gainSlider, &panLabel, &panSlider,
        &editSoundButton, &auditionButton, &deleteLineButton
    };
    const auto setVisible = [] (const auto& controls, bool visible)
    {
        for (auto* control : controls) if (control != nullptr) control->setVisible (visible);
    };
    setVisible (trackControls, inspectorTab == InspectorTab::track);
    setVisible (trackControlsMore, inspectorTab == InspectorTab::track);
    setVisible (trackControlsLast, inspectorTab == InspectorTab::track);
    setVisible (shapeControls, inspectorTab == InspectorTab::shape);
    setVisible (shapeControlsMore, inspectorTab == InspectorTab::shape);
    setVisible (soundControls, inspectorTab == InspectorTab::sound);
    if (inspectorTab == InspectorTab::track)
    {
        const auto showTarget = track() != nullptr && track()->relationshipAction != OrbitsTrack::RelationshipAction::none;
        relationshipTargetLabel.setVisible (showTarget);
        relationshipTargetBox.setVisible (showTarget);
    }
}

void OrbitsEditorComponent::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    for (auto it = triggerFeedback.begin(); it != triggerFeedback.end();)
        if (it->second.second <= now) it = triggerFeedback.erase (it); else ++it;
    if (! previewRunning) { canvas->repaint(); return; }
    const auto delta = juce::jlimit (0.0, 0.1, now - lastPreviewTime);
    lastPreviewTime = now;
    previewSeconds += delta;
    previewPhases.resize (document.tracks.size(), 0.0);
    if (previewTrackPositions.size() != document.tracks.size()) previewTrackPositions.assign (document.tracks.size(), 0.0);
    if (previewTrackRunning.size() != document.tracks.size()) previewTrackRunning.assign (document.tracks.size(), true);
    std::vector<int> wraps (document.tracks.size(), 0);
    const auto anySolo = std::any_of (document.tracks.begin(), document.tracks.end(), [] (const auto& item) { return item.solo; });
    for (int trackIndex = 0; trackIndex < static_cast<int> (document.tracks.size()); ++trackIndex)
    {
        const auto& current = document.tracks[static_cast<size_t> (trackIndex)];
        const auto loopSeconds = document.loopDurationSeconds (current);
        const auto previousPosition = previewTrackPositions[static_cast<size_t> (trackIndex)];
        const auto nextPosition = previousPosition + (previewTrackRunning[static_cast<size_t> (trackIndex)] ? delta / loopSeconds : 0.0);
        previewTrackPositions[static_cast<size_t> (trackIndex)] = nextPosition;
        wraps[static_cast<size_t> (trackIndex)] = static_cast<int> (std::floor (nextPosition) - std::floor (previousPosition));
        const auto nextPhase = std::fmod (nextPosition, 1.0);
        previewPhases[static_cast<size_t> (trackIndex)] = nextPhase;
        if (current.muted || (anySolo && ! current.solo)) continue;
        for (const auto& triggerLine : current.lines)
            for (const auto phase : triggerLine.triggerPhases)
            {
                const auto crossings = juce::jlimit (0, 64, static_cast<int> (
                    std::floor (nextPosition - phase) - std::floor (previousPosition - phase)));
                for (int crossing = 0; crossing < crossings && triggerLine.sound.enabled; ++crossing)
                {
                    const auto passed = juce::Random::getSystemRandom().nextFloat() <= triggerLine.sound.probability;
                    triggerFeedback[triggerLine.id] = { passed, now + (passed ? 0.24 : 0.16) };
                    if (passed && audition)
                    {
                        auto sound = triggerLine.sound;
                        sound.gain *= current.gain;
                        sound.pan = current.outputRoute == OrbitsTrack::OutputRoute::left ? -1.0f
                                  : current.outputRoute == OrbitsTrack::OutputRoute::right ? 1.0f
                                  : juce::jlimit (-1.0f, 1.0f, sound.pan + current.pan);
                        audition (sound);
                    }
                }
            }
    }
    for (int source = 0; source < static_cast<int> (document.tracks.size()); ++source)
    {
        const auto& relation = document.tracks[static_cast<size_t> (source)];
        const auto target = relation.relationshipTarget;
        if (! juce::isPositiveAndBelow (target, static_cast<int> (document.tracks.size())) || target == source) continue;
        if (relation.relationshipAction == OrbitsTrack::RelationshipAction::phaseLock)
            previewTrackPositions[static_cast<size_t> (target)] = std::floor (previewTrackPositions[static_cast<size_t> (target)])
                                                             + std::fmod (previewTrackPositions[static_cast<size_t> (source)], 1.0);
        else if (wraps[static_cast<size_t> (source)] > 0)
        {
            if (relation.relationshipAction == OrbitsTrack::RelationshipAction::reset) previewTrackPositions[static_cast<size_t> (target)] = 0.0;
            else if (relation.relationshipAction == OrbitsTrack::RelationshipAction::start) previewTrackRunning[static_cast<size_t> (target)] = true;
            else if (relation.relationshipAction == OrbitsTrack::RelationshipAction::stop) previewTrackRunning[static_cast<size_t> (target)] = false;
        }
        previewPhases[static_cast<size_t> (target)] = std::fmod (previewTrackPositions[static_cast<size_t> (target)], 1.0);
    }
    canvas->repaint();
}

void OrbitsEditorComponent::paint (juce::Graphics& graphics)
{
    graphics.fillAll (juce::Colour (0xff101614));
    graphics.setColour (juce::Colours::white);
    graphics.setFont (22.0f);
    graphics.drawText ("Orbits", 24, 16, 180, 28, juce::Justification::centredLeft);
    graphics.setColour (juce::Colours::white.withAlpha (0.45f));
    graphics.setFont (13.0f);
    graphics.drawText ("Double-click and drag to place sounds on the selected layer's spiral timeline.",
                       24, 44, 560, 20, juce::Justification::centredLeft);
    const auto inspectorWidth = getWidth() >= 1120 ? 360 : 324;
    const auto inspectorX = getWidth() - inspectorWidth;
    graphics.setColour (juce::Colour (0xff121b18));
    graphics.fillRect (inspectorX, 72, inspectorWidth, getHeight() - 72);
    graphics.setColour (juce::Colour (0xff26372f));
    graphics.drawVerticalLine (inspectorX, 72.0f, static_cast<float> (getHeight()));
    graphics.drawHorizontalLine (71, 0.0f, static_cast<float> (getWidth()));
}

void OrbitsEditorComponent::resized()
{
    auto header = getLocalBounds().removeFromTop (72).reduced (18, 12);
    auto transport = header.removeFromRight (430);
    undoButton.setBounds (transport.removeFromLeft (58)); transport.removeFromLeft (4);
    redoButton.setBounds (transport.removeFromLeft (58)); transport.removeFromLeft (12);
    addTrackButton.setBounds (transport.removeFromLeft (82)); transport.removeFromLeft (6);
    removeTrackButton.setBounds (transport.removeFromLeft (34)); transport.removeFromLeft (14);
    playButton.setBounds (transport.removeFromLeft (72)); transport.removeFromLeft (6);
    stopButton.setBounds (transport.removeFromLeft (72));

    auto body = getLocalBounds().withTrimmedTop (72);
    const auto inspectorWidth = getWidth() >= 1120 ? 360 : 324;
    auto inspectorPanel = body.removeFromRight (inspectorWidth).reduced (12, 10);
    auto tabs = inspectorPanel.removeFromTop (34);
    const auto tabWidth = tabs.getWidth() / 3;
    trackTabButton.setBounds (tabs.removeFromLeft (tabWidth));
    shapeTabButton.setBounds (tabs.removeFromLeft (tabWidth));
    soundTabButton.setBounds (tabs);
    inspectorPanel.removeFromTop (10);
    inspectorViewport.setBounds (inspectorPanel);
    body.removeFromRight (10);
    canvas->setBounds (body.reduced (12, 10));

    const auto contentWidth = juce::jmax (280, inspectorViewport.getWidth() - inspectorViewport.getScrollBarThickness() - 4);
    const auto contentHeight = inspectorTab == InspectorTab::track ? 590
                              : inspectorTab == InspectorTab::shape ? 520
                              : juce::jmax (520, inspectorViewport.getHeight());
    inspectorContent.setSize (contentWidth, juce::jmax (contentHeight, inspectorViewport.getHeight()));
    if (contentHeight <= inspectorViewport.getHeight())
        inspectorViewport.setViewPosition (0, 0);
    auto inspector = inspectorContent.getLocalBounds().reduced (4, 2);

    if (inspectorTab == InspectorTab::track)
    {
        trackLabel.setBounds (inspector.removeFromTop (18));
        trackList.setBounds (inspector.removeFromTop (72));
        inspector.removeFromTop (4);
        auto trackActions = inspector.removeFromTop (26);
        const auto actionWidth = trackActions.getWidth() / 3;
        hideButton.setBounds (trackActions.removeFromLeft (actionWidth));
        muteButton.setBounds (trackActions.removeFromLeft (actionWidth));
        soloButton.setBounds (trackActions);
        inspector.removeFromTop (8);

        mixLabel.setBounds (inspector.removeFromTop (18));
        auto mixRow = [&inspector] (juce::Label& label, juce::Component& control)
        {
            auto row = inspector.removeFromTop (30); label.setBounds (row.removeFromLeft (72)); control.setBounds (row);
        };
        mixRow (trackGainLabel, trackGainSlider);
        mixRow (trackPanLabel, trackPanSlider);
        mixRow (outputRouteLabel, outputRouteBox);
        inspector.removeFromTop (8);

        clockModeLabel.setBounds (inspector.removeFromTop (18));
        clockModeBox.setBounds (inspector.removeFromTop (28));
        auto clockOptions = inspector.removeFromTop (30);
        ratioLabel.setBounds (clockOptions.removeFromLeft (90)); ratioSlider.setBounds (clockOptions);
        resetPhaseButton.setBounds (inspector.removeFromTop (25));
        inspector.removeFromTop (10);

        auto parameterRow = [&inspector] (juce::Label& label, juce::Component& control)
        {
            auto row = inspector.removeFromTop (34);
            label.setBounds (row.removeFromLeft (98));
            control.setBounds (row);
        };
        parameterRow (bpmLabel, bpmSlider);
        meterLabel.setBounds (inspector.removeFromTop (18));
        auto meter = inspector.removeFromTop (40);
        auto meterLeft = meter.removeFromLeft ((meter.getWidth() - 8) / 2); meter.removeFromLeft (8);
        numeratorLabel.setBounds (meterLeft.removeFromTop (15)); numeratorSlider.setBounds (meterLeft);
        denominatorLabel.setBounds (meter.removeFromTop (15)); denominatorSlider.setBounds (meter);
        parameterRow (barsLabel, barsSlider);
        inspector.removeFromTop (8);
        relationshipLabel.setBounds (inspector.removeFromTop (18));
        relationshipActionBox.setBounds (inspector.removeFromTop (28));
        auto relationTarget = inspector.removeFromTop (30);
        relationshipTargetLabel.setBounds (relationTarget.removeFromLeft (72));
        relationshipTargetBox.setBounds (relationTarget);
        return;
    }

    if (inspectorTab == InspectorTab::shape)
    {
        shapeLabel.setBounds (inspector.removeFromTop (20));
        inspector.removeFromTop (8);
        auto parameterPair = [&inspector] (juce::Label& leftLabel, juce::Component& left,
                                           juce::Label& rightLabel, juce::Component& right)
        {
            auto row = inspector.removeFromTop (62);
            auto leftArea = row.removeFromLeft ((row.getWidth() - 8) / 2); row.removeFromLeft (8);
            leftLabel.setBounds (leftArea.removeFromTop (17)); left.setBounds (leftArea);
            rightLabel.setBounds (row.removeFromTop (17)); right.setBounds (row);
        };
        parameterPair (thicknessLabel, thicknessSlider, warpLabel, warpSlider);
        parameterPair (twistLabel, twistSlider, phaseLabel, phaseSlider);
        parameterPair (xRotationLabel, xRotationSlider, yRotationLabel, yRotationSlider);
        parameterPair (xOffsetLabel, xOffsetSlider, yOffsetLabel, yOffsetSlider);
        inspector.removeFromTop (18);
        snapLabel.setBounds (inspector.removeFromTop (20));
        auto snapRow = inspector.removeFromTop (34);
        snapButton.setBounds (snapRow.removeFromLeft (142)); snapDivisionBox.setBounds (snapRow);
        inspector.removeFromTop (18);
        patternLabel.setBounds (inspector.removeFromTop (20));
        auto patternValues = inspector.removeFromTop (38);
        auto leftValue = patternValues.removeFromLeft ((patternValues.getWidth() - 8) / 2); patternValues.removeFromLeft (8);
        patternStepsLabel.setBounds (leftValue.removeFromLeft (42)); patternStepsSlider.setBounds (leftValue);
        patternPulsesLabel.setBounds (patternValues.removeFromLeft (44)); patternPulsesSlider.setBounds (patternValues);
        inspector.removeFromTop (6);
        auto patternRow = [&inspector] (juce::TextButton& a, juce::TextButton& b, juce::TextButton& c)
        {
            auto row = inspector.removeFromTop (30); const auto width = (row.getWidth() - 8) / 3;
            a.setBounds (row.removeFromLeft (width)); row.removeFromLeft (4);
            b.setBounds (row.removeFromLeft (width)); row.removeFromLeft (4);
            c.setBounds (row);
        };
        patternRow (distributeButton, rotateButton, repeatButton);
        inspector.removeFromTop (4);
        patternRow (reverseButton, randomiseButton, euclideanButton);
        return;
    }

    soundLineLabel.setBounds (inspector.removeFromTop (24));
    inspector.removeFromTop (12);
    if (line() == nullptr)
        return;

    auto parameterRow = [&inspector] (juce::Label& label, juce::Component& control)
    {
        auto row = inspector.removeFromTop (40);
        label.setBounds (row.removeFromLeft (82));
        control.setBounds (row);
    };
    parameterRow (lineNameLabel, lineNameEditor);
    parameterRow (playbackLabel, playbackBox);
    parameterRow (lineColourLabel, lineColourBox);
    lineEnabledButton.setBounds (inspector.removeFromTop (30));
    inspector.removeFromTop (8);
    parameterRow (durationLabel, durationSlider);
    parameterRow (probabilityLabel, probabilitySlider);
    parameterRow (gainLabel, gainSlider);
    parameterRow (panLabel, panSlider);
    inspector.removeFromTop (14);
    auto lineActions = inspector.removeFromTop (38);
    editSoundButton.setBounds (lineActions.removeFromLeft (92)); lineActions.removeFromLeft (6);
    auditionButton.setBounds (lineActions.removeFromLeft (86)); lineActions.removeFromLeft (6);
    deleteLineButton.setBounds (lineActions);
    return;
}
}
