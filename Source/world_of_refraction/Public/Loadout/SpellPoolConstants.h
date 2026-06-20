// SpellPoolConstants.h
// Spell-pool economy: the weighted-budget model shared by the Caster innate pool
// and the Broken Darkness pools (docs/Design/InnateSpellPoolBudget.md).
//
// TWO INDEPENDENT limits (both must pass; do not conflate):
//   1. Count cap  — MAX_EQUIPPED_SLOT_POOL, a tier-blind per-pool spell count.
//   2. Weight budget — INNATE_SPELL_BUDGET / BD_SPELL_BUDGET, a total point cost
//      across ALL pools, where each spell's cost is its tier (F1..S7, discounted).
//
// Tier-aware (indexed by TierHelpers::GetTierValue, F=0..S=6) like DurabilityConstants
// / AugmentStoneConstants — includes ItemTier.h (a module leaf), so no dependency cycle.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"

namespace SpellPoolConstants
{
    // --- Count cap (tier-blind, per pool) ---
    /** Raw spell count cap per pool (innate school / BD element pool). Unifies the
     *  former MAX_INNATE_SPELLS_PER_SCHOOL + MAX_BD_POOL_SPELLS (same value, 6). */
    constexpr int32 MAX_EQUIPPED_SLOT_POOL = 6;

    // --- Weight budget (point cost across all pools) ---
    /** Broken Darkness total point budget, shared across the Darkness pool + every
     *  absorbed element pool. */
    constexpr int32 BD_SPELL_BUDGET = 48;

    /** Non-BD Caster innate budget — locked at half of BD (one edit point). */
    constexpr int32 INNATE_SPELL_BUDGET = BD_SPELL_BUDGET / 2; // = 24

    // --- Per-spell cost curve (tier = cost) ---
    /** Slot cost by tier: F=1, E=2, D=3, C=4, B=5, A=6, S=7. */
    constexpr int32 SPELL_SLOT_COST[7] = {1, 2, 3, 4, 5, 6, 7};

    // --- Mastery discount ---
    /** Cost reduction per qualifying world pillar. */
    constexpr int32 SPELL_DISCOUNT_PER_PILLAR = 1;

    /** A world pillar level >= this grants its discount (tunable; ship 7). */
    constexpr int32 SPELL_DISCOUNT_PILLAR_THRESHOLD = 7;

    /** A discounted cost never drops below this — no spell is ever free. */
    constexpr int32 SPELL_SLOT_COST_FLOOR = 1;

    // --- Compile-time guards ---
    static_assert(sizeof(SPELL_SLOT_COST) / sizeof(int32) == 7,
                  "SPELL_SLOT_COST must have one entry per tier (F..S)");
    static_assert(SPELL_SLOT_COST[0] == 1 && SPELL_SLOT_COST[6] == 7,
                  "SPELL_SLOT_COST endpoints drifted from 1..7");
    static_assert(INNATE_SPELL_BUDGET == BD_SPELL_BUDGET / 2,
                  "INNATE_SPELL_BUDGET must stay locked at half of BD_SPELL_BUDGET");

    // --- Helpers (header-only; tier + int discount in, int out — no character/loadout types) ---

    /** Base cost for a spell tier (pre-discount). */
    inline int32 SpellSlotBaseCost(EItemTier Tier)
    {
        const int32 i = TierHelpers::GetTierValue(Tier);
        return (i >= 0 && i < 7) ? SPELL_SLOT_COST[i] : SPELL_SLOT_COST_FLOOR;
    }

    /** Effective cost after a precomputed discount (0..3), floored. */
    inline int32 SpellSlotEffectiveCost(EItemTier Tier, int32 Discount)
    {
        return FMath::Max(SPELL_SLOT_COST_FLOOR, SpellSlotBaseCost(Tier) - Discount);
    }

    /** Discount from RAW world pillar levels (the 0..N ints, NOT GetEffectiveX
     *  multipliers): one step per pillar at/above the threshold, 0..3 total. */
    inline int32 SpellSlotDiscount(int32 WorldMind, int32 WorldBody, int32 WorldSpirit)
    {
        int32 D = 0;
        if (WorldMind >= SPELL_DISCOUNT_PILLAR_THRESHOLD)   D += SPELL_DISCOUNT_PER_PILLAR;
        if (WorldBody >= SPELL_DISCOUNT_PILLAR_THRESHOLD)   D += SPELL_DISCOUNT_PER_PILLAR;
        if (WorldSpirit >= SPELL_DISCOUNT_PILLAR_THRESHOLD) D += SPELL_DISCOUNT_PER_PILLAR;
        return D;
    }
}
