// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// ==================== COMBAT CONSTANTS ====================
// Pure constants - no UE reflection needed
//
// IMPORTANT: This header is included widely across the module. Do NOT add
// game-type dependencies (e.g. enum-switching helpers that pull in ItemTier.h)
// — keep this file to plain constexpr values and tiny math-only helpers.
// Anything that switches on EItemTier lives in EquipmentBonusGenerator.h.

namespace CombatConstants
{
    // ==================== MOTION WARP (W1) ====================
    // The single warp-target name both legs retarget (approach -> return,
    // sequential, never concurrent — the spike's validated pattern). Must
    // match the Warp Target Name field of the Motion Warping notify-state
    // window authored on root-motion montages. Montages without a window
    // ignore the target entirely (in-place content unaffected).
    constexpr const TCHAR *WARP_TARGET_NAME = TEXT("WarpTarget");

    // ==================== SPELL SIZE DEFAULTS (D6 Stage 12) ====================
    // Empty-CastArray fallbacks. EXACT by construction: the Stage 11 PostLoad
    // migration creates an entry whenever ANY delivery field is non-default,
    // so an empty-CastArray spell necessarily carries the old defaults
    // (BaseSize 1.0 / HitboxRatio 0.8) — these reproduce them.
    constexpr float DEFAULT_SPELL_SIZE = 1.0f;         // old BaseSize default (visual / defense seed)
    constexpr float DEFAULT_SPELL_HITBOX_RATIO = 0.8f; // old HitboxRatio default

    // ==================== WORLD STAT BONUSES ====================
    // Per-pillar scaling rates — applied by GetEffectiveMind/Body/Spirit as
    // BasePillar × (1 + WorldLevel × <PillarScalingBonus>). Split out of the
    // single WORLD_STAT_SCALING_BONUS so each pillar can be tuned independently.
    // Bumped 0.01 -> 0.07 in the decoupling rework: with the substat-sum removed from
    // GetEffectiveX, world level is now the meaningful pillar scaler (+7%/level, +49% at
    // world 7). See CombatEconomy_StatRedesign.md.
    constexpr float WORLD_MIND_SCALING_BONUS = 0.07f;   // 7% per WorldMindLevel
    constexpr float WORLD_BODY_SCALING_BONUS = 0.07f;   // 7% per WorldBodyLevel
    constexpr float WORLD_SPIRIT_SCALING_BONUS = 0.07f; // 7% per WorldSpiritLevel

    // ==================== EQUIPMENT STAT BONUS LIMITS ====================
    // Reference bounds for FEquipmentStatBonus authoring. UPROPERTY meta clamps
    // accept only string literals, so these constants are the source of truth
    // for IsDataValid range checks; field-level clamps must mirror the values.
    constexpr int32 EQUIPMENT_BONUS_MAX = 21;   // Weapon/ring int field ceiling (ClampMin=0 baked in)
    constexpr int32 CRYSTAL_BONUS_MIN   = -21;  // Crystal int fields permit negatives (cursed gear)
    constexpr int32 CRYSTAL_BONUS_MAX   = 21;
    constexpr float PILLAR_MODIFIER_MIN = -15.0f; // Pillar percent floor (Mind/Body/Spirit)
    constexpr float PILLAR_MODIFIER_MAX = 15.0f;

    // Substat generation budgets per tier (capacity points distributed across
    // the 13 int substat fields by UEquipmentBonusGenerator).
    constexpr int32 SUBSTAT_BUDGET_F = 6;
    constexpr int32 SUBSTAT_BUDGET_E = 10;
    constexpr int32 SUBSTAT_BUDGET_D = 15;
    constexpr int32 SUBSTAT_BUDGET_C = 21;
    constexpr int32 SUBSTAT_BUDGET_B = 28;
    constexpr int32 SUBSTAT_BUDGET_A = 36;
    constexpr int32 SUBSTAT_BUDGET_S = 45;

