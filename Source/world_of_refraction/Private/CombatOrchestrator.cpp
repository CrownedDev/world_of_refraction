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
#include "SpellData.h"
#include "AbilityData.h"
#include "BrokenDarknessManager.h"
#include "CharacterData.h"

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

	AIDecisionManagerRef = GI->GetSubsystem<UAIDecisionManager>();

	if (!AIDecisionManagerRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Failed to get AIDecisionManager subsystem!"));
	}

	// Auto-start combat if enabled (AFTER subsystems are cached)
	if (bAutoStartCombat)
	{
		// Delay to allow all actors to finish BeginPlay
		GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
														  { DebugStartCombatWithLevelActors(); });
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

void ACombatOrchestrator::StartCombat(const TArray<AActor *> &Team0, const TArray<AActor *> &Team1, EAIDifficulty Difficulty)
{
	// Store difficulty
	CombatDifficulty = Difficulty;

	// Register with AI manager
	if (AIDecisionManagerRef)
	{
		AIDecisionManagerRef->SetCombatOrchestrator(this);
	}

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

		// NEW: Reset all status bars
		for (AActor *Actor : Team0Combatants)
		{
			if (Actor)
			{
				StatusEffectManagerRef->ResetStatusBar(Actor);
			}
		}
		for (AActor *Actor : Team1Combatants)
		{
			if (Actor)
			{
				StatusEffectManagerRef->ResetStatusBar(Actor);
			}
		}
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

	// Unregister from AI manager
	if (AIDecisionManagerRef)
	{
		AIDecisionManagerRef->ClearCombatOrchestrator();
	}

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

	AActor *Actor = GetDebugActor();
	if (!Actor)
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

	// Check if this action requires async execution (projectile spells, attacks with movement)
	bool bRequiresAsync = false;

	if (Action.ActionType == EActionType::Spell && Action.SpellData)
	{
		// Projectile/Homing/Beam spells need async for defense window
		ESpellDeliveryType Delivery = Action.SpellData->DeliveryType;
		bRequiresAsync = (Delivery == ESpellDeliveryType::Projectile ||
						  Delivery == ESpellDeliveryType::Homing ||
						  Delivery == ESpellDeliveryType::Beam);
	}
	else if (Action.ActionType == EActionType::Attack && Action.AttackData)
	{
		// Attacks with movement data need async
		bRequiresAsync = (Action.AttackData->MovementData != nullptr);
	}
	else if (Action.ActionType == EActionType::Ability && Action.AbilityData)
	{
		// Abilities with movement data need async
		bRequiresAsync = (Action.AbilityData->MovementData != nullptr);
	}

	if (bRequiresAsync)
	{
		// Use async path - turn will end when action fully completes
		bWaitingForAsyncAction = true;

		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] %s executing %s asynchronously..."),
			   *CurrentActor->GetName(), *Action.GetActionName());

		ActionExecutorRef->ExecuteActionAsync(CurrentActor, Action,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));

		return true; // Action started, will complete later
	}

	// Synchronous execution for instant actions
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

	AActor *Actor = GetDebugActor();
	if (!Actor)
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

	AActor *Actor = GetDebugActor();
	if (!Actor)
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
	if (bWaitingForAsyncAction)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] OnActionCompleted ignored - waiting for async action"));
		return;
	}

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

	// Process Broken Darkness overflow (aura damage to nearby, self-damage, energy drain)
	ProcessBrokenDarknessOverflow(Actor);

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

	// Process status bar decay
	StatusEffectManagerRef->ProcessStatusBarDecay(Actor);

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
	// Check if AI-controlled
	if (IsActorAIControlled(Actor))
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Routing %s to AI decision"), *Actor->GetName());

		if (AIDecisionManagerRef)
		{
			// Disable auto-advance - AI will submit action
			GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
			AIDecisionManagerRef->RequestDecision(Actor);
			return;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] AI actor but no AIDecisionManager - falling back to auto-advance"));
		}
	}

	// Player turn - broadcast for UI
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

bool ACombatOrchestrator::IsActorAlive(AActor *Actor) const
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
// Enemy AI
// ========================================

