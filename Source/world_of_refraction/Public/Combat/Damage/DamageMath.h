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
	bool bApplyDefense = false;
	/** GetDefenderFlatDefense: a reduction fraction. */
	float DefenderDefense = 0.0f;
	/** Grid defense modifier; applied when above 0. */
	float GridDefenseModifier = 0.0f;
};

namespace WoRDamageMath
{
	/** The damage for Inputs. No actor, subsystem or random reads. */
	WORLD_OF_REFRACTION_API FDamageCalculationResult Compute(const FDamageMathInputs &Inputs);

	/** Every input, one line each. */
	WORLD_OF_REFRACTION_API FString GetInputsString(const FDamageMathInputs &Inputs);
}
