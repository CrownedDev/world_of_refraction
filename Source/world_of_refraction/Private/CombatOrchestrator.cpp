// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatOrchestrator.h"
#include "TurnManager.h"
#include "StatusEffectManager.h"
#include "ActionExecutor.h"
#include "CharacterDataComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InventoryComponent.h"
#include "LoadoutComponent.h"
#include "CombatGridSubsystem.h"
#include "LoadoutComponent.h"
#include "WeaponData.h"
#include "WeaponAttackData.h"

ACombatOrchestrator::ACombatOrchestrator()
{
	PrimaryActorTick.bCanEverTick = false;

	CombatState = ECombatState::Idle;
	CurrentActor = nullptr;
	CurrentTurnNumber = 0;
	TurnManagerRef = nullptr;
	StatusEffectManagerRef = nullptr;
	ActionExecutorRef = nullptr;
	bWaitingForAsyncAction = false;
}

void ACombatOrchestrator::BeginPlay()
{
	Super::BeginPlay();

	// Cache subsystem references FIRST
	UGameInstance *GI = GetGameInstance();
	if (GI)
	{
		TurnManagerRef = GI->GetSubsystem<UTurnManager>();
		StatusEffectManagerRef = GI->GetSubsystem<UStatusEffectManager>();
		ActionExecutorRef = GI->GetSubsystem<UActionExecutor>();
	}

	if (!TurnManagerRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Failed to get TurnManager subsystem!"));
	}

	if (!StatusEffectManagerRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Failed to get StatusEffectManager subsystem!"));
	}

	if (!ActionExecutorRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Failed to get ActionExecutor subsystem!"));
	}

	// Auto-start combat if enabled (AFTER subsystems are cached)
	if (bAutoStartCombat)
	{
		DebugStartCombatWithLevelActors();
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

void ACombatOrchestrator::StartCombat(const TArray<AActor *> &Team0, const TArray<AActor *> &Team1)
{
	if (CombatState != ECombatState::Idle)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] StartCombat called while combat already active. Forcing end."));
		ForceEndCombat();
	}

	if (!TurnManagerRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Cannot start combat - TurnManager not available!"));
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
	bWaitingForAsyncAction = false;

	SetCombatState(ECombatState::Initializing);

	// Prepare loadouts for battle
	PrepareAllLoadoutsForBattle();

	// Assign grid positions and place actors
	if (UCombatGridSubsystem *Grid = GetGameInstance()->GetSubsystem<UCombatGridSubsystem>())
	{
		FVector ArenaCenter = GetActorLocation();

		Grid->AutoAssignTeam(Team0Combatants, 0, ECombatRow::Middle);
		Grid->AutoAssignTeam(Team1Combatants, 1, ECombatRow::Middle);
		Grid->PlaceAllActors(ArenaCenter);
		Grid->UpdateAllActorFacing(ArenaCenter);

		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Grid positions assigned and actors placed"));
	}

	// Set arena center for ActionExecutor movement calculations
	if (UActionExecutor *Executor = GetGameInstance()->GetSubsystem<UActionExecutor>())
	{
		Executor->SetArenaCenter(GetActorLocation());
	}

	// Bind to TurnManager events and start turns (if TurnManager available)
	if (!TurnManagerRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] TurnManager not available - grid positioned but turns won't advance"));
		return;
	}

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

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Force ending combat with state %d"), (int32)ForcedState);

	// Clear auto-advance timer
	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);

	UnbindTurnManagerEvents();

	// Clear all status effects from combat
	if (StatusEffectManagerRef)
	{
		StatusEffectManagerRef->ClearAllEffects();
	}

	// === Consume used items and reset loadouts ===
	ConsumeAllUsedItems();

	// Clear grid positions
	if (UCombatGridSubsystem *Grid = GetGameInstance()->GetSubsystem<UCombatGridSubsystem>())
	{
		Grid->ClearAllPositions();
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Grid positions cleared"));
	}

	if (TurnManagerRef)
	{
		TurnManagerRef->EndCombat();
	}

	SetCombatState(ForcedState);

	FCombatResult Result = BuildCombatResult();
	OnCombatResultReady.Broadcast(Result);

	// Reset for next combat
	Team0Combatants.Empty();
	Team1Combatants.Empty();
	CurrentActor = nullptr;
	CurrentTurnNumber = 0;
	bWaitingForAsyncAction = false;

	SetCombatState(ECombatState::Idle);
}

