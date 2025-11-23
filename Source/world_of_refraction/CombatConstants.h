// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// ==================== COMBAT CONSTANTS ====================
// Pure constants - no UE reflection needed

namespace CombatConstants
{
    // ==================== STAT SCALING ====================

    // Mind Scaling
    constexpr float COST_REDUCTION_PER_POINT = 0.006f;      // 0.6% per point
    constexpr float COST_REDUCTION_MAX = 0.7f;              // 70% max

    constexpr float TURN_SPEED_BASE = 10.0f;                // Base turn speed
    constexpr float TURN_SPEED_PER_POINT = 0.5f;            // Scaling per point

    constexpr float CRIT_CHANCE_BASE = 0.05f;               // 5% base
    constexpr float CRIT_CHANCE_PER_POINT = 0.003f;         // 0.3% per point
    constexpr float CRIT_CHANCE_MAX = 0.6f;                 // 60% max
    constexpr float CRIT_DAMAGE_MULTIPLIER = 1.5f;          // 1.5x damage

    // Body Scaling
    constexpr float DEFENSE_PER_POINT = 0.4f;               // Flat defense per point
    constexpr float ATTACK_SPEED_BASE = 1.0f;               // Base animation speed
    constexpr float ATTACK_SPEED_PER_POINT = 0.05f;         // 5% per point
    constexpr float RAW_DAMAGE_PER_POINT = 0.006f;          // 0.6% per point

    // Spirit Scaling
    constexpr float EFFECT_DAMAGE_PER_POINT = 0.006f;       // 0.6% per point
    constexpr float RESISTANCE_PER_POINT = 0.005f;          // 0.5% per point
    constexpr float RESISTANCE_MAX = 0.5f;                  // 50% max
    constexpr float ABILITY_SIZE_PER_POINT = 0.007f;        // 0.7% per point

    // ==================== WORLD STAT BONUSES ====================

    constexpr int32 WORLD_STAT_MAX_LEVEL = 7;              // Max level per stat
    constexpr float WORLD_STAT_SCALING_BONUS = 0.05f;      // 5% per level
    constexpr int32 POINTS_PER_WORLD_STAT_LEVEL = 3;       // Sub-stat points generated

    // ==================== TURN SYSTEM ====================

    constexpr float TURN_SPEED_THRESHOLD_DOUBLE = 15.0f;   // Need +15 for 2:1 ratio
    constexpr int32 MAX_TURN_RATIO = 2;                     // Capped at 2:1

    // ==================== STAT BUDGET ====================

    constexpr int32 STAT_BUDGET_MIN = 20;                   // Minimum distributable points
    constexpr int32 STAT_BUDGET_STANDARD = 30;              // Standard character
    constexpr int32 STAT_BUDGET_MAX = 40;                   // Maximum distributable points
}