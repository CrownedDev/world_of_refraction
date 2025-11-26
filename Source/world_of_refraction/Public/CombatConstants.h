// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// ==================== COMBAT CONSTANTS ====================
// Pure constants - no UE reflection needed

namespace CombatConstants
{
    // ==================== STAT SCALING ====================

    // HP/EP Calculations
    constexpr int32 BASE_HP = 100;
    constexpr int32 HP_PER_BODY = 10;
    constexpr int32 BASE_EP = 50;
    constexpr int32 EP_PER_SPIRIT = 5;

    // Mind Scaling
    constexpr float COST_REDUCTION_PER_POINT = 0.0007f; // 0.07% per point (was 0.6%)
    constexpr float COST_REDUCTION_MAX = 0.5f;
    constexpr float COST_REDUCTION_MIN = 0.3f; // 50% max (was 70%)

    constexpr float TURN_SPEED_BASE = 10.0f;      // Base turn speed
    constexpr float TURN_SPEED_PER_POINT = 0.08f; // 0.08 per point (was 0.5)

    constexpr float CRIT_CHANCE_BASE = 0.05f;        // 5% base
    constexpr float CRIT_CHANCE_PER_POINT = 0.0013f; // 0.13% per point (was 0.3%)
    constexpr float CRIT_CHANCE_MAX = 0.4f;          // 40% max (was 60%)
    constexpr float CRIT_DAMAGE_MULTIPLIER = 1.5f;   // 1.5x damage

    // Body Scaling
    constexpr float DEFENSE_PER_POINT = 0.06f;       // Flat defense per point (was 0.4)
    constexpr float ATTACK_SPEED_BASE = 1.0f;        // Base animation speed
    constexpr float ATTACK_SPEED_PER_POINT = 0.005f; // 0.5% per point (was 5%)
    constexpr float RAW_DAMAGE_PER_POINT = 0.0008f;  // 0.08% per point (was 0.6%)

    // Spirit Scaling
    constexpr float EFFECT_DAMAGE_PER_POINT = 0.002f; // 0.2% per point (was 0.6%)
    constexpr float RESISTANCE_PER_POINT = 0.0015f;   // 0.15% per point (was 0.5%)
    constexpr float RESISTANCE_MAX = 0.4f;            // 40% max (was 50%)
    constexpr float ABILITY_SIZE_PER_POINT = 0.005f;  // 0.5% per point (was 0.7%)

    // ==================== WORLD STAT BONUSES ====================

    constexpr int32 WORLD_STAT_MAX_LEVEL = 7;         // Max level per stat
    constexpr float WORLD_STAT_SCALING_BONUS = 0.03f; // 3% per level
    constexpr int32 POINTS_PER_WORLD_STAT_LEVEL = 3;  // Sub-stat points generated

    // ==================== TURN SYSTEM ====================

    constexpr float TURN_SPEED_THRESHOLD_DOUBLE = 15.0f; // Need +15 for 2:1 ratio
    constexpr int32 MAX_TURN_RATIO = 2;                  // Capped at 2:1

    // ==================== STAT BUDGET ====================

    constexpr int32 STAT_BUDGET_MIN = 20;      // Minimum distributable points
    constexpr int32 STAT_BUDGET_STANDARD = 30; // Standard character
    constexpr int32 STAT_BUDGET_MAX = 40;      // Maximum distributable points

    // ==================== ABILITY SYSTEM ====================

    // Requirement Penalties
    constexpr float REQUIREMENT_PENALTY_SCALE = 0.10f; // sqrt(deficit) � this value
    constexpr float REQUIREMENT_PENALTY_MAX = 0.6f;    // 60% max penalty

    // Infusion System
    constexpr float INFUSION_DAMAGE_PENALTY = 0.30f;   // 30% damage reduction when infused
    constexpr float INFUSION_ENERGY_MULTIPLIER = 1.5f; // 50% more energy cost when infused
    constexpr int32 BASE_STATUS_BUILDUP_PER_HIT = 5;   // Base buildup before Spirit scaling

    // Status Effect Thresholds
    constexpr int32 STATUS_EFFECT_THRESHOLD = 100; // Buildup needed to trigger status
}