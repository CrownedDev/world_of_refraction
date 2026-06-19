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

    // ==================== DEFENSE DELTA AIM (PER-IMPACT) ====================
    // Aim offset for a synthesized per-impact defense press, expressed as a MULTIPLE of the
    // perfect half-band (Band = PerfectThreshold x impactMult). <1 = inside the perfect band,
    // >1 = outside it (whiffs perfect, may still catch the wider success window). Lower AI tier
    // aims looser. AI difficulty governs aim-WITHIN-band only; the band itself comes from the
    // impact's authored difficulty so it matches what MatchAndConsumeInput judges against.
    // Placeholders — Crown tunes in PIE.
    constexpr float EASY_DELTA_BAND_MULT = 3.0f;    // ~3x outside band → usually whiffs early
    constexpr float MEDIUM_DELTA_BAND_MULT = 1.6f;
    constexpr float HARD_DELTA_BAND_MULT = 0.9f;    // just inside the band edge
    constexpr float EXPERT_DELTA_BAND_MULT = 0.35f; // deep in perfect band

    // Randomized +/- fraction of the computed delta, so the AI isn't frame-perfect-deterministic.
    constexpr float DEFENSE_DELTA_JITTER_FRAC = 0.25f;

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

    // ==================== EMERALD (BONUS-TURN ITEM) VALUATION ====================
    // All Emerald scores are in HP-DAMAGE UNITS so they compare directly against
    // BuildOffensiveAction's ActionScores (~50-300). Do NOT reuse KILL_POTENTIAL_SCORE
    // (=2000) here — that is target-SELECTION scale and would dominate the action argmax.

    // Enemy-target: value of banking the DoT-secured kill, as a fraction of the
    // target's current HP (the damage the DoT pays "for free").
    constexpr float KILL_SECURE_FACTOR = 1.0f;

    // The target gets ONE free action before the DoT kills them at end-of-turn —
    // subtract this many turns of their threat.
    constexpr float FREE_ACTION_FACTOR = 1.0f;

    // How many upcoming turn-slots to scan for the target's next appearance when
    // sizing the rescue-exposure window (enemy turns before the target acts again).
    constexpr int32 EMERALD_EXPOSURE_LOOKAHEAD = 10;

    // Self-target: only fire when a held (unaffordable) action outscores the best
    // affordable one by at least this margin.
    constexpr float STARVE_MARGIN = 1.3f;

    // Discount on the held action's value because it is deferred DelayTurns turns:
    // DelayDiscount = 1 / (1 + DELAY_DECAY * DelayTurns).
    constexpr float DELAY_DECAY = 0.15f;

    // Estimated EP regained per turn, used by the self-target EP-regen lookahead
    // (CurrentEP + this*DelayTurns >= heldCost). The combat model has NO passive
    // per-turn EP regen (EP is gained only via absorption / items / skill effects),
    // so self-target Emerald-AI is DORMANT until passive EP regen exists; set >0 to
    // activate (the self-target premise depends on EP regenerating before the bonus
    // turn). Do NOT set a fabricated value — keep 0 until a real regen mechanic lands.
    constexpr int32 ESTIMATED_EP_REGEN_PER_TURN = 0;
}