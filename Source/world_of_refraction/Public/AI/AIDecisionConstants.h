// AIDecisionConstants.h
// Constants for AI decision making

#pragma once

#include "CoreMinimal.h"

namespace AIConstants
{
    // ==================== TARGET SCORING ====================

    constexpr int32 KILL_POTENTIAL_SCORE = 2000;
    constexpr float HP_MISSING_WEIGHT = 500.0f;
    constexpr float THREAT_WEIGHT = 2.0f;

    // ==================== STATUS BUILDUP SCORING ====================

    // Weight applied to a spell/ability's status-buildup score relative to its
    // damage score — status is secondary to damage but meaningful.
    constexpr float STATUS_SCORE_WEIGHT = 0.3f;

    // Status-score tiers (pre-weight, ~0..50 range).
    constexpr float STATUS_SCORE_TRIGGER = 50.0f;    // hit would trigger / bar near trigger
    constexpr float STATUS_SCORE_CONTRIBUTE = 12.0f; // meaningful buildup, bar not close
    constexpr float STATUS_SCORE_REDUNDANT = 5.0f;   // target already has a dangerous status

    // Threat calculation weights
    constexpr float RAW_DAMAGE_THREAT_MULT = 2.0f;
    constexpr float STATUS_MULTIPLIER_THREAT_MULT = 1.5f;
    constexpr float SPELL_POWER_THREAT_MULT = 2.0f;

    // ==================== THINKING DELAYS ====================

    constexpr float EASY_THINK_MIN = 2.0f;
    constexpr float EASY_THINK_MAX = 3.5f;

    constexpr float MEDIUM_THINK_MIN = 1.0f;
    constexpr float MEDIUM_THINK_MAX = 2.0f;

    constexpr float HARD_THINK_MIN = 0.5f;
    constexpr float HARD_THINK_MAX = 1.0f;

    constexpr float EXPERT_THINK_MIN = 0.2f;
    constexpr float EXPERT_THINK_MAX = 0.3f;

    // ==================== DEFENSE RATES ====================

    // Chance to attempt defense when window opens
    constexpr float EASY_DEFENSE_ATTEMPT = 0.40f;
    constexpr float MEDIUM_DEFENSE_ATTEMPT = 0.65f;
    constexpr float HARD_DEFENSE_ATTEMPT = 0.85f;
    constexpr float EXPERT_DEFENSE_ATTEMPT = 0.95f;

    // Timing accuracy (success rate when attempting)
    constexpr float EASY_DEFENSE_ACCURACY = 0.50f;
    constexpr float MEDIUM_DEFENSE_ACCURACY = 0.75f;
    constexpr float HARD_DEFENSE_ACCURACY = 0.90f;
    constexpr float EXPERT_DEFENSE_ACCURACY = 0.98f;

    // ==================== SURVIVAL THRESHOLDS ====================

    constexpr float SURVIVAL_HP_THRESHOLD = 0.25f;
    constexpr float ENERGY_CONSERVATION_THRESHOLD = 0.50f;
    constexpr float ENERGY_ABUNDANT_THRESHOLD = 0.70f;

    // ==================== DEFENSE REACTION TIMING ====================
    // Reaction delay as a fraction of the defense window, per difficulty
    // (parallel to THINK_MIN/MAX). Lower = reacts earlier in the window.
    constexpr float EASY_REACTION_FRACTION_MIN = 0.7f;
    constexpr float EASY_REACTION_FRACTION_MAX = 0.9f;

    constexpr float MEDIUM_REACTION_FRACTION_MIN = 0.4f;
    constexpr float MEDIUM_REACTION_FRACTION_MAX = 0.7f;

    constexpr float HARD_REACTION_FRACTION_MIN = 0.2f;
    constexpr float HARD_REACTION_FRACTION_MAX = 0.5f;

    constexpr float EXPERT_REACTION_FRACTION_MIN = 0.1f;
    constexpr float EXPERT_REACTION_FRACTION_MAX = 0.3f;

    // ==================== PARRY CHANCE ====================
    // Parry-vs-block choice; only the Hard/Expert "smart choice" path reaches this.
    constexpr float HARD_PARRY_CHANCE = 0.4f;
    constexpr float EXPERT_PARRY_CHANCE = 0.7f;

    // ==================== STATUS BAR ====================
    // Bar-fill fraction at/above which the AI treats the target as "near trigger".
    // Distinct from ENERGY_ABUNDANT_THRESHOLD despite the matching value.
    constexpr float STATUS_BAR_NEAR_TRIGGER_THRESHOLD = 0.70f;

    // ==================== HEAL HP THRESHOLDS ====================
    // HP fraction below which the AI prioritises healing. Hard/Expert heal earlier.
    // EASYMED value is intentionally separate from SURVIVAL_HP_THRESHOLD (same
    // number, different knob).
    constexpr float HARDEXPERT_HEAL_HP_THRESHOLD = 0.4f;
    constexpr float EASYMED_HEAL_HP_THRESHOLD = 0.25f;
}