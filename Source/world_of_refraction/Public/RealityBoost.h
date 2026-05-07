// RealityBoost.h
// Per-action stat boost magnitude for Reality element at infusion Level 2.
// See May2026/Design_Decisions_05052026.md Section 3 for locked design.

#pragma once

#include "CoreMinimal.h"

namespace RealityBoost
{
    /** Reality at L2 grants +5% to the five offensive sub-stats for the action. */
    constexpr float L2_MULTIPLIER = 1.05f;

    /** Apply the L2 boost to a stat value if active, else passthrough. */
    inline float ApplyTo(float StatValue, bool bBoostActive)
    {
        return bBoostActive ? StatValue * L2_MULTIPLIER : StatValue;
    }

    /** Convenience for integer stats (rounds to nearest int after boost). */
    inline int32 ApplyTo(int32 StatValue, bool bBoostActive)
    {
        return bBoostActive ? FMath::RoundToInt(StatValue * L2_MULTIPLIER) : StatValue;
    }
}
