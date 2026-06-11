// ItemConstants.h
// Constants for the item system

#pragma once

#include "CoreMinimal.h"

namespace ItemConstants
{
    // ==================== INVENTORY LIMITS ====================
    constexpr int32 MAX_ITEM_SLOTS = 6;
    constexpr int32 MAX_STACKS_PER_SLOT = 3;
    constexpr int32 MAX_TOTAL_ITEMS = MAX_ITEM_SLOTS * MAX_STACKS_PER_SLOT; // 18

    // ==================== REVIVE ====================
    /** HP fraction restored on revive — the Revive skill-effect intercept
     *  (CharacterDataComponent::CheckDeath) and Sapphire-S share it. */
    constexpr float REVIVE_HP_PERCENT = 0.3f;

    // ==================== QUARTZ ====================
    /** Quartz F-A elemental ResistanceBuff magnitude (S-tier grants immunity
     *  instead). NOT the stat-crystal curve's 30 endpoint — unrelated value. */
    constexpr float QUARTZ_RESIST_PERCENT = 30.0f;

    // ==================== IOLITE ====================
    /** Sentinel returned by GetEffectsToRemoveCount at Iolite S-tier: remove ALL
     *  matching effects (the cleanse loop stops counting when it sees this). */
    constexpr int32 IOLITE_REMOVE_ALL = 99;

    // ==================== BROKEN DARKNESS ENERGY BONUSES ====================
    // Broken Darkness gains EP equal to a percentage of the target's MaxEP,
    // scaled by the crystal's tier. Resolved at the call site
    // (ItemExecutor::ApplyBrokenDarknessBonus) where MaxEP is available.
    constexpr float BD_ENERGY_PERCENT_F = 0.10f;
    constexpr float BD_ENERGY_PERCENT_E = 0.20f;
    constexpr float BD_ENERGY_PERCENT_D = 0.30f;
    constexpr float BD_ENERGY_PERCENT_C = 0.40f;
    constexpr float BD_ENERGY_PERCENT_B = 0.50f;
    constexpr float BD_ENERGY_PERCENT_A = 0.60f;
    constexpr float BD_ENERGY_PERCENT_S = 0.70f;
}