bool ACombatOrchestrator::IsActorAIControlled(AActor *Actor) const
{
	if (!Actor)
	{
		return false;
	}

	UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!Comp || !Comp->CharacterData)
	{
		return false;
	}

	return Comp->CharacterData->ShouldUseAI();
}

TArray<AActor *> ACombatOrchestrator::GetLivingEnemies(AActor *ForActor) const
{
	TArray<AActor *> Enemies = GetEnemyTeam(ForActor);

	Enemies.RemoveAll([this](AActor *Actor)
					  { return !IsActorAlive(Actor); });

	return Enemies;
}

TArray<AActor *> ACombatOrchestrator::GetLivingAllies(AActor *ForActor) const
{
	TArray<AActor *> Allies;
	int32 TeamIndex = GetActorTeamIndex(ForActor);

	if (TeamIndex == 0)
	{
		Allies = Team0Combatants;
	}
	else if (TeamIndex == 1)
	{
		Allies = Team1Combatants;
	}

	// Remove self and dead allies
	Allies.RemoveAll([this, ForActor](AActor *Actor)
					 { return Actor == ForActor || !IsActorAlive(Actor); });

	return Allies;
}

// ========================================
// BROKEN DARKNESS HELPERS
// ========================================

UBrokenDarknessManager *ACombatOrchestrator::GetBrokenDarknessManager(AActor *Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}
	return Actor->FindComponentByClass<UBrokenDarknessManager>();
}

TArray<AActor *> ACombatOrchestrator::GetCombatantsInRange(AActor *Origin, float Range)
{
	TArray<AActor *> Result;

	if (!Origin || Range <= 0.0f)
	{
		return Result;
	}

	FVector OriginLocation = Origin->GetActorLocation();

	// Check all combatants from both teams
	for (AActor *Combatant : Team0Combatants)
	{
		if (Combatant && Combatant != Origin && IsActorAlive(Combatant))
		{
			float Distance = FVector::Dist(OriginLocation, Combatant->GetActorLocation());
			if (Distance <= Range)
			{
				Result.Add(Combatant);
			}
		}
	}

	for (AActor *Combatant : Team1Combatants)
	{
		if (Combatant && Combatant != Origin && IsActorAlive(Combatant))
		{
			float Distance = FVector::Dist(OriginLocation, Combatant->GetActorLocation());
			if (Distance <= Range)
			{
				Result.Add(Combatant);
			}
		}
	}

	return Result;
}

