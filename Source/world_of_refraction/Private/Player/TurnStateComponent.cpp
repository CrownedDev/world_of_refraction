// TurnStateComponent.cpp
// B-228. See TurnStateComponent.h.

#include "Player/TurnStateComponent.h"
#include "Player/WoRGameState.h"
#include "Combat/TurnManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "WoRLog.h"

namespace TurnStateInternal
{
	/** Upcoming slots copied for the HUD; the turn-order strip shows 4 (UTurnOrderStripWidget::PreviewCount). */
	constexpr int32 TURN_STATE_PREVIEW_COUNT = 8;
	/** UTurnManager's teams: the local party (0) and the opposing party (1). */
	constexpr int32 TURN_STATE_TEAM_COUNT = 2;

	FAutoConsoleCommandWithWorld GPrintTurnStateCmd(
		TEXT("wor.Turn.PrintState"),
		TEXT("DEBUG (B-228): the GameState's replicated turn state (what every player's HUD reads)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			const UTurnStateComponent *TurnState = UTurnStateComponent::Find(World);
			UE_LOG(LogWoR, Log, TEXT("[TurnState] %s"), TurnState ? *TurnState->GetDebugString() : TEXT("no turn state (no AWoRGameState)"));
		}));
}

UTurnStateComponent::UTurnStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UTurnStateComponent *UTurnStateComponent::Find(const UObject *WorldContext)
{
	const UWorld *World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AWoRGameState *GameState = World ? World->GetGameState<AWoRGameState>() : nullptr;
	return GameState ? GameState->GetTurnState() : nullptr;
}

void UTurnStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTurnStateComponent, bCombatActive);
	DOREPLIFETIME(UTurnStateComponent, CurrentActor);
	DOREPLIFETIME(UTurnStateComponent, TurnNumber);
	DOREPLIFETIME(UTurnStateComponent, bCurrentTurnIsBonus);
	DOREPLIFETIME(UTurnStateComponent, Upcoming);
	DOREPLIFETIME(UTurnStateComponent, Teams);
}

void UTurnStateComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		BindToTurnManager();
	}
}

void UTurnStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromTurnManager();
	Super::EndPlay(EndPlayReason);
}

UTurnManager *UTurnStateComponent::GetTurnManager() const
{
	const UWorld *World = GetWorld();
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	return WoRFight::Get<UTurnManager>(GI);
}

void UTurnStateComponent::BindToTurnManager()
{
	UTurnManager *TurnManager = GetTurnManager();
	if (!TurnManager || BoundTurnManager.Get() == TurnManager)
	{
		return;
	}
	UnbindFromTurnManager();
	BoundTurnManager = TurnManager;
	TurnManager->OnTurnStarted.AddDynamic(this, &UTurnStateComponent::HandleTurnStarted);
	TurnManager->OnSpeedChanged.AddDynamic(this, &UTurnStateComponent::HandleSpeedChanged);
	TurnManager->OnCombatEnded.AddDynamic(this, &UTurnStateComponent::HandleCombatEnded);
	RefreshFromTurnManager();
}

void UTurnStateComponent::UnbindFromTurnManager()
{
	if (UTurnManager *TurnManager = BoundTurnManager.Get())
	{
		TurnManager->OnTurnStarted.RemoveDynamic(this, &UTurnStateComponent::HandleTurnStarted);
		TurnManager->OnSpeedChanged.RemoveDynamic(this, &UTurnStateComponent::HandleSpeedChanged);
		TurnManager->OnCombatEnded.RemoveDynamic(this, &UTurnStateComponent::HandleCombatEnded);
	}
	BoundTurnManager.Reset();
}

void UTurnStateComponent::RefreshFromTurnManager()
{
	const UTurnManager *TurnManager = BoundTurnManager.IsValid() ? BoundTurnManager.Get() : GetTurnManager();
	if (!TurnManager)
	{
		return;
	}
	bCombatActive = TurnManager->IsCombatActive();
	TurnNumber = TurnManager->GetTurnCount(); // the live count, so a nested turn advance can't be overwritten
	CurrentActor = bCombatActive ? TurnManager->GetCurrentActor() : nullptr;
	bCurrentTurnIsBonus = bCombatActive && TurnManager->GetCurrentTurnIsBonus();
	Upcoming.Reset();
	Teams.Reset();
	if (bCombatActive)
	{
		for (const FPreviewTurnEntry &Entry : TurnManager->PreviewTurnOrder(TurnStateInternal::TURN_STATE_PREVIEW_COUNT))
		{
			FTurnStateSlot &Slot = Upcoming.AddDefaulted_GetRef();
			Slot.Actor = Entry.Actor;
			Slot.bIsBonusTurn = Entry.bIsBonusTurn;
		}
		for (int32 TeamIndex = 0; TeamIndex < TurnStateInternal::TURN_STATE_TEAM_COUNT; ++TeamIndex)
		{
			for (AActor *Member : TurnManager->GetTeamMembers(TeamIndex))
			{
				FTurnStateTeam &Entry = Teams.AddDefaulted_GetRef();
				Entry.Actor = Member;
				Entry.Team = TeamIndex;
			}
		}
	}
	OnTurnStateChanged.Broadcast(); // the server's own HUD (OnRep doesn't fire on the authority)
}

int32 UTurnStateComponent::GetActorTeam(const AActor *Actor) const
{
	const FTurnStateTeam *Entry = Teams.FindByPredicate([Actor](const FTurnStateTeam &Each) { return Each.Actor == Actor; });
	return Entry ? Entry->Team : INDEX_NONE;
}

TArray<AActor *> UTurnStateComponent::GetTeamMembers(int32 Team) const
{
	TArray<AActor *> Members;
	for (const FTurnStateTeam &Entry : Teams)
	{
		if (Entry.Team == Team && Entry.Actor)
		{
			Members.Add(Entry.Actor);
		}
	}
	return Members;
}

void UTurnStateComponent::HandleTurnStarted(AActor *Actor, int32 InTurnNumber)
{
	RefreshFromTurnManager();
}

void UTurnStateComponent::HandleSpeedChanged(AActor *Actor)
{
	RefreshFromTurnManager();
}

void UTurnStateComponent::HandleCombatEnded(int32 FinalTurnCount)
{
	// UTurnManager broadcasts this before it clears its own state, so clear here instead of copying.
	TurnNumber = FinalTurnCount;
	bCombatActive = false;
	CurrentActor = nullptr;
	bCurrentTurnIsBonus = false;
	Upcoming.Reset();
	Teams.Reset();
	OnTurnStateChanged.Broadcast();
}

void UTurnStateComponent::OnRep_TurnState()
{
	OnTurnStateChanged.Broadcast();
}

FString UTurnStateComponent::GetDebugString() const
{
	FString Out = FString::Printf(TEXT("combat %s, turn %d, current %s%s, %d upcoming, %d combatants"),
								  bCombatActive ? TEXT("on") : TEXT("off"), TurnNumber, *GetNameSafe(CurrentActor),
								  bCurrentTurnIsBonus ? TEXT(" (bonus)") : TEXT(""), Upcoming.Num(), Teams.Num());
	for (int32 Index = 0; Index < Upcoming.Num(); ++Index)
	{
		Out += FString::Printf(TEXT("\n  +%d %s%s (team %d)"), Index + 1, *GetNameSafe(Upcoming[Index].Actor),
							   Upcoming[Index].bIsBonusTurn ? TEXT(" bonus") : TEXT(""), GetActorTeam(Upcoming[Index].Actor));
	}
	return Out;
}
