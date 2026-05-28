// RealityBoost.h
// Reality element stat boost magnitudes. Consumed by
// UActionExecutor::ComputeActionStatModifiers when building
// FActionStatModifiers for a given action.

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
}