// ========================================
// ACTION SUBMISSION
// ========================================

bool ACombatOrchestrator::SubmitAction(const FAction &Action)
{
	if (CombatState != ECombatState::InProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] SubmitAction called but combat not in progress"));
		return false;
	}

	if (!CurrentActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] SubmitAction called with no current actor"));
		return false;
	}

	if (!ActionExecutorRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] ActionExecutor not available!"));
		return false;
	}

	if (bWaitingForAsyncAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Already waiting for async action to complete"));
		return false;
	}

	// Clear auto-advance timer since we're submitting a real action
	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);

	// Validate action
	FActionValidationResult Validation = ActionExecutorRef->ValidateAction(CurrentActor, Action);
	if (!Validation.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Action validation failed: %s"), *Validation.ErrorMessage);
		return false;
	}

	// Execute action synchronously
	FActionResult Result = ActionExecutorRef->ExecuteAction(CurrentActor, Action);

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] %s executed %s: %s (Damage: %d, Healing: %d)"),
		   *CurrentActor->GetName(),
		   *Action.GetActionName(),
		   Result.bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"),
		   Result.TotalDamageDealt,
		   Result.TotalHealingDone);

	// Broadcast result for UI
	OnActionExecuted.Broadcast(CurrentActor, Result);

	// End turn
	OnActionCompleted();

	return Result.bSuccess;
}

void ACombatOrchestrator::SubmitActionAsync(const FAction &Action)
{
	if (CombatState != ECombatState::InProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] SubmitActionAsync called but combat not in progress"));
		return;
	}

	if (!CurrentActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] SubmitActionAsync called with no current actor"));
		return;
	}

	if (!ActionExecutorRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] ActionExecutor not available!"));
		return;
	}

	if (bWaitingForAsyncAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Already waiting for async action to complete"));
		return;
	}

	// Clear auto-advance timer
	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);

	// Validate action
	FActionValidationResult Validation = ActionExecutorRef->ValidateAction(CurrentActor, Action);
	if (!Validation.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Async action validation failed: %s"), *Validation.ErrorMessage);
		return;
	}

	bWaitingForAsyncAction = true;

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] %s executing %s asynchronously..."),
		   *CurrentActor->GetName(), *Action.GetActionName());

	// Execute action asynchronously with callback
	ActionExecutorRef->ExecuteActionAsync(CurrentActor, Action,
										  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
}

FActionValidationResult ACombatOrchestrator::ValidateAction(const FAction &Action) const
{
	if (!ActionExecutorRef)
	{
		return FActionValidationResult(false, TEXT("ActionExecutor not available"));
	}

	if (!CurrentActor)
	{
		return FActionValidationResult(false, TEXT("No current actor"));
	}

	return ActionExecutorRef->ValidateAction(CurrentActor, Action);
}

void ACombatOrchestrator::HandleAsyncActionCompleted(const FActionResult &Result)
{
	bWaitingForAsyncAction = false;

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Async action completed: %s (Damage: %d, Healing: %d)"),
		   Result.bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"),
		   Result.TotalDamageDealt,
		   Result.TotalHealingDone);

	// Broadcast result for UI
	if (CurrentActor)
	{
		OnActionExecuted.Broadcast(CurrentActor, Result);
	}

	// End turn
	OnActionCompleted();
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

		// Clear all status effects from combat
		if (StatusEffectManagerRef)
		{
			StatusEffectManagerRef->ClearAllEffects();
		}

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