void ACombatOrchestrator::ProcessBrokenDarknessOverflow(AActor *Actor)
{
	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Actor);
	if (!BDManager || !BDManager->IsOverloaded())
	{
		return;
	}

	// Get aura range based on MaxEnergy stat
	float AuraRange = BDManager->CalculateAuraRange();

	// Find all combatants in range
	TArray<AActor *> ActorsInRange = GetCombatantsInRange(Actor, AuraRange);

	// Get character stats for damage/efficiency calculations
	float EffectDamageMult = 1.0f;
	float EfficiencyPercent = 0.0f;

	UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (CharComp && CharComp->CharacterData)
	{
		EffectDamageMult = CharComp->CharacterData->CalculateEffectDamageMultiplier();
		EfficiencyPercent = CharComp->CharacterData->CalculateEfficiencyMultiplier() * 100.0f;
	}

	// Process the overflow tick (aura damage, self-damage, energy drain)
	BDManager->ProcessOverloadTick(ActorsInRange, EffectDamageMult, EfficiencyPercent);

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] BD Overflow processed for %s - Range: %.1f, Targets: %d"),
		   *Actor->GetName(), AuraRange, ActorsInRange.Num());
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
	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugTestAttackMovement: No active turn"));
		return;
	}

	// Find target from opposing team
	AActor *Target = nullptr;
	bool bActorInTeam0 = Team0Combatants.Contains(Actor);

	if (bActorInTeam0 && Team1Combatants.Num() > 0)
	{
		Target = Team1Combatants[0];
	}
	else if (!bActorInTeam0 && Team0Combatants.Num() > 0)
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

	// Get weapon attack data via GetActiveWeapon (respects bUsingPrimary)
	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		UWeaponData *ActiveWeapon = Loadout->GetActiveWeapon();
		if (ActiveWeapon && ActiveWeapon->WeaponAttack)
		{
			AttackAction.AttackData = ActiveWeapon->WeaponAttack;
			UE_LOG(LogTemp, Log, TEXT("[DebugTestAttackMovement] Using attack: %s from weapon: %s"),
				   *ActiveWeapon->WeaponAttack->AttackName, *ActiveWeapon->WeaponName);
		}
	}

	if (!AttackAction.AttackData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestAttackMovement] No weapon attack data found on %s"),
			   *Actor->GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DebugTestAttackMovement] %s attacking %s"),
		   *Actor->GetName(), *Target->GetName());

	// Execute through ActionExecutor with callback
	if (ActionExecutorRef)
	{
		ActionExecutorRef->ExecuteActionAsync(Actor, AttackAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}
void ACombatOrchestrator::DebugTestAbilityMovement()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugTestAbilityMovement: No actor available"));
		return;
	}

	// Find target from opposing team
	AActor *Target = nullptr;
	bool bActorInTeam0 = Team0Combatants.Contains(Actor);

	if (bActorInTeam0 && Team1Combatants.Num() > 0)
	{
		Target = Team1Combatants[0];
	}
	else if (!bActorInTeam0 && Team0Combatants.Num() > 0)
	{
		Target = Team0Combatants[0];
	}

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugTestAbilityMovement: No valid target"));
		return;
	}

	// Build ability action
	FAction AbilityAction;
	AbilityAction.ActionType = EActionType::Ability;
	AbilityAction.Targets.Add(Target);

	// Get ability from LoadoutComponent
	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		TArray<UAbilityData *> AvailableAbilities = Loadout->GetAvailableAbilities();
		if (AvailableAbilities.Num() > 0)
		{
			AbilityAction.AbilityData = AvailableAbilities[0];
			UE_LOG(LogTemp, Log, TEXT("[DebugTestAbilityMovement] Using ability: %s"),
				   *AbilityAction.AbilityData->AbilityName);
		}
	}

	// Fallback to test ability
	if (!AbilityAction.AbilityData)
	{
		AbilityAction.AbilityData = LoadObject<UAbilityData>(nullptr,
															 TEXT("/Game/Data/Weapons/Gauntlets/Abilities/DA_Abilities_HeavyStrike.DA_Abilities_HeavyStrike"));
	}

	if (!AbilityAction.AbilityData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestAbilityMovement] No ability available on %s"),
			   *Actor->GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DebugTestAbilityMovement] %s using %s on %s"),
		   *Actor->GetName(), *AbilityAction.AbilityData->AbilityName, *Target->GetName());

	// Execute through ActionExecutor with callback
	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, AbilityAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugTestSpellMovement()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugTestSpellMovement: No actor available"));
		return;
	}

	// Find target from opposing team
	AActor *Target = nullptr;
	bool bActorInTeam0 = Team0Combatants.Contains(Actor);

	if (bActorInTeam0 && Team1Combatants.Num() > 0)
	{
		Target = Team1Combatants[0];
	}
	else if (!bActorInTeam0 && Team0Combatants.Num() > 0)
	{
		Target = Team0Combatants[0];
	}

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugTestSpellMovement: No valid target"));
		return;
	}

	// Build spell action
	FAction SpellAction;
	SpellAction.ActionType = EActionType::Spell;
	SpellAction.Targets.Add(Target);

	// Get spell from active slot (respects bUsingPrimary)
	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		TArray<USpellData *> ActiveSpells = Loadout->GetActiveSlotSpells();
		if (ActiveSpells.Num() > 0)
		{
			SpellAction.SpellData = ActiveSpells[0];
			UE_LOG(LogTemp, Log, TEXT("[DebugTestSpellMovement] Using spell: %s (Element: %s)"),
				   *SpellAction.SpellData->SpellName,
				   *UEnum::GetValueAsString(SpellAction.SpellData->Element));
		}
		else
		{
			// Fallback to all available spells
			TArray<USpellData *> AllSpells = Loadout->GetAvailableSpells();
			if (AllSpells.Num() > 0)
			{
				SpellAction.SpellData = AllSpells[0];
				UE_LOG(LogTemp, Log, TEXT("[DebugTestSpellMovement] Fallback spell: %s"),
					   *SpellAction.SpellData->SpellName);
			}
		}
	}

	// Final fallback to test spell
	if (!SpellAction.SpellData)
	{
		SpellAction.SpellData = LoadObject<USpellData>(nullptr,
													   TEXT("/Game/Data/Spells/Fire/DA_Spell_Fireball.DA_Spell_Fireball"));
	}

	if (!SpellAction.SpellData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestSpellMovement] No spell available on %s"),
			   *Actor->GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DebugTestSpellMovement] %s casting %s on %s"),
		   *Actor->GetName(), *SpellAction.SpellData->SpellName, *Target->GetName());

	// Execute through ActionExecutor with callback
	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, SpellAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugExecuteSyncAttack()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteSyncAttack: No actor available"));
		return;
	}
	bool bUsingOverride = (DebugOverrideActor != nullptr);
	if (bUsingOverride)
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
	}

	// Find target from opposing team
	AActor *Target = nullptr;
	bool bActorInTeam0 = Team0Combatants.Contains(Actor);

	if (bActorInTeam0 && Team1Combatants.Num() > 0)
	{
		Target = Team1Combatants[0];
	}
	else if (!bActorInTeam0 && Team0Combatants.Num() > 0)
	{
		Target = Team0Combatants[0];
	}

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteSyncAttack: No valid target"));
		return;
	}

	// Get weapon attack data via GetActiveWeapon (respects bUsingPrimary)
	UWeaponAttackData *AttackData = nullptr;
	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		UWeaponData *ActiveWeapon = Loadout->GetActiveWeapon();
		if (ActiveWeapon && ActiveWeapon->WeaponAttack)
		{
			AttackData = ActiveWeapon->WeaponAttack;
		}
	}
	if (!AttackData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugExecuteSyncAttack] No weapon attack data on %s"),
			   *Actor->GetName());
		return;
	}

	// Build action
	FAction AttackAction;
	AttackAction.ActionType = EActionType::Attack;
	AttackAction.AttackData = AttackData;
	AttackAction.Targets.Add(Target);

	// Get target HP before
	int32 TargetHPBefore = 0;
	if (UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>())
	{
		TargetHPBefore = TargetComp->CurrentHP;
	}

	UE_LOG(LogTemp, Log, TEXT("[DebugExecuteSyncAttack] %s attacking %s with %s (Target HP: %d)"),
		   *Actor->GetName(), *Target->GetName(), *AttackData->AttackName, TargetHPBefore);

	// Execute SYNCHRONOUSLY - bypasses movement entirely
	if (ActionExecutorRef)
	{
		TArray<AActor *> Targets;
		Targets.Add(Target);

		FActionResult Result = ActionExecutorRef->ExecuteAttack(
			Actor,
			AttackData,
			Targets,
			false // bIsInfused
		);

		// Get target HP after
		int32 TargetHPAfter = 0;
		if (UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>())
		{
			TargetHPAfter = TargetComp->CurrentHP;
		}

		UE_LOG(LogTemp, Log, TEXT("[DebugExecuteSyncAttack] Result: %s | Damage: %d | Target HP: %d -> %d | Crit: %s"),
			   Result.bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"),
			   Result.TotalDamageDealt,
			   TargetHPBefore, TargetHPAfter,
			   Result.bWasCritical ? TEXT("YES") : TEXT("NO"));

		// Broadcast for UI
		OnActionExecuted.Broadcast(Actor, Result);
	}

	if (!bUsingOverride)
	{
		OnActionCompleted();
	}
}

