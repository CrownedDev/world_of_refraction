// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/CombatOrchestrator.h"
#include "Combat/TurnManager.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Character/CharacterDataComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Loadout/LoadoutComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Weapons/WeaponAttackData.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Character/CharacterData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/EAttachedItemKind.h"
#include "Equipment/Durability/DurabilityConstants.h"
#include "Inventory/ItemEffectType.h"
#include "Combat/Camera/CombatCameraManager.h"
#include "Combat/Mechanics/WeatherStateManager.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "Equipment/Crystals/CrystalManager.h"

ACombatOrchestrator::ACombatOrchestrator()
{
	PrimaryActorTick.bCanEverTick = false;

	CombatState = ECombatState::Idle;
	CurrentActor = nullptr;
	CurrentTurnNumber = 0;

	TurnManagerRef = nullptr;
	SkillEffectManagerRef = nullptr;
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
		SkillEffectManagerRef = GI->GetSubsystem<USkillEffectManager>();
		ActionExecutorRef = GI->GetSubsystem<UActionExecutor>();
	}

	if (!TurnManagerRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Failed to get TurnManager subsystem!"));
	}

	if (!SkillEffectManagerRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Failed to get SkillEffectManager subsystem!"));
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
	// Unregister from systems that hold a back-reference to us
	if (AIDecisionManagerRef)
	{
		AIDecisionManagerRef->ClearCombatOrchestrator();
	}
	if (UGameInstance *GI = GetGameInstance())
	{
		if (UCombatCommandMenuSubsystem *Menu = GI->GetSubsystem<UCombatCommandMenuSubsystem>())
		{
			Menu->ClearCombatOrchestrator();
		}
	}

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

	// Register with combat command menu
	if (UGameInstance *GI = GetGameInstance())
	{
		if (UCombatCommandMenuSubsystem *Menu = GI->GetSubsystem<UCombatCommandMenuSubsystem>())
		{
			Menu->SetCombatOrchestrator(this);
		}
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

	// Initialise weather leaders
	if (UWeatherStateManager *WeatherManager = GetGameInstance()->GetSubsystem<UWeatherStateManager>())
	{
		WeatherManager->InitialiseLeaders(Team0Combatants, Team1Combatants);
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

	OnCombatStartedUI(Team0Combatants, Team1Combatants);

	SetCombatState(ECombatState::InProgress);

	// Initialize camera system
	ACombatCameraManager *CamMgr = FindCameraManager();
	if (CamMgr)
	{
		CamMgr->InitializeForCombat(this);
	}
}

void ACombatOrchestrator::ForceEndCombat(ECombatState ForcedState)
{
	if (CombatState == ECombatState::Idle)
		return;

	if (CameraManager)
	{
		CameraManager->EndCombat();
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Force ending combat with state %d"), (int32)ForcedState);

	if (UWeatherStateManager *WeatherManager = GetGameInstance()->GetSubsystem<UWeatherStateManager>())
	{
		WeatherManager->EndCombat();
	}

	// Clear auto-advance timer
	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);

	UnbindTurnManagerEvents();

	// Clear all status effects from combat
	if (SkillEffectManagerRef)
	{
		SkillEffectManagerRef->ClearAllEffects();
	}

	// Reset all status bars — split out of SkillEffectManager into a dedicated subsystem.
	if (UStatusBuildupManager *BuildupManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatusBuildupManager>() : nullptr)
	{
		for (AActor *Actor : Team0Combatants)
		{
			if (Actor)
			{
				BuildupManager->ResetStatusBar(Actor);
			}
		}
		for (AActor *Actor : Team1Combatants)
		{
			if (Actor)
			{
				BuildupManager->ResetStatusBar(Actor);
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

	// Unregister listeners that hold back-references
	if (AIDecisionManagerRef)
	{
		AIDecisionManagerRef->ClearCombatOrchestrator();
	}
	if (UGameInstance *GI = GetGameInstance())
	{
		if (UCombatCommandMenuSubsystem *Menu = GI->GetSubsystem<UCombatCommandMenuSubsystem>())
		{
			Menu->ClearCombatOrchestrator();
		}
	}
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

	AActor *Actor = CurrentActor;
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
	UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Validation: %s - %s"),
		   Validation.bIsValid ? TEXT("VALID") : TEXT("INVALID"),
		   *Validation.ErrorMessage);
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
		bRequiresAsync = (Action.AttackData->ApproachData != nullptr);
	}
	else if (Action.ActionType == EActionType::Ability && Action.AbilityData)
	{
		// Abilities with movement data need async
		bRequiresAsync = Action.AbilityData->RequiresApproach();
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
	AActor *Actor = CurrentActor;
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

	AActor *Actor = CurrentActor;
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
		if (SkillEffectManagerRef)
		{
			SkillEffectManagerRef->ClearAllEffects();
		}

		// Phase B: destroy any crystals that broke during combat. Runs BEFORE
		// repair so destroyed crystals aren't candidates for repair.
		ApplyBetweenCombatCrystalDestruction();

		// Auto-repair crystals between combats. Only fires on completed battles
		// (ForceEndCombat does NOT repair — abort cases aren't "battles completed").
		ApplyBetweenCombatRepair();

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

void ACombatOrchestrator::HandleCombatEnded(int32 FinalTurnCount)
{
	// TurnManager ended combat (could be from no valid combatants)
	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] TurnManager ended combat at turn %d"), FinalTurnCount);

	if (CombatState == ECombatState::InProgress)
	{
		// Clear all status effects from combat
		if (SkillEffectManagerRef)
		{
			SkillEffectManagerRef->ClearAllEffects();
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
		TurnManagerRef->OnCombatEnded.AddDynamic(this, &ACombatOrchestrator::HandleCombatEnded);
	}
}

void ACombatOrchestrator::UnbindTurnManagerEvents()
{
	if (TurnManagerRef)
	{
		TurnManagerRef->OnTurnStarted.RemoveDynamic(this, &ACombatOrchestrator::HandleTurnStarted);
		TurnManagerRef->OnCombatEnded.RemoveDynamic(this, &ACombatOrchestrator::HandleCombatEnded);
	}
}

void ACombatOrchestrator::ProcessStartOfTurnEffects(AActor *Actor)
{
	if (!SkillEffectManagerRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] SkillEffectManager not available for start-of-turn processing"));
		return;
	}

	// Delegate to SkillEffectManager - processes buffs, regen, etc.
	SkillEffectManagerRef->ProcessStartOfTurnEffects(Actor);

	// Process status bar decay (buildup lives in its own subsystem post-split).
	if (UStatusBuildupManager *BuildupManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatusBuildupManager>() : nullptr)
	{
		BuildupManager->ProcessStatusBarDecay(Actor);
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Processed start-of-turn effects for %s"),
		   *Actor->GetName());
}

void ACombatOrchestrator::ProcessEndOfTurnEffects(AActor *Actor)
{
	if (!SkillEffectManagerRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] SkillEffectManager not available for end-of-turn processing"));
		return;
	}

	// Delegate to SkillEffectManager - processes DOTs, ticks durations, expires effects
	SkillEffectManagerRef->ProcessEndOfTurnEffects(Actor);

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Processed end-of-turn effects for %s"),
		   *Actor->GetName());
}

void ACombatOrchestrator::RequestActionFromActor(AActor *Actor)
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] RequestActionFromActor called with null actor"));
		return;
	}

	// === AI-CONTROLLED ACTOR ===
	if (IsActorAIControlled(Actor))
	{
		if (!AIDecisionManagerRef)
		{
			UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] %s is AI-controlled but no AIDecisionManager - skipping turn"),
				   *Actor->GetName());
			// Don't broadcast player UI events for an AI actor with no AI brain.
			// Don't auto-advance either; this is a configuration error worth seeing.
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Routing %s to AI decision"), *Actor->GetName());
		GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
		AIDecisionManagerRef->RequestDecision(Actor);
		return;
	}

	// === PLAYER-CONTROLLED ACTOR ===
	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Broadcasting OnActionRequested for player actor: %s"),
		   *Actor->GetName());
	OnActionRequested.Broadcast(Actor);

	// Auto-advance is a debug fallback for when no UI is wired.
	// Once the menu is bound to OnActionRequested, this should be off.
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
	auto PrepareActor = [this](AActor *Actor)
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

				// Apply FSkillEffect-authored bonuses for evolution crystal + equipment.
				if (SkillEffectManagerRef)
				{
					UEvolutionItemData *EvoCrystal = Loadout->GetActivePrimaryEvolutionCrystal(Actor);
					if (EvoCrystal && EvoCrystal->Effects.Num() > 0)
					{
						SkillEffectManagerRef->ApplyEvolutionEffects(
							Actor, EvoCrystal->GetName(), static_cast<int32>(EvoCrystal->GetUniqueID()), EvoCrystal->Effects);
						UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Applied %d evolution effects from %s to %s"),
							   EvoCrystal->Effects.Num(), *EvoCrystal->GetName(), *Actor->GetName());
					}

					TArray<FSkillEffect> EquipmentEffects = Loadout->GetActiveEffects(Actor);
					if (EquipmentEffects.Num() > 0)
					{
						int32 SourceID = static_cast<int32>(Actor->GetUniqueID());
						SkillEffectManagerRef->ApplyEquipmentEffects(Actor, EquipmentEffects, SourceID);
						UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Applied %d equipment effects to %s"),
							   EquipmentEffects.Num(), *Actor->GetName());
					}
				}
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

	// Pre-compute the source-side scalars the overload tick needs. StatusMultiplier
	// folds in BOTH the base-stat layer (innate+equip+stone, shared getter) AND the
	// transient StatusMultiplierBuff/Debuff layer — so a transient buff burns BD faster /
	// a debuff slower, matching base-stat. The self-status pass skips BOTH (bSkipBaseStatAmp)
	// to avoid double-counting, since both are baked here.
	float StatusMultiplierBonus = 1.0f;
	float EfficiencyMult = 1.0f;

	UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (CharComp && CharComp->CharacterData)
	{
		// Crystal-aware StatusMultiplier — shared base-stat getter (innate Spirit×points +
		// equipment BonusStatusMultiplier + attached StatusStone) COMPOUNDED with the
		// transient buff/debuff (×(1+(buff−debuff)/100) + max(0) floor, same form as
		// AddStatusBuildup Step 5b). Both scale the BD drain AND self-status.
		if (UStatusBuildupManager *SBM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatusBuildupManager>() : nullptr)
		{
			float StatusFactor = SBM->GetSourceStatusMultiplierFactor(Actor);
			if (SkillEffectManagerRef)
			{
				const float SmBuff = SkillEffectManagerRef->GetTotalStatModifier(Actor, ESkillEffectType::StatusMultiplierBuff);
				const float SmDebuff = SkillEffectManagerRef->GetTotalStatModifier(Actor, ESkillEffectType::StatusMultiplierDebuff);
				StatusFactor *= FMath::Max(0.0f, 1.0f + (SmBuff - SmDebuff) / CombatConstants::STAT_PERCENT_DIVISOR);
			}
			StatusMultiplierBonus = StatusFactor;
		}

		// EfficiencyMultiplier — unified getter (innate crystal-aware Mind + equipment
		// BonusEfficiency + attached EfficiencyStone, one clamp). Byte-neutral vs the
		// canonical getter for characters with no BonusEfficiency and no stone; equipment
		// and the stone now reach the BD drain through it. Fraction-of-cost in
		// [1 - EFFICIENCY_MAX, 1.0]; lower = better efficiency = smaller leak.
		EfficiencyMult = CharComp->GetEffectiveEfficiencyMultiplier();
	}

	// Process the overflow tick: aura HP damage (SpellDamage-scaled), self HP
	// damage (same), and the coupled energy leak (released = BaseEnergyRelease
	// × StatusMultiplier × Efficiency → drains absorption + becomes self-status
	// in alignment element).
	BDManager->ProcessOverloadTick(ActorsInRange, StatusMultiplierBonus, EfficiencyMult);

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] BD Overflow processed for %s - Range: %.1f, Targets: %d"),
		   *Actor->GetName(), AuraRange, ActorsInRange.Num());
}