void ACombatOrchestrator::HandleTurnStarted(AActor *Actor, int32 TurnNumber)
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

void ACombatOrchestrator::HandleTurnEnded(AActor *Actor, int32 TurnNumber)
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
		// Clear all status effects from combat
		if (StatusEffectManagerRef)
		{
			StatusEffectManagerRef->ClearAllEffects();
		}

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

void ACombatOrchestrator::ProcessStartOfTurnEffects(AActor *Actor)
{
	if (!StatusEffectManagerRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] StatusEffectManager not available for start-of-turn processing"));
		return;
	}

	// Delegate to StatusEffectManager - processes buffs, regen, etc.
	StatusEffectManagerRef->ProcessStartOfTurnEffects(Actor);

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Processed start-of-turn effects for %s"),
		   *Actor->GetName());
}

void ACombatOrchestrator::ProcessEndOfTurnEffects(AActor *Actor)
{
	if (!StatusEffectManagerRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] StatusEffectManager not available for end-of-turn processing"));
		return;
	}

	// Delegate to StatusEffectManager - processes DOTs, ticks durations, expires effects
	StatusEffectManagerRef->ProcessEndOfTurnEffects(Actor);

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Processed end-of-turn effects for %s"),
		   *Actor->GetName());
}

void ACombatOrchestrator::RequestActionFromActor(AActor *Actor)
{
	// Broadcast that we're waiting for action
	OnActionRequested.Broadcast(Actor);

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Requesting action from %s"), *Actor->GetName());

	// For testing: auto-advance after delay if no UI/AI integration
	if (bAutoAdvanceTurns)
	{
		GetWorld()->GetTimerManager().SetTimer(
			AutoAdvanceTimerHandle,
			this,
			&ACombatOrchestrator::OnActionCompleted,
			AutoAdvanceDelay,
			false);

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

int32 ACombatOrchestrator::CountLivingMembers(const TArray<AActor *> &Team)
{
	int32 Count = 0;
	for (AActor *Actor : Team)
	{
		if (IsActorAlive(Actor))
		{
			Count++;
		}
	}
	return Count;
}

bool ACombatOrchestrator::IsActorAlive(AActor *Actor)
{
	if (!Actor)
		return false;

	UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp)
		return false;

	return CharComp->bIsAlive;
}

int32 ACombatOrchestrator::GetActorTeamIndex(AActor *Actor) const
{
	if (Team0Combatants.Contains(Actor))
		return 0;
	if (Team1Combatants.Contains(Actor))
		return 1;
	return -1;
}

TArray<AActor *> ACombatOrchestrator::GetEnemyTeam(AActor *Actor) const
{
	int32 TeamIndex = GetActorTeamIndex(Actor);
	if (TeamIndex == 0)
		return Team1Combatants;
	if (TeamIndex == 1)
		return Team0Combatants;
	return TArray<AActor *>();
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
		for (AActor *Actor : Team0Combatants)
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
		for (AActor *Actor : Team1Combatants)
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

void ACombatOrchestrator::PrepareAllLoadoutsForBattle()
{
	auto PrepareActor = [](AActor *Actor)
	{
		if (!Actor)
			return;

		UInventoryComponent *Inventory = Actor->FindComponentByClass<UInventoryComponent>();
		ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();

		if (Loadout && Inventory)
		{
			if (Loadout->PrepareForBattle(Inventory))
			{
				UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Prepared loadout for %s"), *Actor->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Failed to prepare loadout for %s"), *Actor->GetName());
			}
		}
	};

	for (AActor *Actor : Team0Combatants)
	{
		PrepareActor(Actor);
	}

	for (AActor *Actor : Team1Combatants)
	{
		PrepareActor(Actor);
	}
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
	UE_LOG(LogTemp, Display, TEXT("Waiting for Async: %s"), bWaitingForAsyncAction ? TEXT("Yes") : TEXT("No"));

	UE_LOG(LogTemp, Display, TEXT("\nTeam 0 (%d members):"), Team0Combatants.Num());
	for (AActor *Actor : Team0Combatants)
	{
		UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
		if (Comp)
		{
			UE_LOG(LogTemp, Display, TEXT("  %s - HP: %d/%d, EP: %d/%d, Alive: %s"),
				   *Actor->GetName(),
				   Comp->CurrentHP, Comp->MaxHP,
				   Comp->CurrentEP, Comp->MaxEP,
				   Comp->bIsAlive ? TEXT("Yes") : TEXT("No"));
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("  %s - (no CharacterDataComponent)"), *Actor->GetName());
		}
	}

	UE_LOG(LogTemp, Display, TEXT("\nTeam 1 (%d members):"), Team1Combatants.Num());
	for (AActor *Actor : Team1Combatants)
	{
		UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
		if (Comp)
		{
			UE_LOG(LogTemp, Display, TEXT("  %s - HP: %d/%d, EP: %d/%d, Alive: %s"),
				   *Actor->GetName(),
				   Comp->CurrentHP, Comp->MaxHP,
				   Comp->CurrentEP, Comp->MaxEP,
				   Comp->bIsAlive ? TEXT("Yes") : TEXT("No"));
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("  %s - (no CharacterDataComponent)"), *Actor->GetName());
		}
	}

	UE_LOG(LogTemp, Display, TEXT("================================="));
}

