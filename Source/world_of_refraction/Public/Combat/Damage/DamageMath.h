// DamageMath.h
// B-219 (StatMathsRedesign.md §4 step 2): the damage maths as one pure function over its inputs.
// UDamageCalculator::GatherDamageMathInputs reads the inputs from the actors today; GAS stage 3's
// damage execution (B-042) will fill the same struct from attributes. Compute runs the steps of
// UDamageCalculator::CalculateDamage in the same order with the same rounding, the Pattern P
// stat-cap correction included.
// Debug: WoRDamageMath::GetInputsString; UDamageMathDebug::PrintDamageMath.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Damage/DamageCalculator.h"

/** One authored per-skill scaling entry, resolved: its grade's coefficient and the attacker's stat
 *  as a fraction. */
struct FDamageMathScalingTerm
{
	/** GetScalingTierCoefficient(grade) (tuning data aware). */
	float Coefficient = 0.0f;
	/** GetScalingFraction(stat, attacker's effective stat). */
	float StatFraction = 0.0f;
};

/** Everything the damage maths reads, already resolved from the actors. */
struct WORLD_OF_REFRACTION_API FDamageMathInputs
{
	// ---- Action ----
	int32 BaseDamage = 0;
	bool bIsSpell = false;
	ESpellElement Element = ESpellElement::None;
	FActionStatModifiers ActionMods;

	// ---- Attacker ----
	/** GetAttackerDamageMultiplier (SpellDamage for spells, RawDamage otherwise). */
	float AttackerStatMultiplier = 1.0f;
	/** Gear term added to the attacker multiplier (0 when the attacker has no loadout). */
	float EquipmentTermInMultiplier = 0.0f;
	TArray<FDamageMathScalingTerm> ScalingTerms;
	/** False when the attacker has no character component: no stone factor, no stat cap. */
	bool bHasAttackerStats = false;
	/** The matching (spell or raw) values for the stat cap and stone step. */
	float EquipmentTerm = 0.0f;
	float StoneFactor = 1.0f;
	float TransientFactor = 1.0f;
	float EvolutionModifiedStat = 1.0f;
	float GridDamageModifier = 1.0f;
	float StatusEffectModifier = 1.0f;

	// ---- Crit ----
	bool bCanCrit = true;
	/** Override or GetCriticalChance, before ActionMods. */
	float CritChance = 0.0f;
	bool bForceCrit = false;
	/** The battle's random roll in [0, 1); read only when bCanCrit and not bForceCrit. */
	float CritRoll = 1.0f;
	float CritDamageMultiplier = 1.0f;

	// ---- Defender ----
	/** The defender's resistance to the hit's element (fraction, + resists, - weak; uncapped).
	 *  0 for non-elemental hits (D-005, D-036, Q-145). */
	float ElementResistance = 0.0f;
	/** Ignore Defense (input flag or the attacker's effect): also ignores positive element
	 *  resistance (Q-146b, all-or-nothing today). */
	bool bIgnoreDefense = false;
	bool bApplyDefense = false;
	/** GetDefenderFlatDefense: a reduction fraction. */
	float DefenderDefense = 0.0f;
	/** Grid defense modifier; applied when above 0. */
	float GridDefenseModifier = 0.0f;
};

namespace WoRDamageMath
{
	/** Element resistance scales elemental damage 1:1 up to this, either way (D-036). */
	constexpr float ELEMENT_DAMAGE_RESIST_CAP = 0.5f;

	/** The element step's multiplier: 1 - resistance, resistance capped at +-ELEMENT_DAMAGE_RESIST_CAP,
	 *  positive resistance ignored under Ignore Defense. */
	WORLD_OF_REFRACTION_API float GetElementMultiplier(float ElementResistance, bool bIgnoreDefense);

	/** The damage for Inputs. No actor, subsystem or random reads. */
	WORLD_OF_REFRACTION_API FDamageCalculationResult Compute(const FDamageMathInputs &Inputs);

	/** Every input, one line each. */
	WORLD_OF_REFRACTION_API FString GetInputsString(const FDamageMathInputs &Inputs);
}
