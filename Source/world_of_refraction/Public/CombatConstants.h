// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// ==================== COMBAT CONSTANTS ====================
// Pure constants - no UE reflection needed

namespace CombatConstants
{
    // ==================== WORLD STAT BONUSES ====================

    constexpr float WORLD_STAT_SCALING_BONUS = 0.01f; // 1% per level

    // ==================== STAT SCALING ====================
    // 13 Stats: Mind(4), Body(4), Spirit(5)
    // Formula: Base + (EffectiveStat × TotalPoints × PER_POINT)

    // ==================== MIND STATS (4) ====================
    // Efficiency, SpellDamage, CritChance, SpellSpeed

    // Efficiency - Reduces EP cost of Spells & Abilities (not Attacks)
    // Resonators: Also reduces ring break chance
    constexpr float EFFICIENCY_PER_POINT = 0.002f;            // 0.2% reduction per point
    constexpr float EFFICIENCY_MAX = 0.50f;                   // 50% max EP reduction
    constexpr float EFFICIENCY_RING_BREAK_PER_POINT = 0.003f; // 0.3% ring break reduction (Resonator only)
    constexpr float EFFICIENCY_RING_BREAK_MAX = 0.50f;        // 50% max ring break reduction

    // Status Multiplier — Status buildup amplification (Spirit-driven; substat
    // moved off Mind). Consumed by StatusBuildupManager::AddStatusBuildup and
    // the per-skill CalculateStatusBuildup helpers.
    // Halved from 0.002 → 0.001 to absorb the swap from the old
    // (1 + raw/100) shape to the pillar-scaled CalculateStatusMultiplier shape.
    constexpr float STATUS_MULTIPLIER_PER_POINT = 0.001f; // 0.1% per point

    // Spell Damage - Spell damage multiplier (applied once via DamageCalculator::GetAttackerDamageMultiplier for Spell ActionType)
    constexpr float SPELL_DAMAGE_PER_POINT = 0.004f; // 0.4% per point — doubled to compensate for the removal of the duplicate StatusMultiplier mult on spell damage

    // Crit Chance - Critical hit probability (all actions)
    constexpr float CRIT_CHANCE_BASE = 0.05f;        // 5% base
    constexpr float CRIT_CHANCE_PER_POINT = 0.0013f; // 0.13% per point
    constexpr float CRIT_CHANCE_MAX = 0.40f;         // 40% max
    constexpr float CRIT_DAMAGE_MULTIPLIER = 1.5f;   // 1.5x damage on crit

    // Spell Speed - Projectile travel speed (affects defender reaction time)
    constexpr float SPELL_SPEED_BASE = 1.0f;       // Base multiplier
    constexpr float SPELL_SPEED_PER_POINT = 0.01f; // 1% per point

    // ==================== BODY STATS (4) ====================
    // Defense, ActionSpeed, RawDamage, MaxHealth

    // Defense - Flat damage reduction per hit
    constexpr float DEFENSE_PER_POINT = 0.06f; // Flat defense per point

    // Action Speed - Approach speed & animation speed (RENAMED from AttackSpeed)
    constexpr float MOVEMENT_SPEED_BASE = 400.0f;       // Base units per second (approach)
    constexpr float MOVEMENT_SPEED_PER_POINT = 0.01f;   // 1% per point
    constexpr float ANIMATION_SPEED_BASE = 1.0f;        // Base animation multiplier
    constexpr float ANIMATION_SPEED_PER_POINT = 0.005f; // 0.5% per point

    // Raw Damage - Physical/non-elemental damage multiplier
    constexpr float RAW_DAMAGE_PER_POINT = 0.0008f; // 0.08% per point

    // Max Health - HP pool size (NOW BODY, was Spirit)
    constexpr float MAX_HEALTH_BASE = 100.0f;    // Base HP
    constexpr float MAX_HEALTH_PER_POINT = 5.0f; // +5 HP per point

    // ==================== SPIRIT STATS (5) ====================
    // MaxEnergy, Resistance, TurnSpeed, Luck, StatusMultiplier
    // (STATUS_MULTIPLIER_PER_POINT lives in the Mind block above for layout reasons —
    //  the substat itself is on the Spirit pillar.)