void ACombatOrchestrator::DebugKillActor(AActor *Actor)
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugKillActor: null actor"));
		return;
	}

	UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (Comp)
	{
		Comp->ServerTakeDamage(9999);
		TurnManagerRef->OnActorDied(Actor);
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] DEBUG: Killed %s"), *Actor->GetName());

		// Update facing for remaining actors
		UCombatGridSubsystem *Grid = GetGameInstance()->GetSubsystem<UCombatGridSubsystem>();
		if (Grid)
		{
			Grid->UpdateAllActorFacing(GetActorLocation());
		}
	}
}

void ACombatOrchestrator::DebugHealAllTeam(int32 TeamIndex)
{
	TArray<AActor *> &Team = (TeamIndex == 0) ? Team0Combatants : Team1Combatants;

	for (AActor *Actor : Team)
	{
		UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
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

void ACombatOrchestrator::DebugExecuteTestAction()
{
	if (CombatState != ECombatState::InProgress || !CurrentActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteTestAction: No active turn"));
		return;
	}

	// Find a living enemy target
	TArray<AActor *> Enemies = GetEnemyTeam(CurrentActor);
	AActor *Target = nullptr;
	for (AActor *Enemy : Enemies)
	{
		if (IsActorAlive(Enemy))
		{
			Target = Enemy;
			break;
		}
	}

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteTestAction: No living enemy targets"));
		OnActionCompleted();
		return;
	}

	// Create a Defend action (simplest - no data asset needed)
	FAction TestAction;
	TestAction.ActionType = EActionType::Defend;

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] DEBUG: %s executing Defend action"),
		   *CurrentActor->GetName());

	SubmitAction(TestAction);
}

