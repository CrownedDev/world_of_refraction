// WoRGameState.cpp

#include "Player/WoRGameState.h"
#include "Player/TurnStateComponent.h"
#include "Player/RunStateComponent.h"
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
