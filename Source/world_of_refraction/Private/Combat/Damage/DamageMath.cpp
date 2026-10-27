// DamageMath.cpp

#include "Combat/Damage/DamageMath.h"
#include "Combat/CombatConstants.h"

FDamageCalculationResult WoRDamageMath::Compute(const FDamageMathInputs &Inputs)
{
	FDamageCalculationResult Result;
	Result.EffectiveElement = Inputs.Element;

	if (Inputs.BaseDamage <= 0)
	{
		return Result;
	}

	float RunningDamage = static_cast<float>(Inputs.BaseDamage);

	// Step 1: attacker multiplier = stat (with ActionMods) + gear term + per-skill scaling.
	const ESubStat AttackerStat = Inputs.bIsSpell ? ESubStat::SpellDamage : ESubStat::RawDamage;
	float AttackerMult = Inputs.ActionMods.ApplyTo(Inputs.AttackerStatMultiplier, AttackerStat);
	AttackerMult += Inputs.EquipmentTermInMultiplier;

	float TierScalingBonus = 0.0f;
	for (const FDamageMathScalingTerm &Term : Inputs.ScalingTerms)
	{
		TierScalingBonus += Term.Coefficient * Term.StatFraction;
	}
	AttackerMult += TierScalingBonus;

	Result.AttackerDamageMultiplier = AttackerMult;
	RunningDamage *= AttackerMult;

	// Step 1.25: augment-stone factor (raw for attacks and abilities, spell for spells).
	if (Inputs.bHasAttackerStats)
	{
		RunningDamage *= Inputs.StoneFactor;
	}

	// Steps 1.5 and 2: grid position, then status effects.
	RunningDamage *= Inputs.GridDamageModifier;
	RunningDamage *= Inputs.StatusEffectModifier;

	// Step 2.5/2.6: Pattern P. The stat is capped alone at STAT_MULT_CAP, then gear multiplies it and
	// stone/transient apply, bounded by STAT_MODIFIER_MAX; the uncapped product already in
	// RunningDamage is divided out. Below the caps the correction is exactly 1.
	if (Inputs.bHasAttackerStats)
	{
		const float RawCharMod =
			(Inputs.EvolutionModifiedStat + Inputs.EquipmentTerm) * Inputs.StoneFactor * Inputs.TransientFactor;
		const float StatBase = FMath::Min(Inputs.EvolutionModifiedStat, CombatConstants::STAT_MULT_CAP);
		const float TargetCharMod =
			FMath::Clamp(StatBase * (1.0f + Inputs.EquipmentTerm) * Inputs.StoneFactor * Inputs.TransientFactor,
						 CombatConstants::STAT_MODIFIER_MIN, CombatConstants::STAT_MODIFIER_MAX);
		const float Correction = (RawCharMod > KINDA_SMALL_NUMBER) ? (TargetCharMod / RawCharMod) : 1.0f;
		RunningDamage *= Correction;
	}

	// Step 3: no elemental advantage system; always neutral.
	Result.ElementMultiplier = 1.0f;

	// Step 4: critical hit. ActionMods' crit-chance boost routes through Luck (5e-C2).
	if (Inputs.bCanCrit)
	{
		const float CritChance = Inputs.ActionMods.ApplyTo(Inputs.CritChance, ESubStat::Luck);
		Result.bWasCritical = Inputs.bForceCrit || (Inputs.CritRoll < CritChance);
		if (Result.bWasCritical)
		{
			Result.CritMultiplier = Inputs.CritDamageMultiplier;
			RunningDamage *= Inputs.CritDamageMultiplier;
		}
	}

	Result.DamageBeforeDefense = FMath::RoundToInt(RunningDamage);

	// Step 5: defense, a capped percent reduction.
	if (Inputs.bApplyDefense)
	{
		Result.DefenderFlatDefense = Inputs.DefenderDefense;
		const int32 PreDefenseDamage = FMath::RoundToInt(RunningDamage);
		RunningDamage *= (1.0f - Inputs.DefenderDefense);
		Result.DamageBlockedByDefense = PreDefenseDamage - FMath::RoundToInt(RunningDamage);
	}

	// Step 6.5: grid position defense (defender).
	if (Inputs.GridDefenseModifier > 0.0f)
	{
		RunningDamage /= Inputs.GridDefenseModifier;
	}

	// Step 7: minimum damage.
	Result.FinalDamage = FMath::Max(DamageConstants::MIN_DAMAGE, FMath::RoundToInt(RunningDamage));
	return Result;
}

FString WoRDamageMath::GetInputsString(const FDamageMathInputs &Inputs)
{
	FString Out;
	Out += FString::Printf(TEXT("Base %d  %s  element %d\n"), Inputs.BaseDamage,
						   Inputs.bIsSpell ? TEXT("spell") : TEXT("attack/ability"), static_cast<int32>(Inputs.Element));
	Out += FString::Printf(TEXT("Attacker stat x%.4f  gear in mult +%.4f  scaling terms %d\n"),
						   Inputs.AttackerStatMultiplier, Inputs.EquipmentTermInMultiplier, Inputs.ScalingTerms.Num());
	for (const FDamageMathScalingTerm &Term : Inputs.ScalingTerms)
	{
		Out += FString::Printf(TEXT("  coefficient %.4f  fraction %.4f\n"), Term.Coefficient, Term.StatFraction);
	}
	Out += FString::Printf(TEXT("Attacker stats %s: gear %.4f  stone x%.4f  transient x%.4f  evolution stat x%.4f\n"),
						   Inputs.bHasAttackerStats ? TEXT("yes") : TEXT("no"), Inputs.EquipmentTerm, Inputs.StoneFactor,
						   Inputs.TransientFactor, Inputs.EvolutionModifiedStat);
	Out += FString::Printf(TEXT("Grid x%.4f  status x%.4f\n"), Inputs.GridDamageModifier, Inputs.StatusEffectModifier);
	Out += FString::Printf(TEXT("Crit %s: chance %.4f  forced %s  roll %.4f  damage x%.4f\n"),
						   Inputs.bCanCrit ? TEXT("on") : TEXT("off"), Inputs.CritChance,
						   Inputs.bForceCrit ? TEXT("yes") : TEXT("no"), Inputs.CritRoll, Inputs.CritDamageMultiplier);
	Out += FString::Printf(TEXT("Defense %s: %.4f  grid defense %.4f\n"), Inputs.bApplyDefense ? TEXT("on") : TEXT("off"),
						   Inputs.DefenderDefense, Inputs.GridDefenseModifier);
	return Out;
}
