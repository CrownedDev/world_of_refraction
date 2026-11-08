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
	/** Tick period while a vote is open, or a passed vote waits for a fight to end. */
	constexpr float REVIVE_VOTE_TICK_SECONDS = 1.0f;

	bool IsSameVote(const FReviveVote &A, const FReviveVote &B)
	{
		return A.bOpen == B.bOpen && A.LastResult == B.LastResult && A.VoterIds == B.VoterIds && A.YesIds == B.YesIds;
	}
}

bool AWoRGameState::IsPlayerInBattle(const APlayerState *Player) const
{
	const UTurnManager *Turns = WoRFight::Get<UTurnManager>(this);
	const AController *Controller = Player ? Cast<AController>(Player->GetOwner()) : nullptr;
	if (!Turns || !Turns->IsCombatActive() || !Controller)
	{
		return false;
	}
	// TODO: ControlsActor asks whether the player may command a fighter, not whether it is in the fight: under the
	// seat rotation (B-232) an unclaimed member can count. Right while players share one fight; revisit with per-player fights.
	for (const int32 Team : {TurnManagerTeams::LOCAL_TEAM_INDEX, TurnManagerTeams::OPPOSING_TEAM_INDEX})
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

bool AWoRGameState::IsAnyFightActive() const
{
	const UTurnManager *Turns = WoRFight::Get<UTurnManager>(this);
	return Turns && Turns->IsCombatActive();
}

EReviveVoteResult AWoRGameState::StartReviveVote(AWoRPlayerState *Starter)
{
	if (!HasAuthority() || !Starter)
	{
		return EReviveVoteResult::Failed;
	}
	if (bReviveWaitsForFight || !WoRRestSpot::CanReviveHere(GetWorld()))
	{
		// Review: the client's menu has closed, so it hears why (a passed revive is waiting, or nothing to revive).
		Starter->Client_ShowFeatureMessage(bReviveWaitsForFight
			? NSLOCTEXT("WoRGameState", "ReviveVoteAlreadyWaits", "The mini-boss already returns when the fight ends.")
			: NSLOCTEXT("WoRGameState", "ReviveVoteNothing", "There is no mini-boss to revive here."));
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
		const TObjectPtr<APlayerState> *Found = PlayerArray.FindByPredicate([Id](const APlayerState *P) { return P && P->GetPlayerId() == Id; });
		return Found && IsPlayerInBattle(*Found);
	});
	const EReviveVoteResult Result = ReviveVote::Start(ReviveVoteState, Starter->GetPlayerId(), Voters, GetServerWorldTimeSeconds());
	UE_LOG(LogWoR, Log, TEXT("[ReviveVote] %s starts a vote: %d voter(s) -> %s"), *Starter->GetPlayerName(), Voters.Num(),
		   ReviveVote::ResultName(Result));
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
	OnReviveVoteChanged.Broadcast(); // the host's own menu (clients hear it in OnRep_ReviveVote)
	if (Result != EReviveVoteResult::Pending)
	{
		CloseReviveVote(Result); // solo: the starter is the only voter
	}
	return Result;
}

void AWoRGameState::CastReviveVote(const AWoRPlayerState *Voter, bool bYes)
{
	// Review: a late or repeated answer (a double click, an RPC in flight, the console) never acts on a closed vote again.
	if (!HasAuthority() || !Voter || !ReviveVoteState.bOpen)
	{
		return;
	}
	const FReviveVote Before = ReviveVoteState;
	const EReviveVoteResult Result = ReviveVote::Answer(ReviveVoteState, Voter->GetPlayerId(), bYes);
	FinishReviveVoteStep(Before, Result);
}

void AWoRGameState::TickReviveVote()
{
	if (bReviveWaitsForFight)
	{
		if (!IsAnyFightActive())
		{
			bReviveWaitsForFight = false;
			GetWorldTimerManager().ClearTimer(ReviveVoteTimer);
			ReviveAfterVote();
		}
		return;
	}
	if (!ReviveVoteState.bOpen)
	{
		GetWorldTimerManager().ClearTimer(ReviveVoteTimer);
		return;
	}
	const FReviveVote Before = ReviveVoteState;
	EReviveVoteResult Result = EReviveVoteResult::Pending;
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
	FinishReviveVoteStep(Before, Result);
}

