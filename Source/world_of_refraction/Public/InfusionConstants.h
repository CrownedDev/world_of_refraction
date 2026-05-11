// InfusionConstants.h
// Constants for the infusion system

#pragma once

namespace InfusionConstants
{

	// ==================== CHARGE HP COSTS (NEW) ====================
	// HP cost deducted on charge START (committed cost)
	// Applies to both Spell and Ability charge infusion
	// Uses CURRENT HP, not max HP

	/** L1 Charge: HP cost as percent of current HP (5%) */
	constexpr float CHARGE_L1_HP_COST_PERCENT = 0.05f;

	/** L2 Charge: HP cost as percent of current HP (10%) */
	constexpr float CHARGE_L2_HP_COST_PERCENT = 0.10f;

	// ==================== CHARGE EFFECT MULTIPLIERS (NEW) ====================
	// Exclusive bonuses - L1 OR L2, not both

	/** L1 Charge: Status buildup multiplier (+25%) - spells and abilities */
	constexpr float CHARGE_L1_STATUS_MULT = 1.25f;

	/** L2 Charge: Damage multiplier (+30%) - spells and abilities */
	constexpr float CHARGE_L2_DAMAGE_MULT = 1.30f;

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

	/** L1: Energy cost multiplier (1.15x) */
	constexpr float L1_ENERGY_MULT = 1.15f;

	/** L2: Energy cost multiplier (1.30x) */
	constexpr float L2_ENERGY_MULT = 1.30f;

	/** L1 Spell Size: Energy cost multiplier (1.30x) */
	constexpr float SPELL_L1_ENERGY_MULT = 1.30f;

	/** L2 Spell Size: Energy cost multiplier (1.60x) */
	constexpr float SPELL_L2_ENERGY_MULT = 1.60f;

	// ==================== ATTACK INFUSION ====================

	/** Flat energy cost paid by an infused basic attack (no charge-level concept). */
	constexpr int32 ATTACK_INFUSION_ENERGY_COST = 5;
}
