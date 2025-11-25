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
    // ==================== WORLD STAT LIMITS ====================

    // World stats range from 0-7 (E-Rank to S-Rank)
    constexpr int32 MIN_WORLD_STAT_LEVEL = 0;
    constexpr int32 MAX_WORLD_STAT_LEVEL = 7;

    // Points granted per world stat level (adjustable for balance)
    // Current: 3 points per level (Phase 1)
    // Planned: 10 points per level (Phase 2)
    constexpr int32 DEFAULT_POINTS_PER_WORLD_STAT = 3;

    // ==================== BASE STAT LIMITS ====================

    // Character creation distributable points (subject to change)
    constexpr int32 DEFAULT_DISTRIBUTABLE_POINTS = 30;

    // ==================== CALCULATED STAT RANGES ====================

    // Maximum world substat points per pillar at S-Rank (7 × 3 = 21)
    constexpr int32 MAX_WORLD_SUBSTATS_PER_PILLAR_PHASE1 = MAX_WORLD_STAT_LEVEL * 3;

    // Maximum world substat points per pillar at S-Rank (7 × 10 = 70) - Future
    constexpr int32 MAX_WORLD_SUBSTATS_PER_PILLAR_PHASE2 = MAX_WORLD_STAT_LEVEL * 10;

    // Total possible world substats at S-Rank (7/7/7)
    constexpr int32 MAX_TOTAL_WORLD_SUBSTATS_PHASE1 = MAX_WORLD_SUBSTATS_PER_PILLAR_PHASE1 * 3; // 63
    constexpr int32 MAX_TOTAL_WORLD_SUBSTATS_PHASE2 = MAX_WORLD_SUBSTATS_PER_PILLAR_PHASE2 * 3; // 210

    // ==================== PENALTY SYSTEM ====================

    // Maximum penalty for not meeting requirements (60%)
    constexpr float MAX_REQUIREMENT_PENALTY = 0.60f;

    // Penalty scaling factor (sqrt(deficit) × this value)
    constexpr float PENALTY_MULTIPLIER = 0.10f;

    // ==================== RANK THRESHOLDS ====================

    // These are informational - not enforced by code
    // Used for UI displays and rank badges
    constexpr int32 E_RANK_THRESHOLD = 1; // 1/1/1
    constexpr int32 D_RANK_THRESHOLD = 2; // 2/2/2
    constexpr int32 C_RANK_THRESHOLD = 3; // 3/3/3
    constexpr int32 B_RANK_THRESHOLD = 5; // 5/5/5
    constexpr int32 A_RANK_THRESHOLD = 6; // 6/6/6
    constexpr int32 S_RANK_THRESHOLD = 7; // 7/7/7 (MAX)
}