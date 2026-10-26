// FleeRules.h
// D-091 (Crown, 2026-10-26): you can flee a normal fight, not a mini-boss or boss, and fleeing
// costs a share of the party's run Gold.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Encounter/EEncounterRank.h"

namespace FleeRules
{
	/** Share of each fleeing party member's Gold that is lost. */
	constexpr float GOLD_LOSS_SHARE = 0.10f; // TODO: tune (Q-176)

	inline bool CanFleeRank(EEncounterRank Rank)
	{
		return Rank == EEncounterRank::Normal;
	}

	/** Gold lost from a wallet holding Gold (rounded down, so a small purse can lose nothing). */
	inline int32 GoldLost(int32 Gold)
	{
		return Gold > 0 ? FMath::FloorToInt(Gold * GOLD_LOSS_SHARE) : 0;
	}
}
