// StatConstants.h
// Centralized constants for World of Refraction stat systems

#pragma once

#include "CoreMinimal.h"

/**
 * Stat System Constants
 * Defines limits and scaling values for character progression
 */
namespace StatConstants
{
    // ==================== WORLD STAT SYSTEM ====================

    constexpr int32 MIN_WORLD_STAT_LEVEL = 0;
    constexpr int32 MAX_WORLD_STAT_LEVEL = 7;
    constexpr int32 POINTS_PER_WORLD_STAT_LEVEL = 3;

    // Derived: Max substat points per pillar (7 × 3 = 21)
    constexpr int32 MAX_SUBSTAT_POINTS_PER_PILLAR = MAX_WORLD_STAT_LEVEL * POINTS_PER_WORLD_STAT_LEVEL;

    // ==================== BASE STAT BUDGET ====================

    constexpr int32 INITIAL_STAT_BUDGET = 30;

    // ==================== MOVEMENT SPEED SCALING ====================

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