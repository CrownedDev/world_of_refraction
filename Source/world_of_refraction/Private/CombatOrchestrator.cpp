// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatOrchestrator.h"
#include "TurnManager.h"
#include "CharacterDataComponent.h"
#include "Kismet/GameplayStatics.h"

ACombatOrchestrator::ACombatOrchestrator()
{
	PrimaryActorTick.bCanEverTick = false;

	CombatState = ECombatState::Idle;
	CurrentActor = nullptr;
	CurrentTurnNumber = 0;
	TurnManagerRef = nullptr;
}

void ACombatOrchestrator::BeginPlay()
{
	Super::BeginPlay();

	// Cache TurnManager reference
	UGameInstance* GI = GetGameInstance();
	if (GI)
	{
		TurnManagerRef = GI->GetSubsystem<UTurnManager>();
	}

	if (!TurnManagerRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Failed to get TurnManager subsystem!"));
	}
}

void ACombatOrchestrator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Cleanup
	UnbindTurnManagerEvents();
	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);

	Super::EndPlay(EndPlayReason);
}

// ========================================
// COMBAT CONTROL
// ========================================

void ACombatOrchestrator::StartCombat(const TArray<AActor*>& Team0, const TArray<AActor*>& Team1)
{
	if (CombatState != ECombatState::Idle)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] StartCombat called while combat already active. Forcing end."));
		ForceEndCombat();
	}

	if (!TurnManagerRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Cannot start combat - TurnManager not available!"));
		return;
	}

	// Validate teams
	if (Team0.Num() == 0 || Team1.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Cannot start combat - empty team(s)!"));
		return;
	}

	// Store team references
	Team0Combatants = Team0;
	Team1Combatants = Team1;
	CurrentTurnNumber = 0;
	CurrentActor = nullptr;

	SetCombatState(ECombatState::Initializing);

	// Bind to TurnManager events
	BindTurnManagerEvents();

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Starting combat: Team0 (%d) vs Team1 (%d)"),
		Team0.Num(), Team1.Num());

	// Initialize TurnManager (this will trigger first OnTurnStarted)
	TurnManagerRef->InitializeCombat(Team0, Team1);

	SetCombatState(ECombatState::InProgress);
}

void ACombatOrchestrator::ForceEndCombat(ECombatState ForcedState)
{
	if (CombatState == ECombatState::Idle)
		return;

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Force ending combat with state: %d"), (int32)ForcedState);

	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
	UnbindTurnManagerEvents();

	if (TurnManagerRef && TurnManagerRef->IsCombatActive())
	{
		TurnManagerRef->EndCombat();
	}

	SetCombatState(ForcedState);

	FCombatResult Result = BuildCombatResult();
	OnCombatResultReady.Broadcast(Result);

	// Reset state
	Team0Combatants.Empty();
	Team1Combatants.Empty();
	CurrentActor = nullptr;
	CurrentTurnNumber = 0;

	SetCombatState(ECombatState::Idle);
}

void ACombatOrchestrator::OnActionCompleted()
{
	if (CombatState != ECombatState::InProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] OnActionCompleted called but combat not in progress"));
		return;
	}

	// Clear auto-advance timer if it was running
	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);

	// Process end-of-turn effects
	ProcessEndOfTurnEffects(CurrentActor);

	// Check win condition
	ECombatState WinState = CheckWinCondition();
	if (WinState != ECombatState::InProgress)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Win condition met: %d"), (int32)WinState);

		UnbindTurnManagerEvents();
		TurnManagerRef->EndCombat();

		SetCombatState(WinState);

		FCombatResult Result = BuildCombatResult();
		OnCombatResultReady.Broadcast(Result);

		// Reset for next combat
		Team0Combatants.Empty();
		Team1Combatants.Empty();
		CurrentActor = nullptr;

		SetCombatState(ECombatState::Idle);
		return;
	}

	// Advance to next turn
	TurnManagerRef->AdvanceToNextTurn();
}

// ========================================
// TURN MANAGER EVENT HANDLERS
// ========================================

void ACombatOrchestrator::HandleTurnStarted(AActor* Actor, int32 TurnNumber)
{
	CurrentActor = Actor;
	CurrentTurnNumber = TurnNumber;

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Turn %d started: %s"),
		TurnNumber, *Actor->GetName());

	// Process start-of-turn status effects
	ProcessStartOfTurnEffects(Actor);

	// Check if actor died from DOT effects
	if (!IsActorAlive(Actor))
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] %s died from start-of-turn effects, skipping action"),
			*Actor->GetName());
		OnActionCompleted();
		return;
	}

	// Broadcast for UI/listeners
	OnActorTurnStarted.Broadcast(Actor, TurnNumber);

	// Request action from actor
	RequestActionFromActor(Actor);
}

