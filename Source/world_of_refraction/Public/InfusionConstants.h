// InfusionConstants.h
// Constants for the infusion system

#pragma once

namespace InfusionConstants
{
	// ==================== TIMING ====================

	/** Time to hold for Level 1 charge (seconds) */
	constexpr float LEVEL_1_CHARGE_TIME = 0.5f;

	/** Time to hold for Level 2 charge (seconds) */
	constexpr float LEVEL_2_CHARGE_TIME = 1.5f;

	// ==================== PHYSICAL INFUSION ====================

	/** L2 Physical: Damage multiplier (+30%) */
	constexpr float PHYSICAL_L2_DAMAGE_MULT = 1.30f;

	// ==================== ELEMENT INFUSION ====================

	/** L1 Element: Status buildup multiplier (+50%) */
	constexpr float ELEMENT_L1_STATUS_MULT = 1.50f;

	/** L2 Element: Status buildup multiplier (+100%) */
	constexpr float ELEMENT_L2_STATUS_MULT = 2.00f;

	/** L2 Element: Damage multiplier (+30%) */
	constexpr float ELEMENT_L2_DAMAGE_MULT = 1.30f;

	// ==================== L2 COSTS (SOURCE-BASED) ====================

	/** Innate source: HP cost as percent of max (5%) */
	constexpr float INNATE_L2_HP_COST_PERCENT = 0.05f;

	/** Ring source: Break chance increase for abilities (+15%) */
	constexpr float RING_L2_BREAK_INCREASE = 0.15f;

	/** Ring source: Break chance increase for spells (+20%) */
	constexpr float RING_L2_SPELL_BREAK_INCREASE = 0.20f;

	/** Crystal source: Break chance increase (+15%) */
	constexpr float CRYSTAL_L2_BREAK_INCREASE = 0.15f;

	/** Evolution source: HP cost as percent of max (3%) */
	constexpr float EVOLUTION_L2_HP_COST_PERCENT = 0.03f;

	/** Evolution source: Self-status buildup multiplier (50% of spell's base) */
	constexpr float EVOLUTION_L2_SELF_STATUS_MULT = 0.50f;

	// ==================== Iolite SPECIAL ====================

	/** Iolite L2: All stats buff (+5%) */
	constexpr float Iolite_L2_STAT_BUFF = 0.05f;

	// ==================== SPELL SIZE INFUSION ====================

	/** L1 Spell Size: Size multiplier (1.5x) */
	constexpr float SPELL_L1_SIZE_MULT = 1.50f;

	/** L2 Spell Size: Size multiplier (2.0x) */
	constexpr float SPELL_L2_SIZE_MULT = 2.00f;

	/** L2 Spell Size: Damage multiplier (+30%) */
	constexpr float SPELL_L2_DAMAGE_MULT = 1.30f;

	// ==================== ENERGY COST MULTIPLIERS ====================

	/** L1: Energy cost multiplier (1.15x) */
	constexpr float L1_ENERGY_MULT = 1.15f;

	/** L2: Energy cost multiplier (1.30x) */
	constexpr float L2_ENERGY_MULT = 1.30f;

	/** L1 Spell Size: Energy cost multiplier (1.30x) */
	constexpr float SPELL_L1_ENERGY_MULT = 1.30f;

	/** L2 Spell Size: Energy cost multiplier (1.60x) */
	constexpr float SPELL_L2_ENERGY_MULT = 1.60f;
}
