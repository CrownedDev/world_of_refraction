// TierGapConstants.h
// Tier-gap damage scaling — the action's own tier vs the channel it is used
// through (spell vs catalyst crystal, ability/attack vs active weapon).
// Damage-side counterpart of the wear system's tier gap; the wear system is
// untouched and keeps its own constants (DurabilityConstants.h).

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"

namespace TierGapDamage
{
    // Gap = ActionTier - ChannelTier (TierHelpers::GetTierGap(Channel, Action)).
    // Negative gap: the channel out-tiers the action — the stronger catalyst
    // amplifies it. Positive gap: the action out-tiers the channel — the weak
    // catalyst can't carry it. Starting values, tune after PIE.
    constexpr float BOOST_GAP_NEG4_PLUS = 1.30f;  // gap <= -4
    constexpr float BOOST_GAP_NEG3 = 1.20f;
    constexpr float BOOST_GAP_NEG2 = 1.13f;
    constexpr float BOOST_GAP_NEG1 = 1.06f;
    constexpr float MATCHED_TIER = 1.00f;         // gap 0
    constexpr float PENALTY_GAP_1 = 0.90f;
    constexpr float PENALTY_GAP_2 = 0.78f;
    constexpr float PENALTY_GAP_3 = 0.64f;
    constexpr float PENALTY_GAP_4_PLUS = 0.50f;   // gap >= +4

    /** Tier span is F..S = 6 steps; gaps beyond this are clamped. */
    constexpr int32 GAP_CLAMP = 6;

    /** Damage multiplier for an action of ActionTier used through a channel of
     *  ChannelTier. 1.0 at matched tier; callers with no resolvable channel
     *  should skip the call (no scaling) rather than invent a channel tier. */
    inline float GetTierGapDamageMultiplier(EItemTier ActionTier, EItemTier ChannelTier)
    {
        const int32 Gap = FMath::Clamp(
            TierHelpers::GetTierGap(ChannelTier, ActionTier), -GAP_CLAMP, GAP_CLAMP);

        if (Gap <= -4)
        {
            return BOOST_GAP_NEG4_PLUS;
        }
        if (Gap >= 4)
        {
            return PENALTY_GAP_4_PLUS;
        }

        switch (Gap)
        {
        case -3:
            return BOOST_GAP_NEG3;
        case -2:
            return BOOST_GAP_NEG2;
        case -1:
            return BOOST_GAP_NEG1;
        case 0:
            return MATCHED_TIER;
        case 1:
            return PENALTY_GAP_1;
        case 2:
            return PENALTY_GAP_2;
        case 3:
            return PENALTY_GAP_3;
        default:
            return MATCHED_TIER; // unreachable — gap fully covered above
        }
    }

    // ===================== COST (reciprocal) =====================
    // Cost-side tier gap. RECIPROCAL of the damage ladder above: a channel that
    // out-tiers the action (negative gap) makes the action CHEAPER (< 1.0); an
    // action that out-tiers its channel (positive gap) costs MORE (> 1.0). This
    // is its OWN ladder — it does not reuse the damage multipliers. Starting
    // values, tune after PIE.
    constexpr float COST_GAP_NEG4_PLUS = 0.77f;   // gap <= -4
    constexpr float COST_GAP_NEG3 = 0.83f;
    constexpr float COST_GAP_NEG2 = 0.88f;
    constexpr float COST_GAP_NEG1 = 0.94f;
    constexpr float MATCHED_COST = 1.00f;         // gap 0
    constexpr float COST_GAP_1 = 1.11f;
    constexpr float COST_GAP_2 = 1.28f;
    constexpr float COST_GAP_3 = 1.56f;
    constexpr float COST_GAP_4_PLUS = 2.00f;      // gap >= +4

    /** Cost multiplier for an action of ActionTier used through a channel of
     *  ChannelTier. 1.0 at matched tier; callers with no resolvable channel
     *  should skip the call (no scaling) rather than invent a channel tier. */
    inline float GetTierGapCostMultiplier(EItemTier ActionTier, EItemTier ChannelTier)
    {
        const int32 Gap = FMath::Clamp(
            TierHelpers::GetTierGap(ChannelTier, ActionTier), -GAP_CLAMP, GAP_CLAMP);

        if (Gap <= -4)
        {
            return COST_GAP_NEG4_PLUS;
        }
        if (Gap >= 4)
        {
            return COST_GAP_4_PLUS;
        }

        switch (Gap)
        {
        case -3:
            return COST_GAP_NEG3;
        case -2:
            return COST_GAP_NEG2;
        case -1:
            return COST_GAP_NEG1;
        case 0:
            return MATCHED_COST;
        case 1:
            return COST_GAP_1;
        case 2:
            return COST_GAP_2;
        case 3:
            return COST_GAP_3;
        default:
            return MATCHED_COST; // unreachable — gap fully covered above
        }
    }

    // ===================== REQUIREMENT GAP (±5, raw int gap) =====================
    // Per-pillar requirement scaling: how far a character's world-pillar level sits
    // above/below a skill's required level for that pillar. Over-stat (negative gap)
    // amplifies; under-stat (positive gap) penalizes — same trend as the damage ladder
    // above, EXTENDED by one rung at each end (±5). Takes a RAW gap int (Required -
    // CharLevel), NOT tiers: world levels are 0-7 so the gap can reach ±7; clamped to
    // ±5 here. Reuses the -4..+4 damage constants by name; only the ±5 ends are new.
    constexpr float REQ_GAP_NEG5 = 1.40f;   // gap <= -5 (extreme over-stat)
    constexpr float REQ_GAP_5 = 0.32f;      // gap >= +5 (extreme under-stat)

    /** Requirement-gap multiplier for a SINGLE pillar. Gap = RequiredLevel - CharLevel
     *  (positive = under-statted). Clamped to -5..+5; negative boosts, positive penalizes.
     *  Raw int gap, NOT tiers — distinct from the tier-gap helpers above. */
    inline float GetRequirementGapMultiplier(int32 Gap)
    {
        Gap = FMath::Clamp(Gap, -5, 5);

        if (Gap <= -5)
        {
            return REQ_GAP_NEG5;
        }
        if (Gap >= 5)
        {
            return REQ_GAP_5;
        }

        switch (Gap)
        {
        case -4:
            return BOOST_GAP_NEG4_PLUS;
        case -3:
            return BOOST_GAP_NEG3;
        case -2:
            return BOOST_GAP_NEG2;
        case -1:
            return BOOST_GAP_NEG1;
        case 0:
            return MATCHED_TIER;
        case 1:
            return PENALTY_GAP_1;
        case 2:
            return PENALTY_GAP_2;
        case 3:
            return PENALTY_GAP_3;
        case 4:
            return PENALTY_GAP_4_PLUS;
        default:
            return MATCHED_TIER; // unreachable — gap fully covered above
        }
    }
}
