// TurnRotation.cpp
// B-170b: the fixed turn rotation (see TurnRotation.h). Log-free: the belt preview replays it 16 times
// per rebuild; UTurnManager logs the live turn.

#include "Combat/TurnRotation.h"
#include "Combat/TurnManager.h"
#include "Character/CharacterDataComponent.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarTurnsActionPips(
		TEXT("wor.Turns.ActionPips"),
		0,
		TEXT("B-170: 1 = fixed turn rotation and banked actions (ActionPips.md); 0 = the Turn Speed timeline as built. ")
		TEXT("Read when a fight starts, so set it before the fight."),
		ECVF_Default);

	constexpr int32 TURN_ROTATION_SIDE_COUNT = TurnRotationConstants::SIDE_COUNT;

	bool IsTurnRotationSide(int32 Team)
	{
		return Team >= 0 && Team < TURN_ROTATION_SIDE_COUNT;
	}

	int32 TurnRotationOtherSide(int32 Team)
	{
		return Team == TurnManagerTeams::LOCAL_TEAM_INDEX ? TurnManagerTeams::OPPOSING_TEAM_INDEX : TurnManagerTeams::LOCAL_TEAM_INDEX;
	}

	bool IsTurnRotationCombatantLiving(const TArray<FCombatantTurnDebt> &State, int32 Index)
	{
		if (!State.IsValidIndex(Index) || !IsValid(State[Index].Actor))
		{
			return false;
		}
		const UCharacterDataComponent *CharComp = State[Index].Actor->FindComponentByClass<UCharacterDataComponent>();
		return CharComp && CharComp->bIsAlive;
	}

	int32 CountTurnRotationLiving(const TArray<FCombatantTurnDebt> &State, int32 Team)
	{
		int32 Living = 0;
		for (int32 Index = 0; Index < State.Num(); ++Index)
		{
			if (State[Index].TeamIndex == Team && IsTurnRotationCombatantLiving(State, Index))
			{
				++Living;
			}
		}
		return Living;
	}

	/** The living member of Team in the next slot after AfterSlot (wrapping to the lowest slot), never
	 *  ExcludeIndex. INDEX_NONE when the side has nobody else living. */
	int32 NextTurnRotationLivingAfter(const TArray<FCombatantTurnDebt> &State, int32 Team, int32 AfterSlot, int32 ExcludeIndex)
	{
		int32 NextAfter = INDEX_NONE;
		int32 Lowest = INDEX_NONE;
		for (int32 Index = 0; Index < State.Num(); ++Index)
		{
			if (Index == ExcludeIndex || State[Index].TeamIndex != Team || !IsTurnRotationCombatantLiving(State, Index))
			{
				continue;
			}
			const int32 Slot = State[Index].PositionInTeam;
			if (Lowest == INDEX_NONE || Slot < State[Lowest].PositionInTeam)
			{
				Lowest = Index;
			}
			if (Slot > AfterSlot && (NextAfter == INDEX_NONE || Slot < State[NextAfter].PositionInTeam))
			{
				NextAfter = Index;
			}
		}
		return NextAfter != INDEX_NONE ? NextAfter : Lowest;
	}
}

bool TurnRotation::IsActionPipsOn()
{
	return CVarTurnsActionPips.GetValueOnGameThread() != 0;
}

void FTurnRotation::Reset(int32 OpeningTeam)
{
	RoundOrder.Reset();
	LeadTeam = IsTurnRotationSide(OpeningTeam) ? OpeningTeam : TurnManagerTeams::LOCAL_TEAM_INDEX;
	bOpeningRoundNext = IsTurnRotationSide(OpeningTeam);
	for (int32 Side = 0; Side < TURN_ROTATION_SIDE_COUNT; ++Side)
	{
		LastDrawnSlot[Side] = INDEX_NONE;
	}
	RoundsBuilt = 0;
}

int32 FTurnRotation::DrawNext(const TArray<FCombatantTurnDebt> &State, int32 Team)
{
	const int32 Index = NextTurnRotationLivingAfter(State, Team, LastDrawnSlot[Team], INDEX_NONE);
	if (Index != INDEX_NONE)
	{
		LastDrawnSlot[Team] = State[Index].PositionInTeam;
	}
	return Index;
}

