// DurabilityConstants.h
// Constants for the crystal durability system
//
// Replaces the % break chance model. Crystals accrue wear on each "overwork" event:
// tier mismatch, infusion levels, custom spell casting. At 0 durability the crystal
// breaks. Auto-repair restores durability between combats.
//
// See: Infusion_Design_Decisions_Locked.md (1 May 2026) for spec authority.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"

namespace DurabilityConstants
{
    // ==================== TIER MAX DURABILITY ====================
    // Per locked spec: F=30, E=40, D=50, C=60, B=70, A=80, S=100

    constexpr int32 MAX_DURABILITY_F_TIER = 30;
    constexpr int32 MAX_DURABILITY_E_TIER = 40;
    constexpr int32 MAX_DURABILITY_D_TIER = 50;
    constexpr int32 MAX_DURABILITY_C_TIER = 60;
    constexpr int32 MAX_DURABILITY_B_TIER = 70;
    constexpr int32 MAX_DURABILITY_A_TIER = 80;
    constexpr int32 MAX_DURABILITY_S_TIER = 100;

    // ==================== WEAR VALUES ====================
    // Wear stacks. Example: 2-tier-mismatched L2 spell on a custom spell =
    //   6 (mismatch: 3 per tier x 2) + 12 (L2 spell) + 2 (custom) = 20 wear

    /** Wear per tier the action is above the crystal tier */
    constexpr int32 WEAR_PER_TIER_MISMATCH = 3;

    /** L1 infusion on Ability/Attack */
    constexpr int32 ABILITY_L1_WEAR = 4;

    /** L2 infusion on Ability/Attack */
    constexpr int32 ABILITY_L2_WEAR = 8;

    /** L1 infusion on Spell. Higher than ability because spells "create from nothing" */
    constexpr int32 SPELL_L1_WEAR = 6;

    /** L2 infusion on Spell */
    constexpr int32 SPELL_L2_WEAR = 12;

    /** Custom (non-default) spell cast on a ring's crystal */
    constexpr int32 CUSTOM_SPELL_WEAR = 2;

    // ==================== SUBSTAT WEAR MODIFIER ====================
    // Caster power/control wraps the base wear:
    //   final = base x power_factor / control_factor (with floor and tier-gated ceiling)
    // power_factor   from spell_damage% + status_multiplier% (caster output)
    // control_factor from efficiency%   + resistance%        (caster discipline)
    // See: feature/crystal-wear-substat-modifier (paper sanity-check)

    /** Sensitivity of power/control factors to each 1.0 of substat fraction */
    constexpr float SUBSTAT_AMP = 5.0f;

    /** Lower bound on power_factor — a weak caster still pays meaningful wear */
    constexpr float SUBSTAT_POWER_FACTOR_MIN = 0.4f;

    /** Lower bound on control_factor — undisciplined caster can't reduce control below this */
    constexpr float SUBSTAT_CONTROL_FACTOR_MIN = 0.5f;

    /** Upper bound on control_factor — a skilled caster still pays some wear */
    constexpr float SUBSTAT_CONTROL_FACTOR_MAX = 3.0f;

    /** Final wear is never less than this fraction of the base (when base > 0) */
    constexpr float SUBSTAT_FLOOR_FRAC = 0.25f;

    /** Final wear is capped at this fraction of max durability when tier gap < ONE_SHOT_GAP */
    constexpr float SUBSTAT_CEIL_FRAC = 0.45f;

    /** At/above this tier gap the ceiling is lifted (shatter is allowed) */
    constexpr int32 SUBSTAT_ONE_SHOT_GAP = 4;

    // ==================== REPAIR ====================

    /** Durability restored to each crystal at end of combat */
    constexpr int32 REPAIR_PER_BATTLE = 10;

    // ==================== HUD WARNING ====================

    /** Show warning indicator on HUD when durability drops below this percent */
    constexpr float LOW_DURABILITY_WARNING_THRESHOLD = 0.25f;

    // ==================== TIER LOOKUP ====================

    /** Get max durability for a given tier. Returns 0 for invalid tiers. */
    inline int32 GetMaxDurabilityForTier(EItemTier Tier)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier:
            return MAX_DURABILITY_F_TIER;
        case EItemTier::E_Tier:
            return MAX_DURABILITY_E_TIER;
        case EItemTier::D_Tier:
            return MAX_DURABILITY_D_TIER;
        case EItemTier::C_Tier:
            return MAX_DURABILITY_C_TIER;
        case EItemTier::B_Tier:
            return MAX_DURABILITY_B_TIER;
        case EItemTier::A_Tier:
            return MAX_DURABILITY_A_TIER;
        case EItemTier::S_Tier:
            return MAX_DURABILITY_S_TIER;
        default:
            return 0;
        }
    }
}