TArray<AActor *> UTurnManager::GetTeamMembers(int32 TeamIndex) const
{
	TArray<AActor *> TeamMembers;

	for (const FCombatantTurnDebt &Combatant : Combatants)
	{
		if (Combatant.TeamIndex == TeamIndex && Combatant.Actor)
		{
			TeamMembers.Add(Combatant.Actor);
		}
	}

	return TeamMembers;
}

int32 UTurnManager::GetActorTeam(AActor *Actor) const
{
	if (!Actor)
		return -1;

	for (const FCombatantTurnDebt &Combatant : Combatants)
	{
		if (Combatant.Actor == Actor)
		{
			return Combatant.TeamIndex;
		}
	}

	return -1; // Not found
}

// ============================================================
// CRYSTAL DURABILITY — between-combat auto-repair
// ============================================================

// ============================================================
// CRYSTAL DURABILITY — between-combat destruction (Phase B)
// ============================================================

void ACombatOrchestrator::ApplyBetweenCombatCrystalDestruction()
{
	int32 CrystalsDestroyed = 0;

	auto DestroyTeam = [&](const TArray<AActor *> &Team)
	{
		for (AActor *Actor : Team)
		{
			if (!Actor)
			{
				continue;
			}

			ULoadoutComponent *LoadoutComp = Actor->FindComponentByClass<ULoadoutComponent>();
			if (!LoadoutComp)
			{
				continue;
			}

			for (const FEquippedCrystalSlot &Slot : LoadoutComp->GetEquippedCrystals())
			{
				if (Slot.Kind == EAttachedItemKind::None)
				{
					continue;
				}

				// Resolve per-instance attachment; IsBroken() filters refined-
				// + durability <= 0 (evolution items default to bCanBreak=false
				// so they always report not-broken), matching the old triple-check.
				// On no match GetCrystalEntryByHolder returns an empty attachment
				// whose IsBroken() is false, so the gate below correctly skips
				// unmatched holders.
				const FRuntimeAttachedItem Attachment = LoadoutComp->GetCrystalEntryByHolder(Slot.Holder);
				if (!Attachment.IsBroken())
				{
					continue;
				}

				// Branch-aware display name. Refined crystals have no asset
				// pointer; resolve via ItemIdentity.
				FString CrystalName;
				switch (Slot.Kind)
				{
				case EAttachedItemKind::Crystal:
					CrystalName = ItemIdentity::GetDisplayName(Slot.CrystalId);
					break;
				case EAttachedItemKind::Evolution:
					CrystalName = Slot.Item ? Slot.Item->GetFullItemName() : TEXT("(null)");
					break;
				default:
					CrystalName = TEXT("(none)");
					break;
				}

				FString HolderDesc;
				bool bCleared = false;

				if (UWeaponData *Weapon = Cast<UWeaponData>(Slot.Holder))
				{
					HolderDesc = FString::Printf(TEXT("Weapon '%s'"), *Weapon->Name);
					LoadoutComp->ResetCrystalEntryByHolder(Slot.Holder);
					bCleared = true;
				}
				else if (URingData *Ring = Cast<URingData>(Slot.Holder))
				{
					HolderDesc = FString::Printf(TEXT("Ring '%s'"), *Ring->Name);
					LoadoutComp->ResetCrystalEntryByHolder(Slot.Holder);
					bCleared = true;
				}
				// Evolution self-holder filtered above — GetCrystalEntryByHolder returns
				// an empty entry for UEvolutionItemData* holders, tripping the IsBroken gate.

				if (bCleared)
				{
					UE_LOG(LogTemp, Log,
						   TEXT("[CombatOrchestrator] Destroyed broken crystal '%s' from %s on %s"),
						   *CrystalName, *HolderDesc, *Actor->GetName());
					CrystalsDestroyed++;
				}
			}

			// Case-B: standalone primary-slot evolution (e.g. Broken Darkness).
			// GetEquippedCrystals above does not surface this slot — evolution
			// self-holders aren't matched by FindAttachedItemByHolder — so the
			// loop misses broken primary evolutions. Helper is a no-op unless
			// the slot is Evolution AND its attachment IsBroken().
			if (LoadoutComp->ClearBrokenPrimaryEvolution())
			{
				UE_LOG(LogTemp, Log,
					   TEXT("[CombatOrchestrator] Destroyed broken primary evolution on %s"),
					   *Actor->GetName());
				CrystalsDestroyed++;
			}
		}
	};

	DestroyTeam(Team0Combatants);
	DestroyTeam(Team1Combatants);

	if (CrystalsDestroyed > 0)
	{
		UE_LOG(LogTemp, Log,
			   TEXT("[CombatOrchestrator] Combat end: destroyed %d broken crystal(s)"),
			   CrystalsDestroyed);
	}
}

