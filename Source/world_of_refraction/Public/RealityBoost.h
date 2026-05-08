// RealityBoost.h
// Per-action stat boost magnitudes for the Reality element.
// New consumers read modifiers via FActionStatModifiers (built by
// UActionExecutor::ComputeActionStatModifiers). Legacy bool-keyed
// ApplyTo(stat, bool) overloads remain for sites still on the
// bRealityL2Boost path during migration.

#pragma once

#include "CoreMinimal.h"

namespace RealityBoost
{
    /** Reality innate (Refractor character) — +10% all sub-stats. */
    constexpr float INNATE_PERCENT = 10.0f;

    /** Reality Evolution crystal slotted as primary — +5% all sub-stats
     *  (in addition to the crystal's authored stats). */
    constexpr float SLOTTED_PERCENT = 5.0f;

    /** Reality crystal infused at L1 — +2.5% all sub-stats. */
    constexpr float L1_PERCENT = 2.5f;

    /** Reality crystal infused at L2 — +5% all sub-stats. */
    constexpr float L2_PERCENT = 5.0f;

    /** Legacy: kept for backward compatibility during migration.
     *  Existing consumers still use ApplyTo with a bool; once retired,
     *  delete this section. New consumers use FActionStatModifiers. */
    constexpr float L2_MULTIPLIER = 1.05f;

    inline float ApplyTo(float StatValue, bool bBoostActive)
    {
        return bBoostActive ? StatValue * L2_MULTIPLIER : StatValue;
    }

    inline int32 ApplyTo(int32 StatValue, bool bBoostActive)
    {
        return bBoostActive ? FMath::RoundToInt(StatValue * L2_MULTIPLIER) : StatValue;
    }
}