void ACombatOrchestrator::HandleTurnEnded(AActor* Actor, int32 TurnNumber)
{
	// Note: This event from TurnManager fires when EndCurrentTurn() is called
	// We handle most end-of-turn logic in OnActionCompleted() instead
	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] TurnManager reports turn %d ended for %s"),
		TurnNumber, *Actor->GetName());
}

void ACombatOrchestrator::HandleCombatEnded(int32 FinalTurnCount)
{
	// TurnManager ended combat (could be from no valid combatants)
	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] TurnManager ended combat at turn %d"), FinalTurnCount);

	if (CombatState == ECombatState::InProgress)
	{
		// Determine winner if we haven't already
		ECombatState WinState = CheckWinCondition();
		if (WinState == ECombatState::InProgress)
		{
			WinState = ECombatState::Draw; // No one alive = draw
		}

		SetCombatState(WinState);

		FCombatResult Result = BuildCombatResult();
		OnCombatResultReady.Broadcast(Result);

		Team0Combatants.Empty();
		Team1Combatants.Empty();
		CurrentActor = nullptr;

		SetCombatState(ECombatState::Idle);
	}
}

// ========================================
// INTERNAL METHODS
// ========================================

void ACombatOrchestrator::SetCombatState(ECombatState NewState)
{
	if (CombatState != NewState)
	{
		ECombatState OldState = CombatState;
		CombatState = NewState;

		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] State: %d -> %d"), (int32)OldState, (int32)NewState);

		OnCombatStateChanged.Broadcast(NewState);
	}
}

void ACombatOrchestrator::BindTurnManagerEvents()
{
	if (TurnManagerRef)
	{
		TurnManagerRef->OnTurnStarted.AddDynamic(this, &ACombatOrchestrator::HandleTurnStarted);
		TurnManagerRef->OnTurnEnded.AddDynamic(this, &ACombatOrchestrator::HandleTurnEnded);
		TurnManagerRef->OnCombatEnded.AddDynamic(this, &ACombatOrchestrator::HandleCombatEnded);
	}
}

void ACombatOrchestrator::UnbindTurnManagerEvents()
{
	if (TurnManagerRef)
	{
		TurnManagerRef->OnTurnStarted.RemoveDynamic(this, &ACombatOrchestrator::HandleTurnStarted);
		TurnManagerRef->OnTurnEnded.RemoveDynamic(this, &ACombatOrchestrator::HandleTurnEnded);
		TurnManagerRef->OnCombatEnded.RemoveDynamic(this, &ACombatOrchestrator::HandleCombatEnded);
	}
}

void ACombatOrchestrator::ProcessStartOfTurnEffects(AActor* Actor)
{
	// STUB: Future integration with StatusEffectManager
	// StatusEffectManager->ProcessStartOfTurnEffects(Actor);

	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] STUB: ProcessStartOfTurnEffects for %s"),
		*Actor->GetName());

	// TODO: Process buffs (attack up, defense up, haste, etc.)
	// These apply their effects at the START of the owner's turn
}

void ACombatOrchestrator::ProcessEndOfTurnEffects(AActor* Actor)
{
	// STUB: Future integration with StatusEffectManager
	// StatusEffectManager->ProcessEndOfTurnEffects(Actor);

	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] STUB: ProcessEndOfTurnEffects for %s"),
		*Actor->GetName());

	// TODO: Process DOTs (poison, burn, bleed, etc.)
	// These deal damage at the END of the owner's turn
	// Also tick down status effect durations
}

void ACombatOrchestrator::RequestActionFromActor(AActor* Actor)
{
	// Broadcast that we're waiting for action
	OnActionRequested.Broadcast(Actor);

	// STUB: Future integration with UI/AI managers
	// if (IsPlayerControlled(Actor))
	//     BattleUIManager->ShowActionMenu(Actor);
	// else
	//     AIDecisionManager->MakeDecisionAsync(Actor, callback);

	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] STUB: RequestActionFromActor for %s"),
		*Actor->GetName());

	// For testing: auto-advance after delay
	if (bAutoAdvanceTurns)
	{
		GetWorld()->GetTimerManager().SetTimer(
			AutoAdvanceTimerHandle,
			this,
			&ACombatOrchestrator::OnActionCompleted,
			AutoAdvanceDelay,
			false
		);

		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Auto-advancing in %.1fs..."), AutoAdvanceDelay);
	}
}

ECombatState ACombatOrchestrator::CheckWinCondition()
{
	int32 Team0Alive = CountLivingMembers(Team0Combatants);
	int32 Team1Alive = CountLivingMembers(Team1Combatants);

	if (Team0Alive == 0 && Team1Alive == 0)
	{
		return ECombatState::Draw;
	}
	else if (Team1Alive == 0)
	{
		return ECombatState::Victory; // Team0 (players) win
	}
	else if (Team0Alive == 0)
	{
		return ECombatState::Defeat; // Team1 (enemies) win
	}

	return ECombatState::InProgress;
}