void ACombatOrchestrator::ApplyBetweenCombatRepair()
{
	const int32 RepairAmount = DurabilityConstants::REPAIR_PER_BATTLE;
	int32 CrystalsRepaired = 0;

	auto RepairTeam = [&](const TArray<AActor *> &Team)
	{
		for (AActor *Actor : Team)
		{
			if (!Actor)
			{
				continue;
			}

			ULoadoutComponent *LoadoutComp = Actor->FindComponentByClass<ULoadoutComponent>();
			if (!LoadoutComp)
			{
				continue;
			}

			for (const FEquippedCrystalSlot &Slot : LoadoutComp->GetEquippedCrystals())
			{
				// Only refined attachments are eligible for between-combat repair.
				// Kind == Refined captures the eligibility gate. Under the
				// bCanBreak opt-in model, a refined crystal may or may not be
				// breakable; repair still routes through the refined branch.
				if (Slot.Kind != EAttachedItemKind::Crystal)
				{
					continue;
				}

				// Read first to gate on IsBroken and capture the pre-repair value.
				// On no match GetCrystalEntryByHolder returns an empty attachment;
				// IsBroken is false for that case, but the Kind == Refined gate
				// above already filtered out the unmatched-holder rows.
				const FRuntimeAttachedItem Before = LoadoutComp->GetCrystalEntryByHolder(Slot.Holder);
				if (Before.IsEmpty() || Before.IsBroken())
				{
					continue;
				}

				const int32 BeforeDur = Before.GetCurrentDurability();
				const int32 NewDur = LoadoutComp->RepairCrystalEntryByHolder(Slot.Holder, RepairAmount);

				if (NewDur > BeforeDur)
				{
					const int32 MaxDur = Before.GetMaxDurability();
					UE_LOG(LogTemp, Verbose,
						   TEXT("[CombatOrchestrator] Repaired '%s' on %s: %d -> %d / %d"),
						   *ItemIdentity::GetDisplayName(Slot.CrystalId), *Actor->GetName(),
						   BeforeDur, NewDur, MaxDur);
					CrystalsRepaired++;
				}
			}
		}
	};

	RepairTeam(Team0Combatants);
	RepairTeam(Team1Combatants);

	if (CrystalsRepaired > 0)
	{
		UE_LOG(LogTemp, Log,
			   TEXT("[CombatOrchestrator] Between-combat repair: +%d to %d crystal(s)"),
			   RepairAmount, CrystalsRepaired);
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
void ACombatOrchestrator::DebugDamageTeam0()
{
	for (AActor *Actor : Team0Combatants)
	{
		if (UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>())
			Comp->ServerTakeDamage(DebugDamageAmount);
	}
}

void ACombatOrchestrator::DebugDamageTeam1()
{
	for (AActor *Actor : Team1Combatants)
	{
		if (UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>())
			Comp->ServerTakeDamage(DebugDamageAmount);
	}
}

void ACombatOrchestrator::DebugSpendEPTeam0()
{
	for (AActor *Actor : Team0Combatants)
	{
		if (UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>())
			Comp->ServerSpendEnergy(DebugDamageAmount);
	}
}

void ACombatOrchestrator::DebugSpendEPTeam1()
{
	for (AActor *Actor : Team1Combatants)
	{
		if (UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>())
			Comp->ServerSpendEnergy(DebugDamageAmount);
	}
}

void ACombatOrchestrator::DebugApplyStatusBuildup()
{
	UStatusBuildupManager *BuildupManager = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
	if (!BuildupManager)
		return;

	// Session Y: manager resolves trigger from (Element, PhysicalType).
	// Fire + None → DOT (same trigger this debug menu used pre-Y).
	for (AActor *Actor : Team0Combatants)
		BuildupManager->AddStatusBuildup(nullptr, Actor, DebugStatusBuildupAmount, ESpellElement::Fire, EPhysicalDamageType::None);

	for (AActor *Actor : Team1Combatants)
		BuildupManager->AddStatusBuildup(nullptr, Actor, DebugStatusBuildupAmount, ESpellElement::Fire, EPhysicalDamageType::None);
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

	// Get weapon attack data via GetActiveWeapon (respects bShowPrimary)
	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		UWeaponData *ActiveWeapon = Loadout->GetActiveWeapon();
		if (ActiveWeapon && ActiveWeapon->WeaponAttack)
		{
			AttackAction.AttackData = ActiveWeapon->WeaponAttack;
			UE_LOG(LogTemp, Log, TEXT("[DebugTestAttackMovement] Using attack: %s from weapon: %s"),
				   *ActiveWeapon->WeaponAttack->Name, *ActiveWeapon->Name);
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
				   *AbilityAction.AbilityData->Name);
		}
	}

	// Fallback to test ability
	if (!AbilityAction.AbilityData)
	{
		AbilityAction.AbilityData = LoadObject<UAbilityData>(nullptr,
															 TEXT("/Game/Testing/Weapons/Abilities/DA_Test_Ability.DA_Test_Ability"));
	}

	if (!AbilityAction.AbilityData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestAbilityMovement] No ability available on %s"),
			   *Actor->GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DebugTestAbilityMovement] %s using %s on %s"),
		   *Actor->GetName(), *AbilityAction.AbilityData->Name, *Target->GetName());

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

	// Get spell from active slot (respects bShowPrimary)
	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		TArray<USpellData *> ActiveSpells = Loadout->GetActiveSlotSpells();
		if (ActiveSpells.Num() > 0)
		{
			SpellAction.SpellData = ActiveSpells[0];
			UE_LOG(LogTemp, Log, TEXT("[DebugTestSpellMovement] Using spell: %s (Element: %s)"),
				   *SpellAction.SpellData->Name,
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
					   *SpellAction.SpellData->Name);
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
		   *Actor->GetName(), *SpellAction.SpellData->Name, *Target->GetName());

	// Execute through ActionExecutor with callback
	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, SpellAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugTestItemOnEnemy()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnEnemy] No actor available"));
		return;
	}

	// Find enemy target
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
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnEnemy] No valid target"));
		return;
	}

	// Get loadout and find damage item (Garnet)
	ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();
	if (!Loadout)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnEnemy] No LoadoutComponent on %s"), *Actor->GetName());
		return;
	}

	TArray<FItemLoadoutSlot> UsableItems = Loadout->GetUsableItems();
	FCrystalId DamageId;
	bool bFoundDamage = false;

	for (const FItemLoadoutSlot &Slot : UsableItems)
	{
		if (!Slot.IsEmpty() && ItemIdentity::GetItemEffectType(Slot.CrystalId) == EItemEffectType::Damage)
		{
			DamageId = Slot.CrystalId;
			bFoundDamage = true;
			break;
		}
	}

	// Fallback: a literal Garnet F when the loadout has no damage item.
	if (!bFoundDamage)
	{
		DamageId = FCrystalId{ECrystalType::Garnet, EItemTier::F_Tier};
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnEnemy] No damage item in loadout, using fallback Garnet F"));
	}

	// Build item action
	FAction ItemAction;
	ItemAction.ActionType = EActionType::Item;
	ItemAction.ItemData = DamageId;
	ItemAction.Targets.Add(Target);

	UE_LOG(LogTemp, Log, TEXT("[DebugTestItemOnEnemy] %s using %s on enemy %s"),
		   *Actor->GetName(), *ItemIdentity::GetDisplayName(DamageId), *Target->GetName());

	// Execute
	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, ItemAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugTestItemOnSelf()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnSelf] No actor available"));
		return;
	}

	// Get loadout and find healing item (Sapphire)
	ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();
	if (!Loadout)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnSelf] No LoadoutComponent on %s"), *Actor->GetName());
		return;
	}

	TArray<FItemLoadoutSlot> UsableItems = Loadout->GetUsableItems();
	FCrystalId HealId;
	bool bFoundHeal = false;

	for (const FItemLoadoutSlot &Slot : UsableItems)
	{
		if (!Slot.IsEmpty() && ItemIdentity::GetItemEffectType(Slot.CrystalId) == EItemEffectType::Healing)
		{
			HealId = Slot.CrystalId;
			bFoundHeal = true;
			break;
		}
	}

	// Fallback: a literal Sapphire F when the loadout has no healing item.
	if (!bFoundHeal)
	{
		HealId = FCrystalId{ECrystalType::Sapphire, EItemTier::F_Tier};
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnSelf] No healing item in loadout, using fallback Sapphire F"));
	}

	// Build item action - target is self
	FAction ItemAction;
	ItemAction.ActionType = EActionType::Item;
	ItemAction.ItemData = HealId;
	ItemAction.Targets.Add(Actor); // Self-target

	UE_LOG(LogTemp, Log, TEXT("[DebugTestItemOnSelf] %s using %s on SELF"),
		   *Actor->GetName(), *ItemIdentity::GetDisplayName(HealId));

	// Execute
	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, ItemAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugTestItemOnAlly()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnAlly] No actor available"));
		return;
	}

	// Find ally target (same team, different actor)
	AActor *Ally = nullptr;
	bool bActorInTeam0 = Team0Combatants.Contains(Actor);
	TArray<AActor *> &TeamArray = bActorInTeam0 ? Team0Combatants : Team1Combatants;

	for (AActor *Teammate : TeamArray)
	{
		if (Teammate != Actor && IsActorAlive(Teammate))
		{
			Ally = Teammate;
			break;
		}
	}

	if (!Ally)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnAlly] No ally available (need 2+ team members)"));
		return;
	}

	// Get loadout and find healing item (Sapphire)
	ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();
	if (!Loadout)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnAlly] No LoadoutComponent on %s"), *Actor->GetName());
		return;
	}

	TArray<FItemLoadoutSlot> UsableItems = Loadout->GetUsableItems();
	FCrystalId HealId;
	bool bFoundHeal = false;

	for (const FItemLoadoutSlot &Slot : UsableItems)
	{
		if (!Slot.IsEmpty() && ItemIdentity::GetItemEffectType(Slot.CrystalId) == EItemEffectType::Healing)
		{
			HealId = Slot.CrystalId;
			bFoundHeal = true;
			break;
		}
	}

	// Fallback: a literal Sapphire F when the loadout has no healing item.
	if (!bFoundHeal)
	{
		HealId = FCrystalId{ECrystalType::Sapphire, EItemTier::F_Tier};
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnAlly] No healing item in loadout, using fallback Sapphire F"));
	}

	// Build item action - target is ally
	FAction ItemAction;
	ItemAction.ActionType = EActionType::Item;
	ItemAction.ItemData = HealId;
	ItemAction.Targets.Add(Ally);

	UE_LOG(LogTemp, Log, TEXT("[DebugTestItemOnAlly] %s using %s on ally %s"),
		   *Actor->GetName(), *ItemIdentity::GetDisplayName(HealId), *Ally->GetName());

	// Execute
	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, ItemAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugExecuteAsyncAttack()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteAsyncAttack: No actor available"));
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
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteAsyncAttack: No valid target"));
		return;
	}

	// Get weapon attack data via GetActiveWeapon (respects bShowPrimary)
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
		UE_LOG(LogTemp, Warning, TEXT("[DebugExecuteAsyncAttack] No weapon attack data on %s"),
			   *Actor->GetName());
		return;
	}

	// Build action
	FAction AttackAction;
	AttackAction.ActionType = EActionType::Attack;
	AttackAction.AttackData = AttackData;
	AttackAction.Targets.Add(Target);

	UE_LOG(LogTemp, Log, TEXT("[DebugExecuteAsyncAttack] %s attacking %s with %s"),
		   *Actor->GetName(), *Target->GetName(), *AttackData->Name);

	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, AttackAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugExecuteAsyncSpell()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteAsyncSpell: No actor available"));
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
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteAsyncSpell: No valid target"));
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
		UE_LOG(LogTemp, Warning, TEXT("[DebugExecuteAsyncSpell] No spell available"));
		return;
	}

	FAction SpellAction;
	SpellAction.ActionType = EActionType::Spell;
	SpellAction.SpellData = SpellData;
	SpellAction.Targets.Add(Target);

	UE_LOG(LogTemp, Log, TEXT("[DebugExecuteAsyncSpell] %s casting %s on %s"),
		   *Actor->GetName(), *SpellData->Name, *Target->GetName());

	UE_LOG(LogTemp, Warning, TEXT("[DEBUG] Spell: %s | TargetType: %d | DeliveryType: %d | CastAnim: %s"),
		   *SpellData->Name,
		   (int32)SpellData->TargetType,
		   (int32)SpellData->DeliveryType,
		   SpellData->CastAnimation ? *SpellData->CastAnimation->GetName() : TEXT("NONE"));

	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, SpellAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
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
				   *SpellAction.SpellData->Name,
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
				   *SpellAction.SpellData->Name,
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

