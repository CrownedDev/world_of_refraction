// SeatRules.h
// B-346 (Q-21-audit, the deckbuilder's seat rule, WoR_TCG_Pivot_Design.md §8 items 23, 29, 80): who
// commands a party member's turn.
// - Solo: the player commands every member.
// - A member is AI only when the party leader sets it as an AI seat before the fight; that setting is
//   fixed for the fight (nothing hands a member over mid-fight, nothing auto-fills).
// - Co-op: a member claimed by a player is that player's; an unclaimed member's turns rotate through
//   its side's players in seat order (item 29), one player per turn.
// Pure functions so the rule is testable headless; the rotation counter is per-fight state and lives
// with the co-op seats (B-232, on the GameState), not here.

#pragma once

#include "CoreMinimal.h"

namespace SeatRules
{
	/** GetCommander's answer for a turn the AI plays. */
	constexpr int32 AI_COMMANDER = INDEX_NONE;

	/** No player has claimed the member (it rotates through its side's players). */
	constexpr int32 UNCLAIMED = INDEX_NONE;

	/** True when the member's turn is taken by the side's rotation (a non-AI member nobody claimed). */
	inline bool UsesRotation(bool bAISeat, int32 ClaimedBy)
	{
		return !bAISeat && ClaimedBy == UNCLAIMED;
	}

	/** The player id that commands the member's turn, or AI_COMMANDER.
	 *  SidePlayers: the side's player ids in seat order. RotationIndex: how many rotated turns the side
	 *  has taken so far this fight (advance it after a turn for which UsesRotation is true). A claim by
	 *  a player no longer on the side falls back to the rotation; a side with no players is AI. */
	inline int32 GetCommander(bool bAISeat, int32 ClaimedBy, const TArray<int32> &SidePlayers, int32 RotationIndex)
	{
		if (bAISeat || SidePlayers.Num() == 0)
		{
			return AI_COMMANDER;
		}
		if (ClaimedBy != UNCLAIMED && SidePlayers.Contains(ClaimedBy))
		{
			return ClaimedBy;
		}
		return SidePlayers[FMath::Abs(RotationIndex) % SidePlayers.Num()];
	}
}