int32 ACombatOrchestrator::CountLivingMembers(const TArray<AActor*>& Team)
{
	int32 Count = 0;
	for (AActor* Actor : Team)
	{
		if (IsActorAlive(Actor))
		{
			Count++;
		}
	}
	return Count;
}

bool ACombatOrchestrator::IsActorAlive(AActor* Actor)
{
	if (!Actor)
		return false;

	UCharacterDataComponent* CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp)
		return false;

	return CharComp->bIsAlive;
}

FCombatResult ACombatOrchestrator::BuildCombatResult()
{
	FCombatResult Result;
	Result.FinalState = CombatState;
	Result.TotalTurns = CurrentTurnNumber;
	Result.Team0Survivors = CountLivingMembers(Team0Combatants);
	Result.Team1Survivors = CountLivingMembers(Team1Combatants);

	// Find last standing actor (for victory screen)
	if (Result.Team0Survivors > 0)
	{
		for (AActor* Actor : Team0Combatants)
		{
			if (IsActorAlive(Actor))
			{
				Result.LastActorStanding = Actor;
				break;
			}
		}
	}
	else if (Result.Team1Survivors > 0)
	{
		for (AActor* Actor : Team1Combatants)
		{
			if (IsActorAlive(Actor))
			{
				Result.LastActorStanding = Actor;
				break;
			}
		}
	}

	return Result;
}

// ========================================
// DEBUG TOOLS
// ========================================

void ACombatOrchestrator::DebugPrintCombatState()
{
	UE_LOG(LogTemp, Display, TEXT("=== COMBAT ORCHESTRATOR STATE ==="));
	UE_LOG(LogTemp, Display, TEXT("Combat State: %d"), (int32)CombatState);
	UE_LOG(LogTemp, Display, TEXT("Current Turn: %d"), CurrentTurnNumber);
	UE_LOG(LogTemp, Display, TEXT("Current Actor: %s"), CurrentActor ? *CurrentActor->GetName() : TEXT("None"));
	UE_LOG(LogTemp, Display, TEXT("Auto-Advance: %s (%.1fs delay)"),
		bAutoAdvanceTurns ? TEXT("ON") : TEXT("OFF"), AutoAdvanceDelay);

	UE_LOG(LogTemp, Display, TEXT("\nTeam 0 (%d members):"), Team0Combatants.Num());
	for (AActor* Actor : Team0Combatants)
	{
		UCharacterDataComponent* Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
		if (Comp)
		{
			UE_LOG(LogTemp, Display, TEXT("  %s - HP: %d/%d, Alive: %s"),
				*Actor->GetName(),
				Comp->CurrentHP, Comp->MaxHP,
				Comp->bIsAlive ? TEXT("Yes") : TEXT("No"));
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("  %s - (no CharacterDataComponent)"), *Actor->GetName());
		}
	}

	UE_LOG(LogTemp, Display, TEXT("\nTeam 1 (%d members):"), Team1Combatants.Num());
	for (AActor* Actor : Team1Combatants)
	{
		UCharacterDataComponent* Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
		if (Comp)
		{
			UE_LOG(LogTemp, Display, TEXT("  %s - HP: %d/%d, Alive: %s"),
				*Actor->GetName(),
				Comp->CurrentHP, Comp->MaxHP,
				Comp->bIsAlive ? TEXT("Yes") : TEXT("No"));
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("  %s - (no CharacterDataComponent)"), *Actor->GetName());
		}
	}

	UE_LOG(LogTemp, Display, TEXT("================================="));
}

void ACombatOrchestrator::DebugKillActor(AActor* Actor)
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugKillActor: null actor"));
		return;
	}

	UCharacterDataComponent* Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (Comp)
	{
		Comp->ServerTakeDamage(9999);
		TurnManagerRef->OnActorDied(Actor);
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] DEBUG: Killed %s"), *Actor->GetName());
	}
}

void ACombatOrchestrator::DebugHealAllTeam(int32 TeamIndex)
{
	TArray<AActor*>& Team = (TeamIndex == 0) ? Team0Combatants : Team1Combatants;

	for (AActor* Actor : Team)
	{
		UCharacterDataComponent* Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
		if (Comp)
		{
			if (!Comp->bIsAlive)
			{
				Comp->ServerResurrect(100);
				TurnManagerRef->OnActorResurrected(Actor);
			}
			else
			{
				Comp->ServerHeal(Comp->MaxHP);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] DEBUG: Healed all Team %d members"), TeamIndex);
}