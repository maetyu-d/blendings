#pragma once

#include "ScSheetScore.h"

namespace ScoreMath
{
    double wrapUnit(double value);
    double deterministicNoise(int rowSeed, int step, int voiceIndex);
    bool shouldTriggerStep(const ScoreRow& row, int step, int steps);
    double contourPosition(const ScoreRow& row, int step, int steps);
    double macroLevel(const ScoreRow& row, int step, int steps);
    double phraseLevel(const ScoreRow& row, double localBeat);
    double accentLevel(const ScoreRow& row, int step);
    int phrasePitchOffset(const ScoreRow& row, double localBeat);
}
