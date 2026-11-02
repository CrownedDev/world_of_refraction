// EffectBundles.h
// B-250 (DRY audit A10): flattening an item's referenced effect bundles (UEffectDefinition), once for gear and
// evolutions. The starting effects are the unconditional ones, the conditional ones wait for their trigger
// (FSkillEffect::IsConditionalEffect); null or unloaded definitions are skipped.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Effects/EffectDefinition.h"
#include "Skills/Effects/FGatheredEffect.h"

namespace EffectBundles
{
	/** Every effect of Defs that is (bConditional) or isn't conditional, in bundle order. */
	inline TArray<FSkillEffect> Flatten(const TArray<TObjectPtr<UEffectDefinition>> &Defs, bool bConditional)
	{
		TArray<FSkillEffect> Result;
		for (const TObjectPtr<UEffectDefinition> &Def : Defs)
		{
			if (!Def)
			{
				continue;
			}
			for (const FSkillEffect &Effect : Def->Effects)
			{
				if (Effect.IsConditionalEffect() == bConditional)
				{
					Result.Add(Effect);
				}
			}
		}
		return Result;
	}

	/** The same set, each with its definition's ID, its bundle index and the item's SourceKey (D-053). */
	inline TArray<FGatheredEffect> GatherWithIds(const TArray<TObjectPtr<UEffectDefinition>> &Defs, bool bConditional, int32 SourceKey)
	{
		TArray<FGatheredEffect> Result;
		for (const TObjectPtr<UEffectDefinition> &Def : Defs)
		{
			if (!Def)
			{
				continue;
			}
			const int32 DefID = static_cast<int32>(Def->GetUniqueID());
			for (int32 BundleIndex = 0; BundleIndex < Def->Effects.Num(); ++BundleIndex)
			{
				if (Def->Effects[BundleIndex].IsConditionalEffect() == bConditional)
				{
					Result.Emplace(DefID, BundleIndex, Def->Effects[BundleIndex], SourceKey);
				}
			}
		}
		return Result;
	}
}
