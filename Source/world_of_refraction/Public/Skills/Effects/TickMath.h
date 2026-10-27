// TickMath.h
// B-042n: a DOT's and a HOT's per-tick maths as pure functions, as B-219 did for damage and B-042l for
// buildup. USkillEffectManager's ticks read the inputs from the actors; GAS stage 3's tick execution
// (UWoRTickExecution) fills them from the ASCs at the same moment, the holder's turn start.
// Debug: WoRTickMath::GetInputsString.

#pragma once

#include "CoreMinimal.h"

/** Everything one tick reads, already resolved. */
struct WORLD_OF_REFRACTION_API FTickMathInputs
{
	/** The effect's stacked value: damage or heal before any step. */
	float Value = 0.0f;

	// ---- DOT ----
	/** Sized from the holder's max HP (the status-bar cap DOT): no element resistance (Q-064). */
	bool bShareOfMaxHP = false;
	/** WoRResistance::GetElementDamageResistance for the DOT's element. */
	float ElementResistance = 0.0f;

	// ---- HOT ----
	/** The holder's HealingReceivedBuff total, in percent. */
	float HealingReceivedPercent = 0.0f;
};

namespace WoRTickMath
{
	/** One DOT tick: Value x the element multiplier (1 for a share of max HP), rounded. */
	WORLD_OF_REFRACTION_API int32 GetDotTickDamage(const FTickMathInputs &Inputs);

	/** One HOT tick: Value x (1 + healing received / 100), rounded. */
	WORLD_OF_REFRACTION_API int32 GetHotTickHeal(const FTickMathInputs &Inputs);

	WORLD_OF_REFRACTION_API FString GetInputsString(const FTickMathInputs &Inputs);
}
