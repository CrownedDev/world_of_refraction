// TurnRotation.h
// B-170b (ActionPips.md §3.2, Q-170d, Q-170f): the fixed turn rotation that replaces the speed timeline
// when wor.Turns.ActionPips is 1. The sides alternate, the leading side first (the party, or the side
// that struck first): P1 E1 P2 E2 ... Within a side, slot order (party-screen order, enemy spawn slot).
// A smaller side fills its empty seats in turn, and its rotation carries on across rounds (the
// deckbuilder's turn belt, WoR_TCG_Pivot_Design.md §7c and §8 items 53-56): 4 heroes vs 1 boss plays
// P1 B P2 B P3 B P4 B. First strike: the striking side takes a whole opening round, then the
// alternation starts with that side.
// UTurnManager owns one FTurnRotation and steps it from AdvanceSimState (the live turn and the belt
// preview share it, on a scratch copy for the belt). Debug: UTurnManager::GetTurnModeString,
// wor.Turns.PrintBelt.

#pragma once

#include "CoreMinimal.h"

struct FCombatantTurnDebt;

namespace TurnRotationConstants
{
	/** Safety bound on one pick: each step uses up a seat or builds a round, and skips are finite, so a
	 *  real fight never comes near it. */
	constexpr int32 MAX_PICK_STEPS = 256;

	/** The two sides of a fight (party, enemies). */
	constexpr int32 SIDE_COUNT = 2;
}

namespace TurnRotation
{
	/** wor.Turns.ActionPips (default 1 since B-170h): 1 = the fixed rotation and banked actions (B-170), 0 = the old Turn
	 *  Speed timeline until B-170h2 deletes it. UTurnManager reads it once per fight, at InitializeCombat. */
	WORLD_OF_REFRACTION_API bool IsActionPipsOn();
}

/** One seat of the current round. bEmptySeat: a smaller side's extra turn (not the holder's own), which
 *  passes to the next living teammate if its holder falls; an own seat is purged with its holder. */
struct FTurnRotationSeat
{
	int32 CombatantIndex = INDEX_NONE;
	bool bEmptySeat = false;
};

/** The rotation's state. Indexes into UTurnManager's Combatants (never reordered during a fight), so a
 *  scratch copy steps a scratch copy of the combatants the same way. Plain data: no UObject pointers. */
struct WORLD_OF_REFRACTION_API FTurnRotation
{
	/** The rest of the current round, front first. Empty = the next pick builds a round. */
	TArray<FTurnRotationSeat> RoundOrder;

	/** The side that opens every round: the party, or the side that struck first. */
	int32 LeadTeam = 0;

	/** Per side (team 0, team 1): the slot drawn last; the next draw takes the next living slot after it. */
	int32 LastDrawnSlot[TurnRotationConstants::SIDE_COUNT] = {INDEX_NONE, INDEX_NONE};

	/** First strike: the next round built is the striking side's opening round. */
	bool bOpeningRoundNext = false;

	/** Rounds built so far this fight (the opening round included). */
	int32 RoundsBuilt = 0;

	/** Fresh state for a fight. OpeningTeam INDEX_NONE = no first strike (the party leads). */
	void Reset(int32 OpeningTeam);

	/** The next turn's combatant index, or INDEX_NONE when nobody can act. Uses up seats of the dead
	 *  (own seats dropped, empty seats passed on) and seats removed by a skip (TurnsToSkip). Sets
	 *  *bOutRoundAdded when it builds a round. */
	int32 PickNext(TArray<FCombatantTurnDebt> &State, bool *bOutRoundAdded);

	/** A combatant fell: its own seats this round go, its empty seats pass to the next living teammate. */
	void OnCombatantDied(const TArray<FCombatantTurnDebt> &State, int32 CombatantIndex);

	/** "Round 3, lead team 0, rest of round: Hero, Boss, Mage(seat)". */
	FString ToString(const TArray<FCombatantTurnDebt> &State) const;

private:
	void BuildRound(TArray<FCombatantTurnDebt> &State);
	int32 DrawNext(const TArray<FCombatantTurnDebt> &State, int32 Team);
};
