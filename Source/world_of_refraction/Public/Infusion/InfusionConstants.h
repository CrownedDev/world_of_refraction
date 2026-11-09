// InfusionConstants.h
// Constants for the infusion system

#pragma once

#include "CoreMinimal.h"

namespace InfusionConstants
{

	// ==================== CHARGE HP COSTS ====================
	// RETIRED (rework 6-2-3): the flat 5%/10%-of-current-HP charge cost is replaced
	// by an EP-derived basis — HP = (pre-Efficiency infused EP / MaxEP) × MaxHP ×
	// (1 − Resistance), computed in UInfusionCostHelper::CalculateHPCost.

	// ==================== CHARGE EFFECT BONUSES (MODE-ROUTED) ====================
	// Infusion charge effect bonuses (multiplier form). The mode (EInfusionMode) routes which pair
	// applies to damage vs status: Physical -> damage=focus / status=off-focus; Status -> reversed;
	// Balanced -> the middle to both. L1/L2 progressive (both scale at both levels).

	/** The mode's focused effect, L1 (+15%) */
	constexpr float CHARGE_FOCUS_L1 = 1.15f;

	/** Focused effect, L2 (+30%) */
	constexpr float CHARGE_FOCUS_L2 = 1.30f;

	/** The off-axis effect, L1 (+10%) */
	constexpr float CHARGE_OFFFOCUS_L1 = 1.10f;

	/** Off-axis effect, L2 (+20%) */
	constexpr float CHARGE_OFFFOCUS_L2 = 1.20f;

	/** Balanced: the middle to both, L1 (+12.5%) */
	constexpr float CHARGE_BALANCED_L1 = 1.125f;

	/** Balanced: the middle to both, L2 (+25%) */
	constexpr float CHARGE_BALANCED_L2 = 1.25f;

	// ==================== LEVELS ====================

	/** Highest infusion level a pick can carry (levels run 0..2; 0 is no infusion). */
	constexpr int32 MAX_INFUSION_LEVEL = 2;

	// ==================== L2 COSTS (SOURCE-BASED) ====================

	// Evolution backlash HP cost is EP-based (UInfusionCostHelper::CalculateHPCost, ED-32).

	/** Evolution L1 backlash: Self-status build (flat amount) — pending status mapping */
	constexpr float EVOLUTION_L1_SELF_STATUS_BUILD = 15.0f;

	/** Evolution L2 backlash: Self-status build (flat amount) — pending status mapping */
	constexpr float EVOLUTION_L2_SELF_STATUS_BUILD = 25.0f;
	// ==================== Iolite SPECIAL ====================

	/** Iolite L2: All stats buff (+5%) */
	constexpr float IOLITE_L2_STAT_BUFF = 0.05f;

	// ==================== SPELL SIZE INFUSION ====================

	/** L1 Spell Size: Size multiplier (1.5x) */
	constexpr float SPELL_L1_SIZE_MULT = 1.50f;

	/** L2 Spell Size: Size multiplier (2.0x) */
	constexpr float SPELL_L2_SIZE_MULT = 2.00f;

	// ==================== ENERGY COST MULTIPLIERS ====================
	// OBSOLETE (B-334, D-126; delete in the D-124 sweep): the x1.5 / x2.0 charge tax. Infusion EP is now
	// the base x the charge level (ChargeLevelCostMultiplier below). Read only by the obsolete
	// UActionExecutor::GetSpellInfusionCostMultiplier / GetAbilityChargeCostMultiplier.

	/** L1: Energy cost multiplier (1.5x, +50%) */
	constexpr float L1_ENERGY_MULT = 1.5f;

	/** L2: Energy cost multiplier (2.0x, +100%) */
	constexpr float L2_ENERGY_MULT = 2.0f;

	/** L1 Spell: Energy cost multiplier (1.5x, +50%) */
	constexpr float SPELL_L1_ENERGY_MULT = 1.5f;

	/** L2 Spell: Energy cost multiplier (2.0x, +100%) */
	constexpr float SPELL_L2_ENERGY_MULT = 2.0f;

	// ==================== CHARGE COSTS (D-126) ====================
	// B-334 (D-126, Q-334): the infusion source choice stays; the EP cost is the base x the charge level
	// (L0 and L1 x1, L2 x2), and the HP cost (Raw, Innate spell, Evolution sources) applies only at L2.

	/** The cost multiplier with no charge: the action pays its base EP. */
	constexpr float UNCHARGED_COST_MULTIPLIER = 1.0f;

	/** The lowest charge level that pays the HP cost. */
	constexpr int32 HP_COST_MIN_LEVEL = 2;

	/** EP multiplier for a charge level: the level itself (clamped to MAX_INFUSION_LEVEL), x1 with no charge. */
	inline float ChargeLevelCostMultiplier(int32 Level)
	{
		return Level <= 0 ? UNCHARGED_COST_MULTIPLIER : static_cast<float>(FMath::Min(Level, MAX_INFUSION_LEVEL));
	}

	/** True when an infusion at this level pays the HP cost (D-126: level 2 only). */
	inline bool LevelPaysHP(int32 Level)
	{
		return Level >= HP_COST_MIN_LEVEL;
	}
}
