// WoRActionCosts.h
// B-043a (StatMathsRedesign.md §2.6, Q-043a): a skill's EP cost and its infusion HP backlash paid as
// Instant GameplayEffects (UWoRGE_EnergyCost, UWoRGE_HPBacklash). Behind `wor.Costs.UseGAS`
// (default 0; B-043h deletes it), only for a payer whose ASC has the Vitals set, which hands a GAS
// change to the game's EP / HP path (B-042g2: no longer also behind `wor.Effects.UseGAS`, now
// gone). Otherwise the callers pay the old way. Either way the amount is the one cost formula's
// (UActionExecutor::CalculateActionEnergyCost, the backlash stored at commit).
// Debug: Debug/GAS/WoRCostDebug.h (wor.Costs.Print).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"

class AActor;
class UAbilitySystemComponent;

namespace WoRCosts
{
	/** The value of `wor.Costs.UseGAS`; tests flip it with SetPaysThroughGAS. */
	WORLD_OF_REFRACTION_API bool PaysThroughGASEnabled();
	WORLD_OF_REFRACTION_API void SetPaysThroughGAS(bool bEnabled);

	/** True when Payer's costs go through the GEs: both switches on, authority, an ASC owned by Payer
	 *  with the Vitals set, and character data (what the Vitals set's hand-off needs). */
	WORLD_OF_REFRACTION_API bool PaysThroughGAS(const AActor *Payer);

	/** The EP cost spec, SetByCaller.Cost.Energy = -Amount. Invalid without an ASC. */
	WORLD_OF_REFRACTION_API FGameplayEffectSpecHandle MakeEnergyCostSpec(UAbilitySystemComponent *ASC, int32 Amount);

	/** The HP backlash spec, SetByCaller.Cost.Health = -Amount. Invalid without an ASC. */
	WORLD_OF_REFRACTION_API FGameplayEffectSpecHandle MakeBacklashSpec(UAbilitySystemComponent *ASC, int32 Amount);

	/** Pays Amount EP through UWoRGE_EnergyCost. False, with nothing paid, when PaysThroughGAS(Payer)
	 *  is false: the caller pays the old way. Amount <= 0 pays nothing and returns true. */
	WORLD_OF_REFRACTION_API bool TryPayEnergy(AActor *Payer, int32 Amount);

	/** Pays Amount HP through UWoRGE_HPBacklash (it can kill); false as TryPayEnergy. */
	WORLD_OF_REFRACTION_API bool TryPayBacklash(AActor *Payer, int32 Amount);
}
