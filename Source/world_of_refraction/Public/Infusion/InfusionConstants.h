// InfusionConstants.h
// Constants for the infusion system

#pragma once

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

	// ==================== TIMING ====================

	/** Time to hold for Level 1 charge (seconds) */
	constexpr float LEVEL_1_CHARGE_TIME = 0.5f;

	/** Time to hold for Level 2 charge (seconds) */
	constexpr float LEVEL_2_CHARGE_TIME = 1.5f;

	// ==================== L2 COSTS (SOURCE-BASED) ====================

	/** Evolution L1 backlash: HP cost as percent of max (5%) */
	constexpr float EVOLUTION_L1_HP_COST_PERCENT = 0.05f;

	/** Evolution L2 backlash: HP cost as percent of max (10%) */
	constexpr float EVOLUTION_L2_HP_COST_PERCENT = 0.10f;

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

	/** L2 Spell Size: Damage multiplier (+30%) */
	constexpr float SPELL_L2_DAMAGE_MULT = 1.30f;

	// ==================== ENERGY COST MULTIPLIERS ====================
	// Ability and spell now share the same charge tax: L1 = half-again (x1.5,
	// +50%), L2 = double (x2.0, +100%). Kept as separate named constants —
	// ability-energy and spell-energy are conceptually distinct cost axes even
	// at equal values; do not merge.

	/** L1: Energy cost multiplier (1.5x, +50%) */
	constexpr float L1_ENERGY_MULT = 1.5f;

	/** L2: Energy cost multiplier (2.0x, +100%) */
	constexpr float L2_ENERGY_MULT = 2.0f;

	/** L1 Spell: Energy cost multiplier (1.5x, +50%) */
	constexpr float SPELL_L1_ENERGY_MULT = 1.5f;

	/** L2 Spell: Energy cost multiplier (2.0x, +100%) */
	constexpr float SPELL_L2_ENERGY_MULT = 2.0f;
}