void ACombatOrchestrator::DebugExecuteAsyncAbility()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteAsyncAbility: No actor available"));
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
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteAsyncAbility: No valid target"));
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
											   TEXT("/Game/Testing/Weapons/Abilities/DA_Test_Ability.DA_Test_Ability"));
	}

	if (!AbilityData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugExecuteAsyncAbility] No ability available"));
		return;
	}

	FAction AbilityAction;
	AbilityAction.ActionType = EActionType::Ability;
	AbilityAction.AbilityData = AbilityData;
	AbilityAction.Targets.Add(Target);

	UE_LOG(LogTemp, Log, TEXT("[DebugExecuteAsyncAbility] %s using %s on %s"),
		   *Actor->GetName(), *AbilityData->Name, *Target->GetName());

	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, AbilityAction,
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

		ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();

		if (Loadout)
		{
			// Transfer model: no end-of-combat consume step — the crystal
			// inventory was debited at equip time and item-slot Quantity is the
			// persistent source of truth. Only reset the battle-ready flag.
			Loadout->ResetBattleState();
			UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Reset loadout battle state for %s"), *Actor->GetName());
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

#include "Combat/Camera/CombatCameraManager.h"

ACombatCameraManager *ACombatOrchestrator::FindCameraManager()
{
	if (!CameraManager)
	{
		TArray<AActor *> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACombatCameraManager::StaticClass(), FoundActors);
		if (FoundActors.Num() > 0)
		{
			CameraManager = Cast<ACombatCameraManager>(FoundActors[0]);
		}
	}
	return CameraManager;
}

