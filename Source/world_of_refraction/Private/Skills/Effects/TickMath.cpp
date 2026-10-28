// TickMath.cpp

#include "Skills/Effects/TickMath.h"
#include "Combat/CombatConstants.h"
#include "Combat/Damage/DamageMath.h"

namespace
{
	/** A DOT tick doesn't ignore any of the holder's resistance. */
	constexpr float TICK_IGNORE_DEFENSE = 0.0f;
}

int32 WoRTickMath::GetDotTickDamage(const FTickMathInputs &Inputs)
{
	const float Multiplier =
		Inputs.bShareOfMaxHP ? 1.0f : WoRDamageMath::GetElementMultiplier(Inputs.ElementResistance, TICK_IGNORE_DEFENSE);
	return FMath::RoundToInt(Inputs.Value * Multiplier);
}

int32 WoRTickMath::AmplifyHeal(float Amount, float HealingReceivedPercent)
{
	return FMath::RoundToInt(Amount * (1.0f + HealingReceivedPercent / CombatConstants::STAT_PERCENT_DIVISOR));
}

int32 WoRTickMath::GetHotTickHeal(const FTickMathInputs &Inputs)
{
	return AmplifyHeal(Inputs.Value, Inputs.HealingReceivedPercent);
}

FString WoRTickMath::GetInputsString(const FTickMathInputs &Inputs)
{
	return FString::Printf(TEXT("Value %.4f  share of max HP=%d  element resistance %.4f  healing received %.2f%%\n"
								"DOT tick %d  HOT tick %d"),
						   Inputs.Value, Inputs.bShareOfMaxHP, Inputs.ElementResistance, Inputs.HealingReceivedPercent,
						   GetDotTickDamage(Inputs), GetHotTickHeal(Inputs));
}
