// StatConstants.h
// Centralized constants for World of Refraction stat systems

#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatConstants.h"

/**
 * Stat System Constants
 * Defines limits and scaling values for character progression
 */
namespace StatConstants
{
    // ==================== WORLD STAT SYSTEM ====================

    constexpr int32 MIN_WORLD_STAT_LEVEL = 0;
    constexpr int32 MAX_WORLD_STAT_LEVEL = 7;

    // OBSOLETE (B-330, D-123; delete in the D-124 sweep): the 3-points-per-level sub-stat pool. A pillar level
    // grants no sub-stat points; sub-stats come from the character asset and gear. Still read by
    // UCharacterData::GetTotalPool (now only the enemy caliber) and BrokenDarknessManager's EP aura range
    // (via MAX_SUBSTAT_POINTS_PER_PILLAR); both need their own constant before the sweep deletes these.
    constexpr int32 POINTS_PER_WORLD_STAT_LEVEL = 3;

    // OBSOLETE (B-330, D-123; delete in the D-124 sweep): max substat points per pillar from the pool (7 × 3 = 21)
    constexpr int32 MAX_SUBSTAT_POINTS_PER_PILLAR = MAX_WORLD_STAT_LEVEL * POINTS_PER_WORLD_STAT_LEVEL;

    // ==================== BASE STAT BUDGET ====================

    // OBSOLETE (B-330, D-123; delete in the D-124 sweep): the pool's starting points (GetTotalPool / caliber).
    constexpr int32 INITIAL_STAT_BUDGET = 30;

    // D-123 (B-330, Q-330): the most sub-stat points a character asset is authored with, at any pillar level.
    // The old pool's 7/7/7 total (30 + 63), so every existing asset stays valid and the per-point maths
    // (CombatConstants::MAX_STAT_POINTS) is unchanged. Pillar levels add nothing to it.
    constexpr int32 MAX_AUTHORED_SUBSTAT_POINTS = 93;
    static_assert(MAX_AUTHORED_SUBSTAT_POINTS == static_cast<int32>(CombatConstants::MAX_STAT_POINTS),
                  "The authored sub-stat ceiling must match the per-point maths' reference (Q-330)");

    // ==================== ACTION SPEED SCALING ====================

    constexpr float MOVEMENT_SPEED_MIN_MULTIPLIER = 1.0f; // 0 points
    constexpr float MOVEMENT_SPEED_MAX_MULTIPLIER = 2.1f; // Max points

    // ==================== REQUIREMENT PENALTIES ====================

    constexpr float MAX_REQUIREMENT_PENALTY = 0.60f;
    constexpr float PENALTY_MULTIPLIER = 0.10f;

    // ==================== RANK THRESHOLDS ====================

    constexpr int32 E_RANK_THRESHOLD = 1;
    constexpr int32 D_RANK_THRESHOLD = 2;
    constexpr int32 C_RANK_THRESHOLD = 3;
    constexpr int32 B_RANK_THRESHOLD = 5;
    constexpr int32 A_RANK_THRESHOLD = 6;
    constexpr int32 S_RANK_THRESHOLD = 7;
}