// BuildupMath.h
// B-042l (ResistanceRedesign.md §3.4): the status buildup maths as one pure function over its inputs,
// as B-219 did for damage. UStatusBuildupManager::GatherBuildupMathInputs reads the inputs from the
// actors; GAS stage 3's buildup execution fills the attribute-backed ones from the ASCs. Compute runs
// AddStatusBuildup's amplification and resistance steps in the same order. The immunity gates and
// the bar update stay in AddStatusBuildup.
// Debug: WoRBuildupMath::GetInputsString.

#pragma once

#include "CoreMinimal.h"

/** Everything the buildup maths reads, already resolved from the actors. */
struct WORLD_OF_REFRACTION_API FBuildupMathInputs
{
	/** The incoming buildup, before any step. */
	float Amount = 0.0f;

	// ---- Source (each 1 when skipped or absent) ----
	/** GetEffectiveStatusMultiplier (step 5); 1 under bSkipBaseStatAmp. */
	float SourceStatusMultiplier = 1.0f;
	/** Element-keyed StatusMultiplierBuff - Debuff, as 1 + net / 100 floored at 0 (step 5b). */
	float ElementStatusMultiplier = 1.0f;
	/** Broken Darkness absorption-stack factor for the element (step 5c). */
	float StackMultiplier = 1.0f;

	// ---- Target (step 6; each 0 when the target has no character data) ----
	/** WoRResistance::GetGeneralStatusResistance (#1-3). */
	float GeneralStatusResistance = 0.0f;
	/** WoRResistance::GetHitTypeResistance for the hit's element and physical type (#4-6). */
	float HitTypeResistance = 0.0f;
	/** WoRResistance::GetStatusResistBonus (#7-8). */
	float StatusResistBonus = 0.0f;
};

namespace WoRBuildupMath
{
	/** The three resistance layers summed and clamped to [RESISTANCE_MIN, RESISTANCE_MAX]. */
	WORLD_OF_REFRACTION_API float GetTotalStatusResistance(const FBuildupMathInputs &Inputs);

	/** The buildup added to the bar: Amount x the source factors x (1 - total resistance). */
	WORLD_OF_REFRACTION_API float Compute(const FBuildupMathInputs &Inputs);

	WORLD_OF_REFRACTION_API FString GetInputsString(const FBuildupMathInputs &Inputs);
}