    // Pillar modifier generation budgets per tier (percent magnitude, sum of
    // abs across the 3 BonusMind/Body/SpiritModifierPercent fields).
    constexpr float PILLAR_BUDGET_F = 3.0f;
    constexpr float PILLAR_BUDGET_E = 5.0f;
    constexpr float PILLAR_BUDGET_D = 8.0f;
    constexpr float PILLAR_BUDGET_C = 12.0f;
    constexpr float PILLAR_BUDGET_B = 18.0f;
    constexpr float PILLAR_BUDGET_A = 25.0f;
    constexpr float PILLAR_BUDGET_S = 33.0f;

    // Per-stat cap fraction — within a pillar's substat allocation, no single
    // substat may exceed (pillar_points × SUBSTAT_CAP_FRACTION). Pillar cap
    // similarly bounds any single pillar modifier to (tier_budget × PILLAR_CAP_FRACTION).
    constexpr float SUBSTAT_CAP_FRACTION = 0.40f;
    constexpr float PILLAR_CAP_FRACTION  = 0.40f;

    // ==================== GEAR RESISTANCE ROLL (own pool) ====================
    // Per-tier net point budget for the gear resistance roll (FResistanceBonus),
    // distributed zero-sum across the 12 resistance categories (9 elements + 3
    // physical types). SEPARATE pool from SUBSTAT_BUDGET — rolling resistance does
    // NOT compete with stat substats. Values are percent points (the class table's
    // 15/10/5 scale). Conservative first pass — tune in PIE.
    constexpr int32 RESISTANCE_BUDGET_F = 8;
    constexpr int32 RESISTANCE_BUDGET_E = 12;
    constexpr int32 RESISTANCE_BUDGET_D = 16;
    constexpr int32 RESISTANCE_BUDGET_C = 20;
    constexpr int32 RESISTANCE_BUDGET_B = 25;
    constexpr int32 RESISTANCE_BUDGET_A = 30;
    constexpr int32 RESISTANCE_BUDGET_S = 36;

    // Per-category |delta| cap for a single resistance roll. THE key balance knob:
    // sized so stacked gear (weapon+ring+evolution = up to 3 pieces, each ≤ this)
    // plus the class profile (single-category max 15) cannot trivially reach the
    // +100% status-immunity ceiling — 3×10 + 15 = 45, a wide margin from +100.
    // Conservative default; tune in PIE.
    constexpr int32 RESISTANCE_CATEGORY_CAP = 10;

    // Hard per-category field clamp on the stored value (BaseResistance +
    // GeneratedResistance), mirroring CRYSTAL_BONUS_MIN/MAX's role. Percent.
    constexpr float RESISTANCE_BONUS_MIN = -15.0f;
    constexpr float RESISTANCE_BONUS_MAX = 15.0f;

    // Per-asset MaxPool override sentinel: Stat/ResistanceMaxPoolOverride == this
    // means "use the tier-derived budget"; any positive value overrides it.
    // Consumed by the pickup roll (UInventoryComponent::AddWeapon/AddRing).
    constexpr int32 POOL_OVERRIDE_USE_TIER = 0;

    // Tier→budget switch helpers live in EquipmentBonusGenerator.h to keep
    // this header free of EItemTier (ItemTier.h) so it can stay a leaf-level
    // header included everywhere without ramifying game-type dependencies.

    // ==================== STAT SCALING ====================
    // 13 Stats: Mind(4), Body(4), Spirit(5)
    // Formula: Base + (EffectiveStat × TotalPoints × PER_POINT)
    //
    // DECOUPLED model: EffectiveX is now the world multiplier only (~1.0-1.49). Per-point
    // constants DERIVE from (cap − base) / STAT_DERIVE_DENOM so MAX investment hits the
    // design cap exactly. Change a cap → the per-point auto-updates. No magic numbers.
    // The CAP constants below are REUSED as the clamp bounds in cluster 2 (single source
    // of truth). Tune FEEL in PIE. See CombatEconomy_StatRedesign.md.
    constexpr float MAX_STAT_POINTS = 93.0f;                                // 30 base + 7×3 levels × 3 pillars
    constexpr float WORLD_MAX_MULT = 1.0f + 7.0f * WORLD_BODY_SCALING_BONUS; // 1.49 — max world multiplier (derived from the world bonus)
    constexpr float STAT_DERIVE_DENOM = MAX_STAT_POINTS * WORLD_MAX_MULT;    // ≈138.57 — max investment scalar