void ACombatOrchestrator::DebugExecuteSyncSpell()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteSyncSpell: No actor available"));
		return;
	}

	bool bUsingOverride = (DebugOverrideActor != nullptr);
	// Clear auto-advance timer when using debug
	if (bUsingOverride)
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
	}

	// Find target from opposing team
	AActor *Target = nullptr;
	bool bActorInTeam0 = Team0Combatants.Contains(Actor);

	if (bActorInTeam0 && Team1Combatants.Num() > 0)
	{
		Target = Team1Combatants[0];
	}
	else if (!bActorInTeam0 && Team0Combatants.Num() > 0)
	{
		Target = Team0Combatants[0];
	}

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteSyncSpell: No valid target"));
		return;
	}

	// Try to get a spell from LoadoutComponent
	USpellData *SpellData = nullptr;
	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		TArray<USpellData *> AvailableSpells = Loadout->GetAvailableSpells();
		if (AvailableSpells.Num() > 0)
		{
			SpellData = AvailableSpells[0];
		}
	}

	// Fallback: Load a test spell directly
	if (!SpellData)
	{
		SpellData = LoadObject<USpellData>(nullptr,
										   TEXT("/Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_FireBall.DA_Spells_Fire_FireBall"));
	}

	if (!SpellData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugExecuteSyncSpell] No spell available"));
		return;
	}

	// Get target HP before
	int32 TargetHPBefore = 0;
	if (UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>())
	{
		TargetHPBefore = TargetComp->CurrentHP;
	}

	UE_LOG(LogTemp, Log, TEXT("[DebugExecuteSyncSpell] %s casting %s on %s (Target HP: %d)"),
		   *Actor->GetName(), *SpellData->SpellName, *Target->GetName(), TargetHPBefore);

	UE_LOG(LogTemp, Warning, TEXT("[DEBUG] Spell: %s | TargetType: %d | DeliveryType: %d | CastAnim: %s"),
		   *SpellData->SpellName,
		   (int32)SpellData->TargetType,
		   (int32)SpellData->DeliveryType,
		   SpellData->CastAnimation ? *SpellData->CastAnimation->GetName() : TEXT("NONE"));

	// Execute SYNCHRONOUSLY
	if (ActionExecutorRef)
	{
		TArray<AActor *> Targets;
		Targets.Add(Target);

		FActionResult Result = ActionExecutorRef->ExecuteSpell(
			Actor,
			SpellData,
			Targets,
			0 // InfusionLevel
		);

		// Get target HP after
		int32 TargetHPAfter = 0;
		if (UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>())
		{
			TargetHPAfter = TargetComp->CurrentHP;
		}

		UE_LOG(LogTemp, Log, TEXT("[DebugExecuteSyncSpell] Result: %s | Damage: %d | EP Spent: %d | Target HP: %d -> %d"),
			   Result.bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"),
			   Result.TotalDamageDealt,
			   Result.EnergySpent,
			   TargetHPBefore, TargetHPAfter);

		// Broadcast for UI
		OnActionExecuted.Broadcast(Actor, Result);
	}

	if (!bUsingOverride)
	{
		OnActionCompleted();
	}
}

