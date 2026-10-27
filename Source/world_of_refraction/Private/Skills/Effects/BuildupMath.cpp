// BuildupMath.cpp

#include "Skills/Effects/BuildupMath.h"
#include "Combat/CombatConstants.h"

float WoRBuildupMath::GetTotalStatusResistance(const FBuildupMathInputs &Inputs)
{
	// Floor RESISTANCE_MIN: a net weakness amplifies buildup (up to 2x at -1). Ceiling
	// RESISTANCE_MAX: above 1 would heal the bar.
	const float Total = Inputs.GeneralStatusResistance + Inputs.HitTypeResistance + Inputs.StatusResistBonus;
	return FMath::Clamp(Total, CombatConstants::RESISTANCE_MIN, CombatConstants::RESISTANCE_MAX);
}

float WoRBuildupMath::Compute(const FBuildupMathInputs &Inputs)
{
	// AddStatusBuildup's order: source multiplier, element-keyed multiplier, stack factor, resistance.
	float Amount = Inputs.Amount;
	Amount *= Inputs.SourceStatusMultiplier;
	Amount *= Inputs.ElementStatusMultiplier;
	Amount *= Inputs.StackMultiplier;
	Amount *= (1.0f - GetTotalStatusResistance(Inputs));
	return Amount;
}

FString WoRBuildupMath::GetInputsString(const FBuildupMathInputs &Inputs)
{
	return FString::Printf(TEXT("Amount %.4f  source x%.4f  element x%.4f  stack x%.4f\n"
								"Resistance: general %.4f + hit type %.4f + bonus %.4f = %.4f (clamped)\n"
								"Buildup %.4f"),
						   Inputs.Amount, Inputs.SourceStatusMultiplier, Inputs.ElementStatusMultiplier, Inputs.StackMultiplier,
						   Inputs.GeneralStatusResistance, Inputs.HitTypeResistance, Inputs.StatusResistBonus,
						   GetTotalStatusResistance(Inputs), Compute(Inputs));
}