    // Shared multiplier-stat family (base 1.0 -> cap 1.5): Raw/Spell damage, Status, all speeds.
    constexpr float STAT_MULT_BASE = 1.0f;
    constexpr float STAT_MULT_CAP  = 1.5f;
    constexpr float STAT_MULT_PER_POINT = (STAT_MULT_CAP - STAT_MULT_BASE) / STAT_DERIVE_DENOM;

    // Shared %-stat family (base 0 -> cap 0.5): Defense, Resistance, Efficiency(+ring-break), Luck.
    constexpr float UNIVERSAL_STAT_CAP = 0.5f;
    constexpr float PCT_STAT_PER_POINT = (UNIVERSAL_STAT_CAP - 0.0f) / STAT_DERIVE_DENOM;

    // ==================== MIND STATS (4) ====================
    // Efficiency, SpellDamage, CritDamage, SpellSpeed

    // Efficiency - Reduces EP cost of Spells & Abilities (not Attacks)
    // Resonators: Also reduces ring break chance
    constexpr float EFFICIENCY_PER_POINT = PCT_STAT_PER_POINT; // -> 0.50 EP-cost reduction at max
    constexpr float EFFICIENCY_MAX = 0.50f;                   // 50% max EP reduction (stat-alone cap, Pattern P)
    // Pattern-P gear ceiling (cluster 5b): the stat-derived reduction caps ALONE at EFFICIENCY_MAX
    // (0.5); gear/stone/buff then MULTIPLY it past 0.5 toward this ceiling. 0.90 = floor of 10% cost
    // (abilities never fully free). FLAG: Crown may raise to 1.0 (gear can make abilities free).
    constexpr float EFFICIENCY_GEAR_CEILING = 0.90f;          // max reduction WITH gear (>= EFFICIENCY_MAX)
    constexpr float EFFICIENCY_RING_BREAK_PER_POINT = PCT_STAT_PER_POINT; // -> 0.50 ring-break reduction at max (Resonator only)
    constexpr float EFFICIENCY_RING_BREAK_MAX = 0.50f;        // 50% max ring break reduction

    // Status Multiplier — Status buildup amplification (Spirit-driven; substat
    // moved off Mind). Consumed by StatusBuildupManager::AddStatusBuildup and
    // the per-skill CalculateStatusBuildup helpers. Multiplier-stat family member.
    constexpr float STATUS_MULTIPLIER_PER_POINT = STAT_MULT_PER_POINT; // -> x1.5 status buildup at max

    // Spell Damage - Spell damage multiplier (applied once via DamageCalculator::GetAttackerDamageMultiplier for Spell ActionType)
    constexpr float SPELL_DAMAGE_PER_POINT = STAT_MULT_PER_POINT; // -> x1.5 spell damage at max