void ACombatOrchestrator::DebugManualAdvanceTurn()
{
	if (!TurnManagerRef || CombatState != ECombatState::InProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Cannot advance - combat not in progress"));
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] DEBUG: Manual turn advance"));
	TurnManagerRef->AdvanceToNextTurn();
}

void ACombatOrchestrator::DebugToggleAutoAdvance()
{
	bAutoAdvanceTurns = !bAutoAdvanceTurns;

	if (!bAutoAdvanceTurns)
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Auto-advance: %s"),
		   bAutoAdvanceTurns ? TEXT("ON") : TEXT("OFF"));
}

void ACombatOrchestrator::DebugSelectTarget(int32 EnemyIndex)
{
	if (!CurrentActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] No current actor"));
		return;
	}

	int32 CurrentTeam = Team0Combatants.Contains(CurrentActor) ? 0 : 1;
	TArray<AActor *> &EnemyTeam = (CurrentTeam == 0) ? Team1Combatants : Team0Combatants;

	if (EnemyIndex < 0 || EnemyIndex >= EnemyTeam.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Invalid enemy index %d (team has %d)"),
			   EnemyIndex, EnemyTeam.Num());
		return;
	}

	DebugSelectedTargetIndex = EnemyIndex;
	AActor *Target = EnemyTeam[EnemyIndex];

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Selected target: %s (index %d)"),
		   *Target->GetName(), EnemyIndex);

	if (CameraManager)
	{
		CameraManager->TransitionToSelection(Target);
	}
}

