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

    // Threat calculation weights
    constexpr float RAW_DAMAGE_THREAT_MULT = 2.0f;
    constexpr float EFFECT_DAMAGE_THREAT_MULT = 1.5f;
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
}