void AWoRGameState::FinishReviveVoteStep(const FReviveVote &Before, EReviveVoteResult Result)
{
	if (!IsSameVote(Before, ReviveVoteState))
	{
		OnReviveVoteChanged.Broadcast(); // review: only on a change, not every tick
	}
	if (Before.bOpen && !ReviveVoteState.bOpen)
	{
		CloseReviveVote(Result); // this step closed it: act once
	}
}

void AWoRGameState::CloseReviveVote(EReviveVoteResult Result)
{
	GetWorldTimerManager().ClearTimer(ReviveVoteTimer);
	UE_LOG(LogWoR, Log, TEXT("[ReviveVote] closed: %s"), ReviveVote::ResultName(Result));
	if (Result != EReviveVoteResult::Passed)
	{
		TellEveryone(NSLOCTEXT("WoRGameState", "ReviveVoteFailed", "The mini-boss is not revived (not everyone agreed)."));
		return;
	}
	if (IsAnyFightActive())
	{
		// Q-187e2 (review): a revive reloads the level, so it waits until no fight is running.
		// TODO: not replicated, so a host hand-over (B-234b) loses a waiting revive; PIE check 2e also watches that the
		// reload doesn't cut off the victory / level-drop screens (it fires within a tick of the fight ending).
		bReviveWaitsForFight = true;
		GetWorldTimerManager().SetTimer(ReviveVoteTimer, this, &AWoRGameState::TickReviveVote, REVIVE_VOTE_TICK_SECONDS, true);
		TellEveryone(NSLOCTEXT("WoRGameState", "ReviveVoteWaits", "Everyone agreed: the mini-boss returns when the fight ends."));
		return;
	}
	ReviveAfterVote();
}

void AWoRGameState::ReviveAfterVote()
{
	// Review: checked again when it happens (revivals may be used up by now); everyone hears only what really happened.
	if (WoRRestSpot::CanReviveHere(GetWorld()) && WoRRestSpot::ReviveMiniBoss(GetWorld())) // the level reloads after this
	{
		TellEveryone(NSLOCTEXT("WoRGameState", "ReviveVotePassed", "Everyone agreed: the mini-boss returns."));
		return;
	}
	UE_LOG(LogWoR, Warning, TEXT("[ReviveVote] passed, but the revive was refused"));
	TellEveryone(NSLOCTEXT("WoRGameState", "ReviveVoteRefusedLate", "Everyone agreed, but the mini-boss can't be revived now."));
}

void AWoRGameState::TellEveryone(const FText &Message)
{
	// TODO: a client may miss this when the level reload follows at once; check in PIE.
	for (APlayerState *Player : PlayerArray)
	{
		if (AWoRPlayerState *WoRPlayer = Cast<AWoRPlayerState>(Player))
		{
			WoRPlayer->Client_ShowFeatureMessage(Message);
		}
	}
}

void AWoRGameState::OnRep_ReviveVote()
{
	OnReviveVoteChanged.Broadcast();
}

FString AWoRGameState::GetReviveVoteString() const
{
	return FString::Printf(TEXT("[ReviveVote] %s%s"), *ReviveVote::GetVoteString(ReviveVoteState, GetServerWorldTimeSeconds()),
						   bReviveWaitsForFight ? TEXT(", waiting for the fight to end") : TEXT(""));
}

static FAutoConsoleCommandWithWorld GTrialReviveVoteCommand(
	TEXT("wor.Trial.ReviveVote"),
	TEXT("B-187e: the mini-boss revive vote (open, yes count, time left, or the last result)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const AWoRGameState *GameState = World ? World->GetGameState<AWoRGameState>() : nullptr;
		UE_LOG(LogWoR, Display, TEXT("%s"), GameState ? *GameState->GetReviveVoteString() : TEXT("[ReviveVote] no game state"));
	}));

