// TierPowerConstants.h
// Own-tier power scaling — how strong a thing is purely from its own tier (F..S),
// independent of the channel it is used through. Multiplies the authored base of
// skills (and the per-point value of gear) by one uniform factor.
// Counterpart of TierGapConstants.h (action tier vs channel tier); the two stack:
// Final base = TierPower(own) * TierGap(vs channel) * StatScaling * envelope.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"

namespace TierPowerScaling
{
    // Own-tier power multiplier — F = floor x1.0, compounding ~30%/tier up to S.
    // Indexed by TierHelpers::GetTierValue (F=0 .. S=6). Locked curve; tune in PIE.
    constexpr float TIER_POWER[7] = {1.00f, 1.30f, 1.70f, 2.20f, 2.85f, 3.70f, 4.80f}; // F..S

    static_assert(sizeof(TIER_POWER) / sizeof(TIER_POWER[0]) == 7,
                  "TIER_POWER must cover all 7 tiers (F..S).");
    static_assert(TIER_POWER[0] == 1.0f, "F is the floor anchor — TIER_POWER[0] must be 1.0.");

    /** Power multiplier for a thing of its own Tier. F = x1.0; unknown/out-of-range
     *  tier returns 1.0 (no scaling) rather than inventing a factor. */
    inline float GetTierPowerMultiplier(EItemTier Tier)
    {
        const int32 i = TierHelpers::GetTierValue(Tier);
        return (i >= 0 && i < 7) ? TIER_POWER[i] : 1.0f;
    }
}
