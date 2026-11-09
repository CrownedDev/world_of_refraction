// ReviveVote.h
// B-187e (Q-187e changed by Crown, 2026-11-08): reviving a mini-boss is a vote. Any player starts it from a rest menu
// (the start is that player's yes); every player outside battle at that moment votes; it passes only when all of them
// say yes, then the host revives as before. A player who enters a fight leaves the voter list; one no, or the timeout,
// fails it. Solo the starter is the only voter, so it passes at once. Rules only; AWoRGameState runs it.

#pragma once

#include "CoreMinimal.h"
#include "ReviveVote.generated.h"

UENUM(BlueprintType)
enum class EReviveVoteResult : uint8
{
	None = 0,
	Pending = 1,
	Passed = 2,
	Failed = 3
};

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FReviveVote
{
	GENERATED_BODY()

	/** A vote is running. */
	UPROPERTY(BlueprintReadOnly, Category = "Revive Vote")
	bool bOpen = false;

	/** APlayerState::GetPlayerId of every voter (outside battle when it started, still outside battle). */
	UPROPERTY(BlueprintReadOnly, Category = "Revive Vote")
	TArray<int32> VoterIds;

	/** The voters who said yes (the starter's start is one). */
	UPROPERTY(BlueprintReadOnly, Category = "Revive Vote")
	TArray<int32> YesIds;

	/** Server time (seconds) when it times out and fails. */
	UPROPERTY(BlueprintReadOnly, Category = "Revive Vote")
	double Deadline = 0.0;

	/** The last vote's outcome (for the menus once it closed). */
	UPROPERTY(BlueprintReadOnly, Category = "Revive Vote")
	EReviveVoteResult LastResult = EReviveVoteResult::None;
};

namespace ReviveVote
{
	/** How long players have to vote. TODO: tune in PIE. */
	constexpr double TIMEOUT_SECONDS = 30.0;

	/** The players who vote: every id whose player is not in battle (IsInBattle), in order. */
	WORLD_OF_REFRACTION_API TArray<int32> SelectVoters(const TArray<int32> &PlayerIds, TFunctionRef<bool(int32)> IsInBattle);

	/** Opens a vote with Voters (the starter among them, its yes counted). Returns Passed when the starter is the only voter
	 *  (solo), else Pending; Failed when the starter is not a voter (in battle) or a vote is already open. */
	WORLD_OF_REFRACTION_API EReviveVoteResult Start(FReviveVote &Vote, int32 StarterId, const TArray<int32> &Voters, double Now);

	/** A voter's answer: a no fails it; the last yes passes it. Non-voters, repeats and late answers change nothing (they
	 *  return Pending, or the closed vote's LastResult). Not named Cast: that would hide UE's Cast<T> in this namespace. */
	WORLD_OF_REFRACTION_API EReviveVoteResult Answer(FReviveVote &Vote, int32 PlayerId, bool bYes);

	/** A voter entered a fight (or left): off the list; the vote then passes if every remaining voter said yes, and fails
	 *  when nobody is left. */
	WORLD_OF_REFRACTION_API EReviveVoteResult DropVoter(FReviveVote &Vote, int32 PlayerId);

	/** Past the deadline: fails. */
	WORLD_OF_REFRACTION_API EReviveVoteResult CheckTimeout(FReviveVote &Vote, double Now);

	/** Whether PlayerId still has to answer. */
	WORLD_OF_REFRACTION_API bool IsWaitingFor(const FReviveVote &Vote, int32 PlayerId);

	/** "pending" / "passed" / "failed" / "none" (debug). */
	WORLD_OF_REFRACTION_API const TCHAR *ResultName(EReviveVoteResult Result);

	/** Debug: "open: 2 of 3 yes, 12 s left" / "closed: passed". */
	WORLD_OF_REFRACTION_API FString GetVoteString(const FReviveVote &Vote, double Now);
}
