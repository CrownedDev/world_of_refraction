// WoRGameState.cpp

#include "Player/WoRGameState.h"
#include "Player/TurnStateComponent.h"
#include "Player/RunStateComponent.h"
#include "Player/WoRPlayerState.h"
#include "Combat/CombatPlayerController.h"
#include "Combat/TurnManager.h"
#include "Combat/WoRFightSubsystem.h"
#include "Trial/TrialRestSpot.h"
#include "TimerManager.h"
#include "Online/HostSuccession.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	FAutoConsoleCommandWithWorld GPrintGameStateCmd(
		TEXT("wor.GameState.Print"),
		TEXT("DEBUG (B-227): the level's GameState class, net mode and players."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			const AWoRGameState *GameState = World ? World->GetGameState<AWoRGameState>() : nullptr;
			UE_LOG(LogWoR, Log, TEXT("[GameState] %s"),
				   GameState ? *GameState->GetDebugString()
							 : *FString::Printf(TEXT("no AWoRGameState (GameState %s)"), World ? *GetNameSafe(World->GetGameState()) : TEXT("-")));
		}));
}

AWoRGameState::AWoRGameState()
{
	TurnState = CreateDefaultSubobject<UTurnStateComponent>(TEXT("TurnState"));
	RunState = CreateDefaultSubobject<URunStateComponent>(TEXT("RunState"));
}

void AWoRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWoRGameState, HostOrder);
	DOREPLIFETIME(AWoRGameState, ReviveVoteState); // B-187e
}

void AWoRGameState::AddPlayerState(APlayerState *PlayerState)
{
	Super::AddPlayerState(PlayerState);
	UpdateHostOrder(PlayerState, /*bJoined=*/true);
}

void AWoRGameState::RemovePlayerState(APlayerState *PlayerState)
{
	UpdateHostOrder(PlayerState, /*bJoined=*/false);
	Super::RemovePlayerState(PlayerState);
}

void AWoRGameState::UpdateHostOrder(const APlayerState *PlayerState, bool bJoined)
{
	// Server only: clients take the order the host sends. An AI's or a not-yet-logged-in player's id is empty.
	if (!HasAuthority() || !PlayerState || PlayerState->IsABot() || !PlayerState->GetUniqueId().IsValid())
	{
		return;
	}
	// TODO: B-234c (review): a new level's GameState rebuilds the order as the players arrive; after a seamless travel
	// that is usually the old order, but two joiners could swap. Carry the previous level's order over if PIE shows it.
	const FString Id = PlayerState->GetUniqueId().ToString();
	if (bJoined)
	{
		HostOrder.AddUnique(Id);
	}
	else
	{
		HostOrder.Remove(Id);
	}
	if (GetNetMode() != NM_Standalone) // desktop review: a solo world (the default map mid hand-over) keeps the known order
	{
		OnRep_HostOrder(); // the host keeps its own copy too
	}
}

void AWoRGameState::OnRep_HostOrder()
{
	const UWorld *World = GetWorld();
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	if (UWoRHostHandOverSubsystem *HandOver = GI ? GI->GetSubsystem<UWoRHostHandOverSubsystem>() : nullptr)
	{
		HandOver->NoteHostOrder(HostOrder);
	}
}

FString AWoRGameState::GetDebugString() const
{
	const UWorld *World = GetWorld();
	const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
	FString Out = FString::Printf(TEXT("%s on %s, net mode %d, %d player(s)"), *GetName(), World ? *World->GetMapName() : TEXT("-"),
								  static_cast<int32>(NetMode), PlayerArray.Num());
	for (const APlayerState *Player : PlayerArray)
	{
		Out += FString::Printf(TEXT("\n  %s (%s)"), Player ? *Player->GetPlayerName() : TEXT("null"), *GetNameSafe(Player));
	}
	Out += FString::Printf(TEXT("\nJoin order (B-234b): %s"), *FString::Join(HostOrder, TEXT(", ")));
	if (TurnState)
	{
		Out += FString::Printf(TEXT("\nTurn state: %s"), *TurnState->GetDebugString());
	}
	if (RunState)
	{
		Out += FString::Printf(TEXT("\nRun state: %s"), *RunState->GetDebugString()); // B-232
	}
	return Out;
}

// ==================== B-187e: the revive vote (Q-187e changed by Crown, 2026-11-08) ====================

namespace
{
	/** Tick period while a vote is open. */
	constexpr float REVIVE_VOTE_TICK_SECONDS = 1.0f;
}

bool AWoRGameState::IsPlayerInBattle(const APlayerState *Player) const
{
	const UTurnManager *Turns = WoRFight::Get<UTurnManager>(this);
	const AController *Controller = Player ? Cast<AController>(Player->GetOwner()) : nullptr;
	if (!Turns || !Turns->IsCombatActive() || !Controller)
	{
		return false;
	}
	constexpr int32 TEAM_COUNT = 2; // the players' side and the enemies'; ControlsActor never says yes to an enemy
	for (int32 Team = 0; Team < TEAM_COUNT; ++Team)
	{
		for (const AActor *Fighter : Turns->GetTeamMembers(Team))
		{
			if (ACombatPlayerController::ControlsActor(Controller, Fighter, /*bFromClient=*/true))
			{
				return true;
			}
		}
	}
	return false;
}