void ACombatOrchestrator::DebugTestAttackMovement()
{
	if (!CurrentActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugTestAttackMovement: No active turn"));
		return;
	}

	// Find target from opposing team
	AActor *Target = nullptr;
	bool bCurrentActorInTeam0 = Team0Combatants.Contains(CurrentActor);

	if (bCurrentActorInTeam0 && Team1Combatants.Num() > 0)
	{
		Target = Team1Combatants[0];
	}
	else if (!bCurrentActorInTeam0 && Team0Combatants.Num() > 0)
	{
		Target = Team0Combatants[0];
	}

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugTestAttackMovement: No valid target"));
		return;
	}

	// Build attack action
	FAction AttackAction;
	AttackAction.ActionType = EActionType::Attack;
	AttackAction.Targets.Add(Target);

	// Get weapon attack data from LoadoutComponent
	if (ULoadoutComponent *Loadout = CurrentActor->FindComponentByClass<ULoadoutComponent>())
	{
		FCombatLoadout ActiveLoadout = Loadout->GetActiveLoadout();
		if (ActiveLoadout.PrimaryWeapon.IsValid() && ActiveLoadout.PrimaryWeapon.WeaponEntry.Weapon)
		{
			UWeaponData *Weapon = ActiveLoadout.PrimaryWeapon.WeaponEntry.Weapon;
			if (Weapon->WeaponAttack)
			{
				AttackAction.AttackData = Weapon->WeaponAttack;
				UE_LOG(LogTemp, Log, TEXT("[DebugTestAttackMovement] Using attack: %s from weapon: %s"),
					   *Weapon->WeaponAttack->AttackName, *Weapon->WeaponName);
			}
		}
	}

	if (!AttackAction.AttackData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestAttackMovement] No weapon attack data found on %s"),
			   *CurrentActor->GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DebugTestAttackMovement] %s attacking %s"),
		   *CurrentActor->GetName(), *Target->GetName());

	// Execute through ActionExecutor with callback
	if (ActionExecutorRef)
	{
		ActionExecutorRef->ExecuteActionAsync(CurrentActor, AttackAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugDrawCombatGrid(float Duration)
{
	UCombatGridSubsystem *Grid = GetGameInstance()->GetSubsystem<UCombatGridSubsystem>();
	if (!Grid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] CombatGridSubsystem not available"));
		return;
	}

	// Use world origin or this actor's location as arena center
	FVector ArenaCenter = GetActorLocation();

	// Draw the grid layout
	Grid->DebugDrawGrid(ArenaCenter, Duration);

	// Draw actor positions
	Grid->DebugDrawActorPositions(ArenaCenter, Duration);

	// Also log to output
	Grid->DebugLogAllPositions();
	Grid->DebugLogModifiers();
}

void ACombatOrchestrator::DebugStartCombatWithLevelActors()
{
	UWorld *World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] No world available"));
		return;
	}

	// Find actors by tag
	TArray<AActor *> Team0;
	TArray<AActor *> Team1;

	TArray<AActor *> AllActors;
	UGameplayStatics::GetAllActorsWithTag(World, FName("Team0"), AllActors);
	for (AActor *Actor : AllActors)
	{
		if (Actor->FindComponentByClass<UCharacterDataComponent>())
		{
			Team0.Add(Actor);
			UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Found Team0: %s"), *Actor->GetName());
		}
	}

	AllActors.Empty();
	UGameplayStatics::GetAllActorsWithTag(World, FName("Team1"), AllActors);
	for (AActor *Actor : AllActors)
	{
		if (Actor->FindComponentByClass<UCharacterDataComponent>())
		{
			Team1.Add(Actor);
			UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Found Team1: %s"), *Actor->GetName());
		}
	}

	// Validate
	if (Team0.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] No actors with 'Team0' tag found. Add tag to player characters."));
		return;
	}
	if (Team1.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] No actors with 'Team1' tag found. Add tag to enemy characters."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Starting combat: %d vs %d"), Team0.Num(), Team1.Num());

	// Start combat (handles grid assignment and actor placement)
	StartCombat(Team0, Team1);

	// Draw debug visualization
	DebugDrawCombatGrid(10.0f);

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Combat started and actors positioned!"));
}

void ACombatOrchestrator::ConsumeAllUsedItems()
{
	auto ConsumeForActor = [](AActor *Actor)
	{
		if (!Actor)
			return;

		UInventoryComponent *Inventory = Actor->FindComponentByClass<UInventoryComponent>();
		ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();

		if (Loadout && Inventory)
		{
			Loadout->ConsumeUsedItems(Inventory);
			Loadout->ResetBattleState();
			UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Consumed items and reset loadout for %s"), *Actor->GetName());
		}
	};

	for (AActor *Actor : Team0Combatants)
	{
		ConsumeForActor(Actor);
	}

	for (AActor *Actor : Team1Combatants)
	{
		ConsumeForActor(Actor);
	}
}