void ACombatOrchestrator::DebugAttackSelectedTarget()
{
	if (!CurrentActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] No current actor"));
		return;
	}

	int32 CurrentTeam = Team0Combatants.Contains(CurrentActor) ? 0 : 1;
	TArray<AActor *> &EnemyTeam = (CurrentTeam == 0) ? Team1Combatants : Team0Combatants;

	if (DebugSelectedTargetIndex >= EnemyTeam.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Invalid target index"));
		return;
	}

	AActor *Target = EnemyTeam[DebugSelectedTargetIndex];

	if (CameraManager)
	{
		CameraManager->TransitionToAction(CurrentActor, Target);
	}

	FAction AttackAction;
	AttackAction.ActionType = EActionType::Attack;
	AttackAction.Targets.Add(Target);

	// Get weapon attack from loadout
	ULoadoutComponent *Loadout = CurrentActor->FindComponentByClass<ULoadoutComponent>();
	if (Loadout)
	{
		TArray<UWeaponAttackData *> Attacks = Loadout->GetAllWeaponAttacks();
		if (Attacks.Num() > 0)
		{
			AttackAction.AttackData = Attacks[0];
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] DEBUG: Attacking %s"), *Target->GetName());

	SubmitActionAsync(AttackAction);
}

// ========================================
// CRYSTAL WEAR DEBUG BUTTONS
// ========================================