    // Crit Chance - Critical hit probability (all actions). Cluster 5e-C2: crit chance is now
    // LUCK-driven (not the Mind crit stat). Base 0.05 + normalized Luck (stat caps at 1.0) x
    // CRIT_CHANCE_LUCK_BONUS -> 0.50 at maxed Luck stat; Luck gear pushes the norm past 1.0 toward
    // the [0,1] crit ceiling (100%).
    constexpr float CRIT_CHANCE_BASE = 0.05f;        // 5% base
    constexpr float CRIT_CHANCE_LUCK_BONUS = 0.45f;  // maxed-Luck-stat crit chance bonus (0.05 + 0.45 = 0.50)
    // (5e-C3 retired CRIT_CHANCE_PER_POINT — crit chance left the Mind stat for Luck; and the dead
    //  CRIT_DAMAGE_MULTIPLIER duplicate. Crit damage uses CRIT_DMG_BASE / CRIT_DAMAGE_PER_POINT below.)
    // Crit DAMAGE stat scaling (cluster 5e-B-fix, Crown-locked). Un-invested crit = CRIT_DMG_BASE
    // (x1.0 = NORMAL damage — a crit does nothing extra without crit-damage investment). The
    // crit-damage stat ramps it to x1.5 at max (base 1.0 + 0.5), gear pushes past toward x2.0.
    constexpr float CRIT_DMG_BASE = 1.0f;            // un-invested crit = normal damage
    constexpr float CRIT_DAMAGE_STAT_CAP = 1.5f;     // crit-damage stat-ALONE ceiling (1.0 base + 0.5 stat)
    // Same per-point rate as RawDamage's 1.0->1.5 ramp = (CRIT_DAMAGE_STAT_CAP - CRIT_DMG_BASE)/DENOM,
    // which equals STAT_MULT_PER_POINT exactly — so the stat lands on x1.5 at max investment.
    constexpr float CRIT_DAMAGE_PER_POINT = STAT_MULT_PER_POINT; // (1.5-1.0)/STAT_DERIVE_DENOM
    // Pattern-P gear ceiling: stat caps at x1.5; gear/transient (ModifyCritDamage now, BonusCritDamage
    // post-5e-C3) MULTIPLY the capped stat past 1.5 toward this.
    constexpr float CRIT_DAMAGE_GEAR_CEILING = 2.0f; // max crit damage WITH gear (>= CRIT_DAMAGE_STAT_CAP)

    // Spell Speed - Projectile travel speed (affects defender reaction time)
    constexpr float SPELL_SPEED_BASE = 1.0f;       // Base multiplier
    constexpr float SPELL_SPEED_PER_POINT = STAT_MULT_PER_POINT; // -> x1.5 cast speed at max

    // ==================== BODY STATS (5) ====================
    // Defense, ActionSpeed, RawDamage, MaxHealth, Reflex

    // Defense - Flat damage reduction per hit
    constexpr float DEFENSE_PER_POINT = PCT_STAT_PER_POINT; // -> 0.50 at max (becomes % reduction in cluster 4)

    // Action Speed - Approach speed & animation speed (RENAMED from AttackSpeed)
    constexpr float MOVEMENT_SPEED_BASE = 400.0f;       // Base units per second (approach)
    constexpr float MOVEMENT_SPEED_PER_POINT = STAT_MULT_PER_POINT; // -> x1.5 approach speed at max
    constexpr float ANIMATION_SPEED_BASE = 1.0f;        // Base animation multiplier
    constexpr float ANIMATION_SPEED_PER_POINT = STAT_MULT_PER_POINT; // -> x1.5 anim play rate at max

    // Raw Damage - Physical/non-elemental damage multiplier
    constexpr float RAW_DAMAGE_PER_POINT = STAT_MULT_PER_POINT; // -> x1.5 physical damage at max

    // Augment-stone consumable buff/debuff duration (DamageStone, DefenseStone, ...)
    constexpr int32 AUGMENT_STONE_CONSUMABLE_DURATION = 3; // turns, flat across tiers

    // Max Health - HP pool size (NOW BODY, was Spirit)
    constexpr float MAX_HEALTH_BASE = 100.0f;    // Base HP
    constexpr float MAX_HEALTH_CAP  = 1000.0f;   // HP ceiling at max investment (clamp bound, cluster 2)
    constexpr float MAX_HEALTH_PER_POINT = (MAX_HEALTH_CAP - MAX_HEALTH_BASE) / STAT_DERIVE_DENOM; // 100 -> 1000

    // Reflex - Widens the defense input window (additive seconds layered ON TOP of
    // UDefenseSystem::DefenseInputWindow; base stays tunable on the subsystem)
    constexpr float WINDOW_CAP_SECONDS = 0.25f; // +0.25s = +50% of the 0.5s base window (clamp bound, cluster 2)
    constexpr float REFLEX_WINDOW_PER_POINT = (WINDOW_CAP_SECONDS - 0.0f) / STAT_DERIVE_DENOM; // -> +0.25s at max

