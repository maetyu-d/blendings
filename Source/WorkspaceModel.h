#pragma once

#include <JuceHeader.h>

#include "AppTheme.h"
#include "CarouselEditorComponent.h"
#include "GridModel.h"
#include "OrbitsElement.h"
#include "ScSheetScore.h"

#include <array>
#include <vector>

namespace blendings
{
juce::String defaultScCode();
juce::String defaultPdPatch();
ScoreDocument defaultScSheet();
gridcollider::GridModel::Snapshot defaultOrcaGrid();

struct RoadRoute
{
    std::vector<juce::Point<float>> points;
    juce::Colour colour = ui::lineColour();
    bool isPipe = false;
    bool isWarpPipe = false;

    bool isDrawable() const noexcept { return points.size() >= 2; }
    float getLength() const;
};

struct TriggerQuantizeRegion
{
    juce::Rectangle<float> bounds;
    int quantizeChoice = 2; // 1/8 note
    bool enabled = true;
    juce::String id { juce::Uuid().toString() };
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

std::array<SequencingClock, 4> defaultSequencingClocks();

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
    enum class Mode { gate = 0, counter, switcher, comparator, flipFlop, everyNth, andGate, orGate, xorGate };
    enum class Orientation { right = 0, down, left, up };
    enum class SignalMode { edge = 0, level };
    enum class TimeoutAction { discard = 0, release, reverse, reroute };
    enum class Comparison { less = 0, lessOrEqual, equal, greaterOrEqual, greater };
    enum class Branch { left = 0, straight, right, random };

    juce::Point<float> position;
    Mode mode = Mode::gate;
    Comparison comparison = Comparison::greaterOrEqual;
    Branch branch = Branch::straight;
    int targetCount = 4;
    double compareSpeed = 1.0;
    double coincidenceBeats = 0.125;
    double levelHoldBeats = 0.25;
    bool gateOpen = true;
    bool enabled = true;
    Orientation orientation = Orientation::right;
    SignalMode signalMode = SignalMode::edge;
    TimeoutAction timeoutAction = TimeoutAction::discard;

    int count = 0;
    bool flipState = false;
    int inputAKey = -1;
    int inputBKey = -1;
    double inputABeat = -1000.0;
    double inputBBeat = -1000.0;
    double inputAFlashUntilMs = 0.0;
    double inputBFlashUntilMs = 0.0;
    double outputFlashUntilMs = 0.0;
    bool releaseHeldInput = false;
    juce::String lastEvent { "Waiting" };
    juce::String id { juce::Uuid().toString() };
};

enum class ModulationTargetKind
{
    disc = 0, tap, drain, quantum, speedLimit, wait, strike, teleport, filter, logic
};

struct Modulator
{
    enum class Shape { sine = 0, triangle, square, random, sawUp, sawDown, smoothRandom };
    juce::String id { juce::Uuid().toString() };
    juce::Point<float> position;
    juce::String name { "Modulator" };
    Shape shape = Shape::sine;
    double cycleBeats = 4.0;
    double phase = 0.0;
    int clockIndex = 0;
    bool bipolar = true;
    bool enabled = true;
};

struct ModulationConnection
{
    enum class Curve { linear = 0, easeIn, easeOut, smooth };
    juce::String sourceId;
    ModulationTargetKind targetKind = ModulationTargetKind::disc;
    juce::String targetId;
    int parameter = 0;
    double depth = 0.5;
    double offset = 0.0;
    double smoothingBeats = 0.0;
    bool inverted = false;
    bool enabled = true;
    Curve curve = Curve::linear;
};

juce::String modulationTargetName (ModulationTargetKind kind);
juce::StringArray modulationParameterNames (ModulationTargetKind kind);

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
    std::vector<OrbitsDocument> orbits;
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

    int getElementCount() const noexcept;
    bool hasNestedWorld() const noexcept { return nestedWorldCount > 0; }
    bool hasScCodeElement() const noexcept { return ! scCodeElements.empty(); }
    bool hasScSheet() const noexcept { return ! scSheets.empty(); }
    bool hasOrcaGrid() const noexcept { return ! orcaGrids.empty(); }
    bool hasPdPatch() const noexcept { return ! pdPatches.empty(); }
    bool hasCarousel() const noexcept { return ! carousels.empty(); }
    bool hasPipeWorld() const noexcept { return ! pipeWorlds.empty(); }
    bool hasOrbits() const noexcept { return ! orbits.empty(); }
};
}
