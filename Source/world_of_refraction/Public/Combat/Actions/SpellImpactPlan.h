// SpellImpactPlan.h
// B-032d (Q-032d): how many impacts a spell lands on each target, and each impact's share of the damage.
// An impact is one arrival (a projectile landing, an AOE or Instant resolve), summed over the cast entries
// (RealTimeDefenseRework §8a). The asset HitCount names the per-impact split only when it equals that count.
// Debug: wor.Combat.PrintSpellImpacts lists the plan of every loaded spell.

#pragma once

#include "CoreMinimal.h"

class USpellData;
struct FSkillCastEntry;

/** One spell's per-target impact plan. */
struct WORLD_OF_REFRACTION_API FSpellImpactPlan
{
	/** Impacts each target takes: one per arrival, summed over the cast entries. */
	int32 ImpactsPerTarget = 1;
	/** The spell's own HitCount (at least 1). */
	int32 AuthoredHitCount = 1;
	/** HitCount equals the impacts, so the DamageSplit table gives each impact its share; else an even split. */
	bool bUsesDamageSplit = true;
	/** An enemy-targeted spell: its arrivals go through the per-impact resolve (support spells never do). */
	bool bOffensive = false;
};

namespace SpellImpactPlan
{
	/** Arrivals on each target from one cast entry: a projectile entry's Count (at least 1), else 1. */
	WORLD_OF_REFRACTION_API int32 ArrivalsOf(const FSkillCastEntry &Entry);

	/** The spell's plan. A spell with no cast entries (the loose delivery path) lands one arrival. */
	WORLD_OF_REFRACTION_API FSpellImpactPlan Make(const USpellData &Spell);

	/** The shape B-032 already resolved per impact: exactly one cast entry and HitCount 1. */
	WORLD_OF_REFRACTION_API bool IsSingleEntrySingleHit(const USpellData &Spell);

	/** The shapes B-032d adds: an offensive spell that is not single-entry single-hit. */
	WORLD_OF_REFRACTION_API bool IsWidened(const USpellData &Spell);

	/** The cast entry an impact ordinal belongs to, counting each entry's arrivals in CastArray order (a burst's
	 *  arrivals before the next entry's). INDEX_NONE for a spell with no cast entries or an ordinal past the plan. */
	WORLD_OF_REFRACTION_API int32 CastEntryOfImpact(const USpellData &Spell, int32 ImpactIndex);

	/** One impact's share of BaseDamage before defense: the DamageSplit percent when the table has one entry per
	 *  impact, else an even share with the remainder on the first impacts (the shares sum to BaseDamage). The one
	 *  formula ResolveImpactDefense and the per-impact close tail both use. */
	WORLD_OF_REFRACTION_API int32 ImpactBaseSlice(int32 BaseDamage, int32 ImpactCount, const TArray<float> &SplitPercents, int32 ImpactIndex);

	/** "Name: N impacts per target (E cast entries, HitCount H, split), per impact | at the window's close" for logs
	 *  and the console. */
	WORLD_OF_REFRACTION_API FString GetPlanString(const USpellData &Spell);
}