    // Defense-window DUEL — the attacker's speed NARROWS the defender's input window the same way
    // Reflex WIDENS it: window = max(MINIMUM, base + defenderReflexBonus − attackerSpeedPenalty).
    // WINDOW_PER_SPEED_POINT is seeded EQUAL to REFLEX_WINDOW_PER_POINT so equal points + equal world
    // level cancel to the base window; kept a SEPARATE constant so the offense/defense ratio can be
    // tuned independently in playtest. MINIMUM_DEFENSE_WINDOW floors it so a fast attacker stays
    // barely defendable, never undefendable.
    constexpr float WINDOW_PER_SPEED_POINT = REFLEX_WINDOW_PER_POINT;  // = REFLEX_WINDOW_PER_POINT for symmetric duel cancel + shared +50% cap
    constexpr float MINIMUM_DEFENSE_WINDOW = 0.1f;   // hard floor on the effective input window (seconds)

    // ==================== SPIRIT STATS (5) ====================
    // MaxEnergy, Resistance, TurnSpeed, Luck, StatusMultiplier
    // (STATUS_MULTIPLIER_PER_POINT lives in the Mind block above for layout reasons —
    //  the substat itself is on the Spirit pillar.)

    // Max Energy - EP pool size (NEW explicit stat)
    constexpr float MAX_ENERGY_BASE = 50.0f;     // Base EP
    constexpr float MAX_ENERGY_CAP  = 1000.0f;   // EP ceiling at max investment (clamp bound, cluster 2)
    constexpr float MAX_ENERGY_PER_POINT = (MAX_ENERGY_CAP - MAX_ENERGY_BASE) / STAT_DERIVE_DENOM; // 50 -> 1000

    // Resistance - Reduces status effect damage & buildup (NOT elemental damage)
    constexpr float RESISTANCE_PER_POINT = PCT_STAT_PER_POINT; // -> 0.50 status resistance at max (UNIVERSAL_STAT_CAP; RESISTANCE_MAX stays the 1.0 hard ceiling)
    // Correctness bound, NOT a design/balance cap: status resistance must stay
    // <= 1.0, else the consumer `Amount *= (1 - Resistance)` goes negative and
    // heals the gauge. Do not "tune" this down — it is a hard ceiling, not a knob.
    constexpr float RESISTANCE_MAX = 1.0f;
    // Negative resistance amplifies status buildup; -1.0 = double (2x) at full
    // vulnerability. Lower bound of the buildup-resistance clamp — resistance
    // debuffs strip through 0 into amplification, capped here. Paired with
    // RESISTANCE_MAX as the [MIN, MAX] clamp in StatusBuildupManager.
    constexpr float RESISTANCE_MIN = -1.0f;

    // Turn Speed - Turn order priority (NOW SPIRIT, was Mind, no longer uses WorldBody)
    constexpr float TURN_SPEED_BASE = 10.0f;      // Base turn speed
    constexpr float TURN_SPEED_CAP  = 15.0f;      // stat-alone turn-speed ceiling at max investment (Pattern P; base +50%)
    constexpr float TURN_SPEED_PER_POINT = (TURN_SPEED_CAP - TURN_SPEED_BASE) / STAT_DERIVE_DENOM; // 10 -> 15
    // Pattern-P gear ceiling (cluster 5c): the stat-derived turn speed caps ALONE at TURN_SPEED_CAP
    // (15); dedicated BonusTurnSpeed gear then MULTIPLIES it past 15 toward this ceiling. 20 mirrors
    // the damage stat×1.5→gear×2.0 logic (base 10 → +50% stat → +100% with gear). Differential vs a
    // maxed no-gear rival = 20-15 = 5, well under TURN_SPEED_THRESHOLD_DOUBLE (15), so the 2:1 ratio
    // stays out of trivial reach. FLAG: Crown may tune (22-30 = more gear weight, nearer the 2:1 gate).
    constexpr float TURN_SPEED_GEAR_CEILING = 20.0f; // max turn speed WITH gear (>= TURN_SPEED_CAP)

