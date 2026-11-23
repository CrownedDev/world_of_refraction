// ReviveVote.cpp

#include "Trial/ReviveVote.h"

namespace
{
	EReviveVoteResult Close(FReviveVote &Vote, EReviveVoteResult Result)
	{
		Vote.bOpen = false;
		Vote.LastResult = Result;
		return Result;
	}

	EReviveVoteResult PassIfAllYes(FReviveVote &Vote)
	{
		if (Vote.VoterIds.Num() == 0)
		{
			return Close(Vote, EReviveVoteResult::Failed);
		}
		for (const int32 Id : Vote.VoterIds)
		{
			if (!Vote.YesIds.Contains(Id))
			{
				return EReviveVoteResult::Pending;
			}
		}
		return Close(Vote, EReviveVoteResult::Passed);
	}
}

TArray<int32> ReviveVote::SelectVoters(const TArray<int32> &PlayerIds, TFunctionRef<bool(int32)> IsInBattle)
{
	TArray<int32> Voters;
	for (const int32 Id : PlayerIds)
	{
		if (!IsInBattle(Id))
		{
			Voters.AddUnique(Id);
		}
	}
	return Voters;
}

EReviveVoteResult ReviveVote::Start(FReviveVote &Vote, int32 StarterId, const TArray<int32> &Voters, double Now)
{
	if (Vote.bOpen || !Voters.Contains(StarterId))
	{
		return EReviveVoteResult::Failed;
	}
	Vote.bOpen = true;
	Vote.VoterIds = Voters;
	Vote.YesIds = {StarterId};
	Vote.Deadline = Now + TIMEOUT_SECONDS;
	Vote.LastResult = EReviveVoteResult::Pending;
	return PassIfAllYes(Vote);
}

EReviveVoteResult ReviveVote::Answer(FReviveVote &Vote, int32 PlayerId, bool bYes)
{
	if (!Vote.bOpen || !IsWaitingFor(Vote, PlayerId))
	{
		return Vote.bOpen ? EReviveVoteResult::Pending : Vote.LastResult;
	}
	if (!bYes)
	{
		return Close(Vote, EReviveVoteResult::Failed);
	}
	Vote.YesIds.AddUnique(PlayerId);
	return PassIfAllYes(Vote);
}

EReviveVoteResult ReviveVote::DropVoter(FReviveVote &Vote, int32 PlayerId)
{
	if (!Vote.bOpen || !Vote.VoterIds.Contains(PlayerId))
	{
		return Vote.bOpen ? EReviveVoteResult::Pending : Vote.LastResult;
	}
	Vote.VoterIds.Remove(PlayerId);
	Vote.YesIds.Remove(PlayerId);
	return PassIfAllYes(Vote);
}

EReviveVoteResult ReviveVote::CheckTimeout(FReviveVote &Vote, double Now)
{
	if (Vote.bOpen && Now >= Vote.Deadline)
	{
		return Close(Vote, EReviveVoteResult::Failed);
	}
	return Vote.bOpen ? EReviveVoteResult::Pending : Vote.LastResult;
}

bool ReviveVote::IsWaitingFor(const FReviveVote &Vote, int32 PlayerId)
{
	return Vote.bOpen && Vote.VoterIds.Contains(PlayerId) && !Vote.YesIds.Contains(PlayerId);
}

FString ReviveVote::GetVoteString(const FReviveVote &Vote, double Now)
{
	if (Vote.bOpen)
	{
		return FString::Printf(TEXT("open: %d of %d yes, %.0f s left"), Vote.YesIds.Num(), Vote.VoterIds.Num(), FMath::Max(0.0, Vote.Deadline - Now));
	}
	return FString::Printf(TEXT("closed: %s"), ResultName(Vote.LastResult));
}

const TCHAR *ReviveVote::ResultName(EReviveVoteResult Result)
{
	switch (Result)
	{
	case EReviveVoteResult::Pending: return TEXT("pending");
	case EReviveVoteResult::Passed: return TEXT("passed");
	case EReviveVoteResult::Failed: return TEXT("failed");
	default: return TEXT("none");
	}
}

FText ReviveVote::MakeLine(ELine Line, const FText &OpponentName, const FText &Starter)
{
	// The keys stay those the lines had in WoRGameState.cpp / WoRPlayerState.cpp.
	switch (Line)
	{
	case ELine::AlreadyWaits:
		return FText::Format(NSLOCTEXT("WoRGameState", "ReviveVoteAlreadyWaits", "The {0} already returns when the fight ends."), OpponentName);
	case ELine::Ask:
		return FText::Format(NSLOCTEXT("WoRGameState", "ReviveVoteAsk", "{0} wants to revive the {1}: vote in the rest menu, or type wor.Trial.Vote yes / no."),
							 Starter, OpponentName);
	case ELine::Failed:
		return FText::Format(NSLOCTEXT("WoRGameState", "ReviveVoteFailed", "The {0} is not revived (not everyone agreed)."), OpponentName);
	case ELine::Waits:
		return FText::Format(NSLOCTEXT("WoRGameState", "ReviveVoteWaits", "Everyone agreed: the {0} returns when the fight ends."), OpponentName);
	case ELine::Passed:
		return FText::Format(NSLOCTEXT("WoRGameState", "ReviveVotePassed", "Everyone agreed: the {0} returns."), OpponentName);
	case ELine::RefusedLate:
		return FText::Format(NSLOCTEXT("WoRGameState", "ReviveVoteRefusedLate", "Everyone agreed, but the {0} can't be revived now."), OpponentName);
	case ELine::Refused:
		return FText::Format(NSLOCTEXT("WoRPlayerState", "ReviveRefused", "The {0} can't be revived from here right now."), OpponentName);
	}
	return FText::GetEmpty();
}