EReviveVoteResult AWoRGameState::StartReviveVote(AWoRPlayerState *Starter)
{
	if (!HasAuthority() || !Starter || !WoRRestSpot::CanReviveHere(GetWorld()))
	{
		return EReviveVoteResult::Failed;
	}
	TArray<int32> PlayerIds;
	for (const APlayerState *Player : PlayerArray)
	{
		if (Player && !Player->IsABot())
		{
			PlayerIds.Add(Player->GetPlayerId());
		}
	}
	const TArray<int32> Voters = ReviveVote::SelectVoters(PlayerIds, [this](int32 Id)
	{
		const APlayerState *const *Found = PlayerArray.FindByPredicate([Id](const APlayerState *P) { return P && P->GetPlayerId() == Id; });
		return Found && IsPlayerInBattle(*Found);
	});
	const EReviveVoteResult Result = ReviveVote::Start(ReviveVoteState, Starter->GetPlayerId(), Voters, GetServerWorldTimeSeconds());
	UE_LOG(LogWoR, Log, TEXT("[ReviveVote] %s starts a vote: %d voter(s) -> %s"), *Starter->GetPlayerName(), Voters.Num(),
		   *UEnum::GetValueAsString(Result));
	if (Result == EReviveVoteResult::Failed)
	{
		// Refused before it opened (the starter is in battle, or a vote is running): only the starter hears it.
		Starter->Client_ShowFeatureMessage(NSLOCTEXT("WoRGameState", "ReviveVoteRefused", "A revive vote can't start right now."));
		return Result;
	}
	if (Result == EReviveVoteResult::Pending)
	{
		GetWorldTimerManager().SetTimer(ReviveVoteTimer, this, &AWoRGameState::TickReviveVote, REVIVE_VOTE_TICK_SECONDS, true);
		// Placeholder until the real screen (D-024): every other voter is told how to answer.
		const FText Ask = FText::Format(NSLOCTEXT("WoRGameState", "ReviveVoteAsk",
												  "{0} wants to revive the mini-boss: vote in the rest menu, or type wor.Trial.Vote yes / no."),
										FText::FromString(Starter->GetPlayerName()));
		for (APlayerState *Player : PlayerArray)
		{
			AWoRPlayerState *Voter = Cast<AWoRPlayerState>(Player);
			if (Voter && ReviveVote::IsWaitingFor(ReviveVoteState, Voter->GetPlayerId()))
			{
				Voter->Client_ShowFeatureMessage(Ask);
			}
		}
	}
	ApplyReviveVoteResult(Result);
	return Result;
}

void AWoRGameState::CastReviveVote(const AWoRPlayerState *Voter, bool bYes)
{
	if (!HasAuthority() || !Voter)
	{
		return;
	}
	ApplyReviveVoteResult(ReviveVote::Cast(ReviveVoteState, Voter->GetPlayerId(), bYes));
}

void AWoRGameState::TickReviveVote()
{
	EReviveVoteResult Result = ReviveVoteState.bOpen ? EReviveVoteResult::Pending : ReviveVoteState.LastResult;
	for (const APlayerState *Player : PlayerArray)
	{
		const int32 Id = Player ? Player->GetPlayerId() : INDEX_NONE;
		if (Result == EReviveVoteResult::Pending && ReviveVoteState.VoterIds.Contains(Id) && IsPlayerInBattle(Player))
		{
			Result = ReviveVote::DropVoter(ReviveVoteState, Id); // entered a fight during the vote
		}
	}
	// A voter who left the session leaves the list too.
	for (const int32 Id : TArray<int32>(ReviveVoteState.VoterIds))
	{
		if (Result == EReviveVoteResult::Pending && !PlayerArray.ContainsByPredicate([Id](const APlayerState *P) { return P && P->GetPlayerId() == Id; }))
		{
			Result = ReviveVote::DropVoter(ReviveVoteState, Id);
		}
	}
	if (Result == EReviveVoteResult::Pending)
	{
		Result = ReviveVote::CheckTimeout(ReviveVoteState, GetServerWorldTimeSeconds());
	}
	ApplyReviveVoteResult(Result);
}

void AWoRGameState::ApplyReviveVoteResult(EReviveVoteResult Result)
{
	if (Result == EReviveVoteResult::Passed || Result == EReviveVoteResult::Failed)
	{
		GetWorldTimerManager().ClearTimer(ReviveVoteTimer);
		const FText Message = Result == EReviveVoteResult::Passed
								  ? NSLOCTEXT("WoRGameState", "ReviveVotePassed", "Everyone agreed: the mini-boss returns.")
								  : NSLOCTEXT("WoRGameState", "ReviveVoteFailed", "The mini-boss is not revived (not everyone agreed).");
		for (APlayerState *Player : PlayerArray)
		{
			if (AWoRPlayerState *WoRPlayer = Cast<AWoRPlayerState>(Player))
			{
				WoRPlayer->Client_ShowFeatureMessage(Message);
			}
		}
		UE_LOG(LogWoR, Log, TEXT("[ReviveVote] %s"), *UEnum::GetValueAsString(Result));
	}
	OnReviveVoteChanged.Broadcast(); // the host's own menu (clients hear it in OnRep_ReviveVote)
	if (Result == EReviveVoteResult::Passed)
	{
		WoRRestSpot::ReviveMiniBoss(GetWorld()); // the host revives as before (the level reloads)
	}
}

void AWoRGameState::OnRep_ReviveVote()
{
	OnReviveVoteChanged.Broadcast();
}

FString AWoRGameState::GetReviveVoteString() const
{
	return FString::Printf(TEXT("[ReviveVote] %s"), *ReviveVote::GetVoteString(ReviveVoteState, GetServerWorldTimeSeconds()));
}

static FAutoConsoleCommandWithWorld GTrialReviveVoteCommand(
	TEXT("wor.Trial.ReviveVote"),
	TEXT("B-187e: the mini-boss revive vote (open, yes count, time left, or the last result)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const AWoRGameState *GameState = World ? World->GetGameState<AWoRGameState>() : nullptr;
		UE_LOG(LogWoR, Display, TEXT("%s"), GameState ? *GameState->GetReviveVoteString() : TEXT("[ReviveVote] no game state"));
	}));