void ACombatOrchestrator::DebugCrystalState()
{
	if (UCrystalManager *CM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCrystalManager>() : nullptr)
	{
		CM->WOR_CrystalState();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugCrystalState: No CrystalManager subsystem."));
	}
}

void ACombatOrchestrator::DebugWearTable()
{
	if (UCrystalManager *CM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCrystalManager>() : nullptr)
	{
		CM->WOR_WearTable();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugWearTable: No CrystalManager subsystem."));
	}
}

void ACombatOrchestrator::DebugSimCast_S_L2()
{
	if (UCrystalManager *CM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCrystalManager>() : nullptr)
	{
		CM->WOR_SimCast(static_cast<int32>(EItemTier::S_Tier), 2);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_S_L2: No CrystalManager subsystem."));
	}
}

void ACombatOrchestrator::DebugSimCast_Matched_L1()
{
	if (!TurnManagerRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_Matched_L1: No TurnManager."));
		return;
	}

	AActor *Actor = TurnManagerRef->GetCurrentActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_Matched_L1: No active combatant (combat not active?)."));
		return;
	}

	ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();
	if (!Loadout)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_Matched_L1: No LoadoutComponent on %s."), *Actor->GetName());
		return;
	}

	UWeaponData *Weapon = Loadout->GetPrimaryWeapon();
	if (!Weapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_Matched_L1: %s has no primary weapon equipped."), *Actor->GetName());
		return;
	}

	FRuntimeAttachedItem *Attachment = Loadout->FindAttachedItemByHolder(Weapon);
	if (!Attachment || Attachment->IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_Matched_L1: %s's primary weapon has no attached crystal."), *Actor->GetName());
		return;
	}

	if (!Attachment->IsCrystal())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_Matched_L1: %s's crystal is not refined (no tier to match)."), *Actor->GetName());
		return;
	}

	const int32 CrystalTier = static_cast<int32>(Attachment->Crystal.Id.Tier);

	if (UCrystalManager *CM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCrystalManager>() : nullptr)
	{
		CM->WOR_SimCast(CrystalTier, 1);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_Matched_L1: No CrystalManager subsystem."));
	}
}