// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// ==================== COMBAT CONSTANTS ====================
// Pure constants - no UE reflection needed

namespace CombatConstants
{
    // ==================== WORLD STAT BONUSES ====================

    constexpr int32 WORLD_STAT_MAX_LEVEL = 7;         // Max level per stat
    constexpr float WORLD_STAT_SCALING_BONUS = 0.03f; // 3% per level
    constexpr int32 POINTS_PER_WORLD_STAT_LEVEL = 3;  // Sub-stat points generated

    // ==================== STAT SCALING ====================
    // 11 Stats: Mind(4), Body(3), Spirit(4)
    // Formula: Base + (EffectiveStat × TotalPoints × PER_POINT)

    // ==================== MIND STATS (4) ====================
    // Efficiency, EffectDamage, CritChance, SpellSpeed

    // Efficiency - Reduces EP cost of Spells & Abilities (not Attacks)
    // Resonators: Also reduces ring break chance
    constexpr float EFFICIENCY_PER_POINT = 0.002f;            // 0.2% reduction per point
    constexpr float EFFICIENCY_MAX = 0.50f;                   // 50% max EP reduction
    constexpr float EFFICIENCY_RING_BREAK_PER_POINT = 0.003f; // 0.3% ring break reduction (Resonator only)
    constexpr float EFFICIENCY_RING_BREAK_MAX = 0.50f;        // 50% max ring break reduction

    // Effect Damage - Spell damage & status effect potency (NOW MIND, was Spirit)
    constexpr float EFFECT_DAMAGE_PER_POINT = 0.002f; // 0.2% per point

    // Crit Chance - Critical hit probability (all actions)
    constexpr float CRIT_CHANCE_BASE = 0.05f;        // 5% base
    constexpr float CRIT_CHANCE_PER_POINT = 0.0013f; // 0.13% per point
    constexpr float CRIT_CHANCE_MAX = 0.40f;         // 40% max
    constexpr float CRIT_DAMAGE_MULTIPLIER = 1.5f;   // 1.5x damage on crit

    // Spell Speed - Projectile travel speed (affects defender reaction time)
    constexpr float SPELL_SPEED_BASE = 1.0f;       // Base multiplier
    constexpr float SPELL_SPEED_PER_POINT = 0.01f; // 1% per point

    // ==================== BODY STATS (3) ====================
    // Defense, MovementSpeed, RawDamage

    // Defense - Flat damage reduction per hit
    constexpr float DEFENSE_PER_POINT = 0.06f; // Flat defense per point

    // Movement Speed - Approach speed & animation speed (RENAMED from AttackSpeed)
    constexpr float MOVEMENT_SPEED_BASE = 400.0f;       // Base units per second (approach)
    constexpr float MOVEMENT_SPEED_PER_POINT = 0.01f;   // 1% per point
    constexpr float ANIMATION_SPEED_BASE = 1.0f;        // Base animation multiplier
    constexpr float ANIMATION_SPEED_PER_POINT = 0.005f; // 0.5% per point

    // Raw Damage - Physical/non-elemental damage multiplier
    constexpr float RAW_DAMAGE_PER_POINT = 0.0008f; // 0.08% per point

    // ==================== SPIRIT STATS (4) ====================
    // MaxEnergy, MaxHealth, Resistance, TurnSpeed

    // Max Energy - EP pool size (NEW explicit stat)
    constexpr float MAX_ENERGY_BASE = 50.0f;     // Base EP
    constexpr float MAX_ENERGY_PER_POINT = 2.0f; // +2 EP per point

    // Max Health - HP pool size (NEW explicit stat)
    constexpr float MAX_HEALTH_BASE = 100.0f;    // Base HP
    constexpr float MAX_HEALTH_PER_POINT = 5.0f; // +5 HP per point

    // Resistance - Reduces status effect damage & buildup (NOT elemental damage)
    constexpr float RESISTANCE_PER_POINT = 0.0015f; // 0.15% per point
    constexpr float RESISTANCE_MAX = 0.40f;         // 40% max

    // Turn Speed - Turn order priority (NOW SPIRIT, was Mind, no longer uses WorldBody)
    constexpr float TURN_SPEED_BASE = 10.0f;      // Base turn speed
    constexpr float TURN_SPEED_PER_POINT = 0.08f; // 0.08 per point

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