    // Luck - Multi-system fortune stat (drop chance/quality, crit bonus, dodge, break skip)
    constexpr float LUCK_PER_POINT = PCT_STAT_PER_POINT; // -> 0.50 raw luck at max (= LUCK_RAW_MAX normalization basis)
    constexpr float LUCK_RAW_MAX = 0.50f;     // Raw stat ceiling — stat normalizes to exactly 1.0 here at max investment
    // Pattern-P gear ceiling (cluster 5e-C1): the stat portion normalizes to 1.0 ALONE; gear (BonusLuck)
    // + transient (LuckBuff/Debuff) MULTIPLY it past 1.0 toward this normalized ceiling. Consumers then
    // scale by their own max bonus and clamp at their own 100% (crit chance, break skip, dodge, drops).
    constexpr float LUCK_GEAR_CEILING = 2.0f; // normalized luck ceiling WITH gear (>= 1.0 stat cap)

    // Consumer-specific caps - applied at the consumer site
    // (5e-C3 retired LUCK_CRIT_BONUS_MAX — crit chance is now Luck itself via CRIT_CHANCE_LUCK_BONUS,
    //  not a +20% bonus layered on a Mind-stat crit chance.)
    constexpr float LUCK_DODGE_MAX = 0.25f;        // 25% per-hit dodge chance
    constexpr float LUCK_BREAK_SKIP_MAX = 0.50f;   // 50% chance to skip crystal wear
    constexpr float LUCK_DROP_CHANCE_MAX = 1.00f;  // 100% extra drop chance (out-of-combat)
    constexpr float LUCK_DROP_QUALITY_MAX = 1.00f; // 100% tier upgrade chance (out-of-combat)
    // TODO: LUCK_DODGE_MAX / LUCK_DROP_CHANCE_MAX / LUCK_DROP_QUALITY_MAX have no
    // consumer code yet. When built, each MUST upper-clamp the normalized fraction
    // FMath::Min(RawLuck / LUCK_RAW_MAX, 1.f) before scaling — clamp only the TOP,
    // letting negatives pass (curse-below-baseline model). RawLuck is unbounded
    // (input cap removed); positive luck plateaus at the consumer MAX, negative
    // luck curses below it. Mirror the crit (DamageCalculator) / break-skip
    // (CrystalManager) pattern, and confirm the consumer handles a negative result
    // gracefully (e.g. a roll FRand() < negative simply never fires).

    // ==================== TURN SYSTEM ====================

    constexpr float TURN_SPEED_THRESHOLD_DOUBLE = 15.0f; // Need +15 for 2:1 ratio
    constexpr int32 MAX_TURN_RATIO = 2;                  // Capped at 2:1

    // Materialized upcoming-turn depth (UTurnManager::TurnBelt). Largest live
    // consumer asks for 10 (AI Emerald lookahead, debug print); 16 gives headroom.
    constexpr int32 TURN_BELT_HORIZON = 16;

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

    // [-100%, +100%] normalization range for a composed stat modifier, expressed as a
    // [0, 2] multiplier (0.0 = −100% / fully nullified, 1.0 = unchanged, 2.0 = +100% /
    // doubled). The hard cap on a modifier-bearing stat's COMPOSED product (the product of
    // its layer factors, excluding the per-character base). Byte-identical below the cap.
    constexpr float STAT_MODIFIER_MIN = 0.0f;
    constexpr float STAT_MODIFIER_MAX = 2.0f;

    // ==================== BD OVERLOAD UI THRESHOLDS ====================
    // CharacterPanelWidget tints the EP text when CurrentEP > MaxEP (BD
    // overload). Thresholds are CurrentEP / MaxEP ratios; the bar percent
    // itself is clamped at 1.0 (visual), the underlying value can exceed it.
    // The overload window is hard-capped at MaxEP + 30% (BrokenDarkness
    // OVERLOAD_CAPACITY_FRACTION), so the meaningful ratio range is
    // [1.00, 1.30]. Thresholds are scaled to fit that window — red sits at
    // 1.20 so it's reachable before the hard cap.
    // Below YELLOW = no tint (white). Above RED = red. Bands between.
    constexpr float OVERLOAD_YELLOW_THRESHOLD = 1.00f; // 101%+ → yellow
    constexpr float OVERLOAD_ORANGE_THRESHOLD = 1.10f; // 111%+ → orange
    constexpr float OVERLOAD_RED_THRESHOLD    = 1.20f; // 121%+ → red
}