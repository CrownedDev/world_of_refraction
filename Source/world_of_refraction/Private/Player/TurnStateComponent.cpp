// TurnStateComponent.cpp
// B-228. See TurnStateComponent.h.

#include "Player/TurnStateComponent.h"
#include "UI/Combat/CombatHUD.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "Player/RunStateComponent.h"
#include "Combat/WoRFightSubsystem.h"
#include "Combat/CombatControl.h"
#include "Loadout/LoadoutComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
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
	if (ClientHUD) // B-228b
	{
		ClientHUD->Teardown();
		bClientHUDBuilt = false;
	}
	CloseClientCommandMenu(); // B-232b
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
	UpdateClientCombatHUD();
	UpdateClientCommandMenu();
	OnTurnStateChanged.Broadcast();
}

void UTurnStateComponent::CloseClientCommandMenu()
{
	if (UCombatCommandMenuSubsystem *Menu = ClientMenuTurn != INDEX_NONE ? WoRFight::Get<UCombatCommandMenuSubsystem>(this) : nullptr; Menu && Menu->IsOpen())
	{
		Menu->Close();
	}
	ClientMenuTurn = INDEX_NONE;
}

void UTurnStateComponent::ReopenClientCommandMenu()
{
	ClientMenuTurn = INDEX_NONE;
	UpdateClientCommandMenu();
}

void UTurnStateComponent::UpdateClientCommandMenu()
{
	UWorld *World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client || !bClientHUDBuilt)
	{
		return;
	}
	UCombatCommandMenuSubsystem *Menu = WoRFight::Get<UCombatCommandMenuSubsystem>(this);
	if (!Menu)
	{
		return;
	}
	if (!bCombatActive || TurnNumber != ClientMenuTurn)
	{
		if (Menu->IsOpen() && ClientMenuTurn != INDEX_NONE)
		{
			Menu->Close(); // the turn moved on (or the fight ended)
		}
	}
	if (!bCombatActive || !CurrentActor || TurnNumber == ClientMenuTurn)
	{
		return;
	}
	const URunStateComponent *Run = URunStateComponent::Find(this);
	// Review fix: only a roster member follows the seat rule; off the roster (a test actor) the client never opens
	// one (the server runs it). An AI seat has no commander (GetCommander: AI_COMMANDER); a member handed to the AI
	// (D-026) is not player-driven.
	const bool bOnRoster = Run && Run->FindRosterEntry(CurrentActor);
	const bool bCommandedHere = bOnRoster && Run->IsCommandedBy(CurrentActor, World->GetFirstPlayerController());
	const APawn *Pawn = Cast<APawn>(CurrentActor);
	const bool bAIControlled = Pawn && !CombatControl::IsPlayerDriven(Pawn);
	if (!bOnRoster || !CommandMenuRules::OpensHere(bAIControlled, /*bHasRunState=*/true, bCommandedHere))
	{
		return;
	}
	ClientMenuTurn = TurnNumber;
	// B-232c (review): a new turn gives the free switch back on the client's copy too (the server's orchestrator
	// does it there), so the menu offers it and the client can follow the server's switch.
	if (ULoadoutComponent *Loadout = CurrentActor->FindComponentByClass<ULoadoutComponent>())
	{
		Loadout->ResetTurnSwitch();
	}
	Menu->OpenForActor(CurrentActor);
	UE_LOG(LogWoRCombat, Log, TEXT("[TurnState] Client: command menu opened for %s (turn %d)"), *GetNameSafe(CurrentActor), TurnNumber);
}

void UTurnStateComponent::UpdateClientCombatHUD()
{
	UWorld *World = GetWorld();
	const bool bIsClient = World && World->GetNetMode() == NM_Client;
	const EClientHUDStep Step = ClientCombatHUD::Next(bIsClient, bCombatActive, bClientHUDBuilt);
	if (Step == EClientHUDStep::Teardown)
	{
		if (ClientHUD)
		{
			ClientHUD->Teardown();
		}
		bClientHUDBuilt = false;
		CloseClientCommandMenu(); // B-232b (review): nothing carries into the next fight
		return;
	}
	if (Step != EClientHUDStep::Build)
	{
		return;
	}
	// The orchestrator is server-only (not replicated), so the client owns its HUD here. Built once both sides have
	// arrived, every member resolved: a later OnRep retries until then (review fix).
	const TArray<AActor *> Party = GetTeamMembers(0);
	const TArray<AActor *> Enemies = GetTeamMembers(1);
	const auto AllValid = [](const TArray<AActor *> &Side) { return !Side.IsEmpty() && !Side.Contains(nullptr); };
	APlayerController *PC = World->GetFirstPlayerController();
	if (!AllValid(Party) || !AllValid(Enemies) || !PC || !PC->GetLocalPlayer())
	{
		UE_LOG(LogWoRCombat, Verbose, TEXT("[TurnState] Client: combat on, waiting for both sides before building the HUD"));
		return;
	}
	if (!ClientHUD)
	{
		ClientHUD = NewObject<UCombatHUD>(this);
	}
	ClientHUD->Build(PC, Party, Enemies);
	bClientHUDBuilt = true;
	UE_LOG(LogWoRCombat, Log, TEXT("[TurnState] Client: combat HUD built from the replicated turn state (%d / %d)"), Party.Num(), Enemies.Num());
}

FString UTurnStateComponent::GetDebugString() const
{
	FString Out = FString::Printf(TEXT("combat %s, turn %d, current %s%s, %d upcoming, %d combatants%s"),
								  bCombatActive ? TEXT("on") : TEXT("off"), TurnNumber, *GetNameSafe(CurrentActor),
								  bCurrentTurnIsBonus ? TEXT(" (bonus)") : TEXT(""), Upcoming.Num(), Teams.Num(),
								  bClientHUDBuilt ? TEXT(", client HUD built") : TEXT(""));
	for (int32 Index = 0; Index < Upcoming.Num(); ++Index)
	{
		Out += FString::Printf(TEXT("\n  +%d %s%s (team %d)"), Index + 1, *GetNameSafe(Upcoming[Index].Actor),
							   Upcoming[Index].bIsBonusTurn ? TEXT(" bonus") : TEXT(""), GetActorTeam(Upcoming[Index].Actor));
	}
	return Out;
}