void ACombatOrchestrator::DebugTestPrimarySpell()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestPrimarySpell] No actor available"));
		return;
	}

	AActor *Target = nullptr;
	bool bActorInTeam0 = Team0Combatants.Contains(Actor);
	if (bActorInTeam0 && Team1Combatants.Num() > 0)
		Target = Team1Combatants[0];
	else if (!bActorInTeam0 && Team0Combatants.Num() > 0)
		Target = Team0Combatants[0];

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestPrimarySpell] No valid target"));
		return;
	}

	FAction SpellAction;
	SpellAction.ActionType = EActionType::Spell;
	SpellAction.Targets.Add(Target);

	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		TArray<USpellData *> PrimarySpells = Loadout->GetPrimarySlotSpells();
		if (PrimarySpells.Num() > 0)
		{
			SpellAction.SpellData = PrimarySpells[0];
			UE_LOG(LogTemp, Log, TEXT("[DebugTestPrimarySpell] Using PRIMARY spell: %s (Element: %s)"),
				   *SpellAction.SpellData->SpellName,
				   *UEnum::GetValueAsString(SpellAction.SpellData->Element));
		}
	}

	if (!SpellAction.SpellData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestPrimarySpell] No PRIMARY slot spells on %s"), *Actor->GetName());
		return;
	}

	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, SpellAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugTestSecondarySpell()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestSecondarySpell] No actor available"));
		return;
	}

	AActor *Target = nullptr;
	bool bActorInTeam0 = Team0Combatants.Contains(Actor);
	if (bActorInTeam0 && Team1Combatants.Num() > 0)
		Target = Team1Combatants[0];
	else if (!bActorInTeam0 && Team0Combatants.Num() > 0)
		Target = Team0Combatants[0];

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestSecondarySpell] No valid target"));
		return;
	}

	FAction SpellAction;
	SpellAction.ActionType = EActionType::Spell;
	SpellAction.Targets.Add(Target);

	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		TArray<USpellData *> SecondarySpells = Loadout->GetSecondarySlotSpells();
		if (SecondarySpells.Num() > 0)
		{
			SpellAction.SpellData = SecondarySpells[0];
			UE_LOG(LogTemp, Log, TEXT("[DebugTestSecondarySpell] Using SECONDARY spell: %s (Element: %s)"),
				   *SpellAction.SpellData->SpellName,
				   *UEnum::GetValueAsString(SpellAction.SpellData->Element));
		}
	}

	if (!SpellAction.SpellData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestSecondarySpell] No SECONDARY slot spells on %s (Generic with evolved weapon crystal only)"), *Actor->GetName());
		return;
	}

	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, SpellAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugExecuteSyncAbility()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteSyncAbility: No actor available"));
		return;
	}
	bool bUsingOverride = (DebugOverrideActor != nullptr);

	if (bUsingOverride)
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
	}

	// Find target from opposing team
	AActor *Target = nullptr;
	bool bActorInTeam0 = Team0Combatants.Contains(Actor);

	if (bActorInTeam0 && Team1Combatants.Num() > 0)
	{
		Target = Team1Combatants[0];
	}
	else if (!bActorInTeam0 && Team0Combatants.Num() > 0)
	{
		Target = Team0Combatants[0];
	}

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteSyncAbility: No valid target"));
		return;
	}

	// Try to get an ability from LoadoutComponent
	UAbilityData *AbilityData = nullptr;
	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		TArray<UAbilityData *> AvailableAbilities = Loadout->GetAvailableAbilities();
		if (AvailableAbilities.Num() > 0)
		{
			AbilityData = AvailableAbilities[0];
		}
	}

	// Fallback: Load a test ability directly
	if (!AbilityData)
	{
		AbilityData = LoadObject<UAbilityData>(nullptr,
											   TEXT("/Game/Data/Weapons/Gauntlets/Abilities/DA_Abilities_HeavyStrike.DA_Abilities_HeavyStrike"));
	}

	if (!AbilityData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugExecuteSyncAbility] No ability available"));
		return;
	}

	// Get target HP before
	int32 TargetHPBefore = 0;
	if (UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>())
	{
		TargetHPBefore = TargetComp->CurrentHP;
	}

	UE_LOG(LogTemp, Log, TEXT("[DebugExecuteSyncAbility] %s using %s on %s (Target HP: %d)"),
		   *Actor->GetName(), *AbilityData->AbilityName, *Target->GetName(), TargetHPBefore);

	// Execute SYNCHRONOUSLY
	if (ActionExecutorRef)
	{
		TArray<AActor *> Targets;
		Targets.Add(Target);

		FActionResult Result = ActionExecutorRef->ExecuteAbility(
			Actor,
			AbilityData,
			Targets,
			false // bIsElementInfused
		);

		// Get target HP after
		int32 TargetHPAfter = 0;
		if (UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>())
		{
			TargetHPAfter = TargetComp->CurrentHP;
		}

		UE_LOG(LogTemp, Log, TEXT("[DebugExecuteSyncAbility] Result: %s | Damage: %d | EP Spent: %d | Target HP: %d -> %d"),
			   Result.bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"),
			   Result.TotalDamageDealt,
			   Result.EnergySpent,
			   TargetHPBefore, TargetHPAfter);

		// Broadcast for UI
		OnActionExecuted.Broadcast(Actor, Result);
	}

	if (!bUsingOverride)
	{
		OnActionCompleted();
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

AActor *ACombatOrchestrator::GetDebugActor() const
{
	// Use override if set, otherwise fall back to current turn actor
	if (DebugOverrideActor)
	{
		return DebugOverrideActor;
	}
	return CurrentActor;
}
