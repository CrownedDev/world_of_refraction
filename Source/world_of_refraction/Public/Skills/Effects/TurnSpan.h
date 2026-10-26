// TurnSpan.h
// The one turn-counting rule (D-030, D-051, D-055; deckbuilder 04-effects.md "Lasts"):
// - a span of N counts the HOLDER's turns; every turn the holder takes counts;
// - a span that lands during the holder's own turn counts that turn as turn 1;
// - it ends as the holder's next turn after those N starts, before that turn's payouts.
// Pure state + rules, no world access, so it is tested on its own (WoR.Effects.TurnSpan)
// and shared by full bars now and the GAS turn tracker later (B-042).
// Debug: GetDebugString.

#pragma once

#include "CoreMinimal.h"
#include "TurnSpan.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FTurnSpan
{
	GENERATED_BODY()

	/** Holder turns still to count before the span ends. */
	UPROPERTY(BlueprintReadOnly, Category = "Duration")
	int32 TurnsLeft = 0;

	static FTurnSpan OfHolderTurns(int32 Turns)
	{
		FTurnSpan Span;
		Span.TurnsLeft = FMath::Max(0, Turns);
		return Span;
	}

	/** At the holder's turn start, before payouts: true = the span is over, end it now. */
	bool EndsAtHolderTurnStart() const
	{
		return TurnsLeft <= 0;
	}

	/** At the holder's turn end: that turn counted. The landing turn counts the same way,
	 *  because a span landing during the holder's turn is still held when that turn ends. */
	void CountHolderTurn()
	{
		TurnsLeft = FMath::Max(0, TurnsLeft - 1);
	}

	FString GetDebugString() const
	{
		return FString::Printf(TEXT("%d holder turn(s) left"), TurnsLeft);
	}
};
