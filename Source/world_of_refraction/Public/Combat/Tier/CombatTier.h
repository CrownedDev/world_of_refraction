// CombatTier.h
// B-325 (D-118): every combatant's Combat Tier, F..S: the average tier of the gear that counts (B-306) and the
// skills it can use, rounded down to a letter (Q-325). Computed on demand, so it follows every loadout change.
// On an enemy it sets the tier-power HP (TrialGate) and its level drops; on a player it is shown only (no stats,
// pillars unchanged). UCharacterData::EnemyTier is the fallback when an actor has no gear and no skills.
// Debug: CombatTierDebug.h, `wor.Combat.PrintTiers`.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"

class AActor;

namespace WoRCombatTier
{
	/** The average of Tiers rounded down to a tier (Q-325, Destiny 2's Power); Fallback when Tiers is empty. */
	WORLD_OF_REFRACTION_API EItemTier GetAverageTier(TConstArrayView<EItemTier> Tiers, EItemTier Fallback);

	/** The tiers that count for Actor: each contributing weapon or adapter (instance tier), the primary evolution,
	 *  and every ability and spell its active loadout offers. Empty without a loadout. */
	WORLD_OF_REFRACTION_API void GatherTiers(const AActor *Actor, TArray<EItemTier> &OutTiers);

	/** Actor's Combat Tier: GetAverageTier of GatherTiers, else its UCharacterData::EnemyTier, else F. */
	WORLD_OF_REFRACTION_API EItemTier GetCombatTier(const AActor *Actor);

	/** "Tier C": the player-facing label. */
	WORLD_OF_REFRACTION_API FText GetCombatTierText(EItemTier Tier);
}
