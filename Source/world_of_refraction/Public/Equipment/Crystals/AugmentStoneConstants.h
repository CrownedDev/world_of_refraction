// AugmentStoneConstants.h
// Per-tier tuning curves for the augment-stone / fusion system, consolidated so
// the value logic has a single edit point. Extracted verbatim (value-preserving)
// from the inline tier-switches in CrystalEffectTable — see that file's getters.
//
// Modeled on DurabilityConstants.h: a tier-aware constants header that includes
// ItemTier.h and is indexed by TierHelpers::GetTierValue (a bare cast, F=0..S=6),
// so array order matches the EItemTier enum order exactly.
//
// NOTE: kept out of CombatConstants.h deliberately — that header forbids an
// ItemTier.h dependency (it is a module-wide leaf). This is the same reason
// DurabilityConstants lives separately.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"

namespace AugmentStoneConstants
{
    // index = TierHelpers::GetTierValue(Tier)  ->  F,E,D,C,B,A,S = 0..6

    /** Shared stat-stone base-percent curve (DamageStone/DefenseStone/CritStone/
     *  TurnSpeedStone/StatusStone/EfficiencyStone/MaxHPStone/MaxEPStone). */
    constexpr float STONE_BASE_PERCENT[7] = {3, 5, 7, 9, 11, 13, 15};

    /** Stone×2 uniform buff curve shared by Emerald (speed), Opal (crit) and
     *  Amber (defense) — exactly 2× STONE_BASE_PERCENT. */
    constexpr float STAT_CRYSTAL_BUFF_PERCENT[7] = {6, 10, 14, 18, 22, 26, 30};

    /** Skill-slot curve: AbilityStone -> ability slots, gem crystal -> spell slots.
     *  Non-sequential (F=2, E=3, D=3, C=4, B=4, A=5, S=6). */
    constexpr int32 ATTACHMENT_SLOTS[7] = {2, 3, 3, 4, 4, 5, 6};

    static_assert(sizeof(STONE_BASE_PERCENT) / sizeof(float) == 7,
                  "STONE_BASE_PERCENT must have one entry per tier (F..S)");
    static_assert(sizeof(STAT_CRYSTAL_BUFF_PERCENT) / sizeof(float) == 7,
                  "STAT_CRYSTAL_BUFF_PERCENT must have one entry per tier (F..S)");
    static_assert(sizeof(ATTACHMENT_SLOTS) / sizeof(int32) == 7,
                  "ATTACHMENT_SLOTS must have one entry per tier (F..S)");

    // Spot checks pinning the curve endpoints to the pre-extraction literals.
    static_assert(STONE_BASE_PERCENT[0] == 3.0f && STONE_BASE_PERCENT[6] == 15.0f,
                  "STONE_BASE_PERCENT endpoints drifted from 3..15");
    static_assert(STAT_CRYSTAL_BUFF_PERCENT[0] == 6.0f && STAT_CRYSTAL_BUFF_PERCENT[6] == 30.0f,
                  "STAT_CRYSTAL_BUFF_PERCENT endpoints drifted from 6..30");
    static_assert(ATTACHMENT_SLOTS[0] == 2 && ATTACHMENT_SLOTS[6] == 6,
                  "ATTACHMENT_SLOTS endpoints drifted from 2..6");
}