void FTurnRotation::BuildRound(TArray<FCombatantTurnDebt> &State)
{
	RoundOrder.Reset();
	++RoundsBuilt;

	// First strike: the striking side's opening round, each living member once.
	if (bOpeningRoundNext)
	{
		bOpeningRoundNext = false;
		const int32 Strikers = CountTurnRotationLiving(State, LeadTeam);
		for (int32 Seat = 0; Seat < Strikers; ++Seat)
		{
			FTurnRotationSeat Opening;
			Opening.CombatantIndex = DrawNext(State, LeadTeam);
			RoundOrder.Add(Opening);
		}
		if (RoundOrder.Num() > 0)
		{
			return;
		}
	}

	// The opening round (if any) is over: the waiting side joins the rotation.
	for (FCombatantTurnDebt &Combatant : State)
	{
		Combatant.bWaitsForOpening = false;
	}

	// Sides alternate, the lead side first. The larger side's count sets the seats per side; the smaller
	// side fills its extra seats by carrying on its own rotation (deckbuilder items 53-55).
	const int32 OtherTeam = TurnRotationOtherSide(LeadTeam);
	const int32 SeatsPerSide = FMath::Max(CountTurnRotationLiving(State, LeadTeam), CountTurnRotationLiving(State, OtherTeam));
	TArray<int32> SeatedThisRound;
	for (int32 Seat = 0; Seat < SeatsPerSide; ++Seat)
	{
		for (const int32 Team : {LeadTeam, OtherTeam})
		{
			const int32 Index = DrawNext(State, Team);
			if (Index == INDEX_NONE)
			{
				continue; // this side has nobody living
			}
			FTurnRotationSeat Entry;
			Entry.CombatantIndex = Index;
			Entry.bEmptySeat = SeatedThisRound.Contains(Index);
			SeatedThisRound.AddUnique(Index);
			RoundOrder.Add(Entry);
		}
	}
}

int32 FTurnRotation::PickNext(TArray<FCombatantTurnDebt> &State, bool *bOutRoundAdded)
{
	for (int32 Step = 0; Step < TurnRotationConstants::MAX_PICK_STEPS; ++Step)
	{
		if (RoundOrder.Num() == 0)
		{
			BuildRound(State);
			if (RoundOrder.Num() == 0)
			{
				return INDEX_NONE; // nobody living on either side
			}
			if (bOutRoundAdded)
			{
				*bOutRoundAdded = true;
			}
		}

		const FTurnRotationSeat Seat = RoundOrder[0];
		RoundOrder.RemoveAt(0);

		int32 Index = Seat.CombatantIndex;
		if (!IsTurnRotationCombatantLiving(State, Index))
		{
			// A holder that fell without OnCombatantDied (e.g. its actor was destroyed): the same rule.
			if (!Seat.bEmptySeat || !State.IsValidIndex(Index))
			{
				continue;
			}
			Index = NextTurnRotationLivingAfter(State, State[Index].TeamIndex, State[Index].PositionInTeam, Index);
			if (Index == INDEX_NONE)
			{
				continue;
			}
		}

		// A skip removes the holder's next turn (ET-13): the seat is used up without happening.
		if (State[Index].TurnsToSkip > 0)
		{
			State[Index].TurnsToSkip--;
			continue;
		}
		return Index;
	}
	return INDEX_NONE;
}

void FTurnRotation::OnCombatantDied(const TArray<FCombatantTurnDebt> &State, int32 CombatantIndex)
{
	if (!State.IsValidIndex(CombatantIndex))
	{
		return;
	}
	const FCombatantTurnDebt &Fallen = State[CombatantIndex];
	for (int32 SeatIndex = RoundOrder.Num() - 1; SeatIndex >= 0; --SeatIndex)
	{
		FTurnRotationSeat &Seat = RoundOrder[SeatIndex];
		if (Seat.CombatantIndex != CombatantIndex)
		{
			continue;
		}
		// Own turn: purged (a revive re-enters from the next round). Empty seat: the next living teammate
		// takes it, in the same place (deckbuilder item 56).
		const int32 Heir = Seat.bEmptySeat
							   ? NextTurnRotationLivingAfter(State, Fallen.TeamIndex, Fallen.PositionInTeam, CombatantIndex)
							   : INDEX_NONE;
		if (Heir == INDEX_NONE)
		{
			RoundOrder.RemoveAt(SeatIndex);
		}
		else
		{
			Seat.CombatantIndex = Heir;
		}
	}
}

FString FTurnRotation::ToString(const TArray<FCombatantTurnDebt> &State) const
{
	FString Out = FString::Printf(TEXT("Rotation: round %d, lead team %d%s, rest of round:"),
								  RoundsBuilt, LeadTeam, bOpeningRoundNext ? TEXT(" (opening round next)") : TEXT(""));
	if (RoundOrder.Num() == 0)
	{
		return Out + TEXT(" (none; the next turn starts a round)");
	}
	for (const FTurnRotationSeat &Seat : RoundOrder)
	{
		const AActor *Actor = State.IsValidIndex(Seat.CombatantIndex) ? State[Seat.CombatantIndex].Actor : nullptr;
		Out += FString::Printf(TEXT(" %s%s,"), *GetNameSafe(Actor), Seat.bEmptySeat ? TEXT("(seat)") : TEXT(""));
	}
	Out.RemoveFromEnd(TEXT(","));
	return Out;
}
