// TrialLoot.h
// B-188 (Resources_Design §7.9, Q-188): a loot pool is a tag query. Items (weapons, rings, spells,
// abilities) carry SourceTags; a chest rolls one item whose tags match its zone's pool.
// TAG = filter, ROLL = selector: the tag picks the candidates, a seeded roll picks one.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AActor;

namespace TrialLoot
{
	/** The item's SourceTags, or null when Item isn't a lootable kind. */
	const FGameplayTagContainer *GetSourceTags(const UObject *Item);

	/** The item's authored name (empty when Item isn't a lootable kind). */
	FText GetItemName(const UObject *Item);

	/** Every authored weapon, ring, spell and ability whose SourceTags match Pool (the asset
	 *  registry, loaded on demand). Empty for an invalid Pool.
	 *  TODO: a packaged build cooks only referenced assets; add them to the Asset Manager's
	 *  primary asset rules before the first public build. */
	TArray<UObject *> GatherPool(const FGameplayTag &Pool);

	/** One of the Candidates whose tags match Pool, picked by a roll seeded with Seed; null when
	 *  none match. TODO: weight the roll (drop chance, Luck) like the enemy-kit draw (§7.9). */
	UObject *PickFromPool(const TArray<UObject *> &Candidates, const FGameplayTag &Pool, int32 Seed);

	/** Put Item into Owner's inventory (a weapon or ring added, a spell or ability learned).
	 *  False when Owner has no inventory, the kind is unknown, or it can't take it. */
	bool GrantTo(AActor *Owner, UObject *Item);
}