    // Max Energy - EP pool size (NEW explicit stat)
    constexpr float MAX_ENERGY_BASE = 50.0f;     // Base EP
    constexpr float MAX_ENERGY_PER_POINT = 2.0f; // +2 EP per point

    // Resistance - Reduces status effect damage & buildup (NOT elemental damage)
    constexpr float RESISTANCE_PER_POINT = 0.0015f; // 0.15% per point
    constexpr float RESISTANCE_MAX = 0.40f;         // 40% max

    // Turn Speed - Turn order priority (NOW SPIRIT, was Mind, no longer uses WorldBody)
    constexpr float TURN_SPEED_BASE = 10.0f;      // Base turn speed
    constexpr float TURN_SPEED_PER_POINT = 0.08f; // 0.08 per point

    // Luck - Multi-system fortune stat (drop chance/quality, crit bonus, dodge, break skip)
    constexpr float LUCK_PER_POINT = 0.0015f; // Same shape as Resistance
    constexpr float LUCK_RAW_MAX = 0.50f;     // Raw multiplier ceiling

    // Consumer-specific caps - applied at the consumer site
    constexpr float LUCK_CRIT_BONUS_MAX = 0.20f;   // +20% crit chance bonus
    constexpr float LUCK_DODGE_MAX = 0.25f;        // 25% per-hit dodge chance
    constexpr float LUCK_BREAK_SKIP_MAX = 0.50f;   // 50% chance to skip crystal wear
    constexpr float LUCK_DROP_CHANCE_MAX = 1.00f;  // 100% extra drop chance (out-of-combat)
    constexpr float LUCK_DROP_QUALITY_MAX = 1.00f; // 100% tier upgrade chance (out-of-combat)

    // ==================== TURN SYSTEM ====================

    constexpr float TURN_SPEED_THRESHOLD_DOUBLE = 15.0f; // Need +15 for 2:1 ratio
    constexpr int32 MAX_TURN_RATIO = 2;                  // Capped at 2:1

    // ==================== ABILITY SYSTEM ====================

    // Requirement Penalties
    constexpr float REQUIREMENT_PENALTY_SCALE = 0.10f; // sqrt(deficit) � this value
    constexpr float REQUIREMENT_PENALTY_MAX = 0.6f;    // 60% max penalty

    // Infusion System
    // INFUSION_DAMAGE_PENALTY removed per locked cost matrix — see commit message.
    constexpr float INFUSION_ENERGY_MULTIPLIER = 1.5f; // 50% more energy cost when infused
    constexpr int32 BASE_STATUS_BUILDUP_PER_HIT = 5;   // Base buildup before Spirit scaling
    constexpr float SPELL_L1_BUILDUP_MULT = 1.5f;      // L1 spell infusion: +50% buildup

    // ==================== DEFENSE-SIDE BUILDUP REDUCTION ====================
    // Buildup reduction when a target blocks/parries. Multipliers expressed as
    // "fraction of base buildup that gets through" (parallel to DefenseSystem's
    // current hardcoded damage multipliers). Dodge cancels buildup entirely —
    // handled by the multi-hit loop being skipped, no constant needed.
    constexpr float BLOCK_BUILDUP_MULTIPLIER = 0.5f; // Block: 50% buildup through
    constexpr float PARRY_BUILDUP_MULTIPLIER = 0.3f; // Parry: 30% buildup through

    // ==================== SPELL DATA ====================
    /** Raw mode damage bonus (+10%) */
    constexpr float RAW_MODE_DAMAGE_MULTIPLIER = 1.10f;

    // Status bar system
    // Status Effect Thresholds
    constexpr float STATUS_EFFECT_THRESHOLD = 100.0f; // Buildup needed to trigger status

    constexpr float STATUS_DECAY_RATE = 0.25f; // 25% per turn
    constexpr int32 STATUS_DECAY_FULL_RESET_TURNS = 3;

    // ==================== STAT-FORMULA HELPERS ====================
    // Final = Base * (1 + StatValue / STAT_PERCENT_DIVISOR). Used wherever
    // a raw sub-stat point value drives a percentage-style multiplier.
    constexpr float STAT_PERCENT_DIVISOR = 100.0f;
}