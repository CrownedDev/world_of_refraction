// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ActionStructs.h"
#include "BrokenDarknessManager.h"
#include "EAIDifficulty.h"
#include "AIDecisionManager.h"
#include "CombatOrchestrator.generated.h"

class UTurnManager;
class UCharacterDataComponent;
class UStatusEffectManager;
class UActionExecutor;

/**
 * Combat state enum
 */
UENUM(BlueprintType)
enum class ECombatState : uint8
{
	Idle,		  // No combat active
	Initializing, // Setting up combatants
	InProgress,	  // Combat active, processing turns
	Victory,	  // Player team won
	Defeat,		  // Enemy team won
	Draw		  // Both teams eliminated (edge case)
};

/**
 * Result of a completed combat
 */
USTRUCT(BlueprintType)
struct FCombatResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ECombatState FinalState = ECombatState::Idle;

	UPROPERTY(BlueprintReadOnly)
	int32 TotalTurns = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Team0Survivors = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Team1Survivors = 0;

	UPROPERTY(BlueprintReadOnly)
	AActor *LastActorStanding = nullptr;
};

/**
 * Delegate signatures
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, ECombatState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatResultReady, const FCombatResult &, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActorTurnStarted, AActor *, Actor, int32, TurnNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionRequested, AActor *, Actor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActionExecuted, AActor *, Actor, const FActionResult &, Result);

/**
 * CombatOrchestrator - Coordinates all combat subsystems
 *
 * Responsibilities:
 * - Initialize/end combat via TurnManager
 * - Listen to turn events and coordinate responses
 * - Process status effects at turn boundaries (via StatusEffectManager)
 * - Accept and execute actions from UI/AI (via ActionExecutor)
 * - Check win conditions
 *
 * Integrated Systems:
 * - TurnManager (turn order, speed changes)
 * - StatusEffectManager (start/end of turn processing)
 * - ActionExecutor (validate and execute actions)
 *
 * Future integrations:
 * - BattleUIManager (show action menu for players)
 * - AIDecisionManager (make decisions for AI)
 */
UCLASS()
class WORLD_OF_REFRACTION_API ACombatOrchestrator : public AActor
{
	GENERATED_BODY()

public:
	ACombatOrchestrator();

	// ========================================
	// COMBAT CONTROL
	// ========================================

	/** Start combat between two teams. Team 0 = players, Team 1 = enemies (by convention) */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void StartCombat(const TArray<AActor *> &Team0, const TArray<AActor *> &Team1, EAIDifficulty Difficulty = EAIDifficulty::Medium);

	/** Force end combat (e.g., flee, cutscene interrupt) */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ForceEndCombat(ECombatState ForcedState = ECombatState::Idle);

	// ========================================
	// ACTION SUBMISSION
	// ========================================

	/**
	 * Submit an action for the current actor (synchronous)
	 * Validates, executes via ActionExecutor, then ends turn
	 * @param Action The action to execute
	 * @return True if action was valid and executed
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	bool SubmitAction(const FAction &Action);

	/**
	 * Submit action asynchronously (for defense windows, animations)
	 * Turn ends when async execution completes
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	void SubmitActionAsync(const FAction &Action);

	/**
	 * Validate an action without executing
	 * @param Action The action to validate
	 * @return Validation result with error message if invalid
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	FActionValidationResult ValidateAction(const FAction &Action) const;

	/** Called when current actor's action completes (internal - advances turn) */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void OnActionCompleted();

	// ========================================
	// QUERY
	// ========================================

	UFUNCTION(BlueprintPure, Category = "Combat")
	ECombatState GetCombatState() const { return CombatState; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	AActor *GetCurrentActor() const { return CurrentActor; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetCurrentTurnNumber() const { return CurrentTurnNumber; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	const TArray<AActor *> &GetTeam0() const { return Team0Combatants; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	const TArray<AActor *> &GetTeam1() const { return Team1Combatants; }

	/** Check if it's currently this actor's turn */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsActorsTurn(AActor *Actor) const { return CurrentActor == Actor && CombatState == ECombatState::InProgress; }

	/** Check if actor is AI-controlled */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsActorAIControlled(AActor *Actor) const;

	/** Get living enemies for an actor */
	UFUNCTION(BlueprintPure, Category = "Combat|Targeting")
	TArray<AActor *> GetLivingEnemies(AActor *ForActor) const;

	/** Get living allies for an actor */
	UFUNCTION(BlueprintPure, Category = "Combat|Targeting")
	TArray<AActor *> GetLivingAllies(AActor *ForActor) const;

	/** Get current AI difficulty */
	UFUNCTION(BlueprintPure, Category = "Combat|AI")
	EAIDifficulty GetCombatDifficulty() const { return CombatDifficulty; }

	// ========================================
	// EVENTS
	// ========================================

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnCombatStateChanged OnCombatStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnCombatResultReady OnCombatResultReady;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnActorTurnStarted OnActorTurnStarted;

	/** Broadcast when waiting for action input (UI should show action menu) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnActionRequested OnActionRequested;

	/** Broadcast when action execution completes (for UI feedback) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnActionExecuted OnActionExecuted;

	// ========================================
	// CONFIGURATION
	// ========================================

	/** If true, auto-advances turn after delay (for testing without UI/AI) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	bool bAutoAdvanceTurns = true;

	/** Automatically start combat on BeginPlay using tagged actors */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	bool bAutoStartCombat = false;

	/** Delay before auto-advancing (simulates action time) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug", meta = (EditCondition = "bAutoAdvanceTurns"))
	float AutoAdvanceDelay = 1.0f;

	/** AI difficulty level for this combat */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|AI")
	EAIDifficulty CombatDifficulty = EAIDifficulty::Medium;

	// ========================================
	// DEBUG TOOLS
	// ========================================

	/** Override actor for debug commands (if set, uses this instead of CurrentActor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	AActor *DebugOverrideActor = nullptr;

	/** Helper to get actor for debug commands */
	AActor *GetDebugActor() const;

	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void DebugPrintCombatState();

	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void DebugKillActor(AActor *Actor);

	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void DebugHealAllTeam(int32 TeamIndex);

	/** Execute a test action (basic attack on random enemy) */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug", meta = (CallInEditor = "true"))
	void DebugExecuteTestAction();

	/** Test attack with movement - current actor attacks first enemy */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugTestAttackMovement();

	/** Execute sync attack - bypasses movement, tests damage pipeline + animation */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugExecuteSyncAttack();

	/** Execute sync spell - tests spell pipeline + VFX */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugExecuteSyncSpell();

	/** Execute sync ability - tests ability pipeline */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugExecuteSyncAbility();

	/** Draw grid visualization with actor positions */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug", meta = (CallInEditor = "true"))
	void DebugDrawCombatGrid(float Duration = 5.0f);

	/** Start combat using tagged actors in level and snap to grid positions */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug", meta = (CallInEditor = "true"))
	void DebugStartCombatWithLevelActors();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// ========================================
	// INTERNAL STATE
	// ========================================

	UPROPERTY()
	ECombatState CombatState;

	UPROPERTY()
	TArray<AActor *> Team0Combatants;

	UPROPERTY()
	TArray<AActor *> Team1Combatants;

	UPROPERTY()
	AActor *CurrentActor;

	UPROPERTY()
	int32 CurrentTurnNumber;

	UPROPERTY()
	UTurnManager *TurnManagerRef;

	UPROPERTY()
	UStatusEffectManager *StatusEffectManagerRef;

	UPROPERTY()
	UActionExecutor *ActionExecutorRef;

	FTimerHandle AutoAdvanceTimerHandle;

	UPROPERTY()
	UAIDecisionManager *AIDecisionManagerRef = nullptr;

	/** Track if we're waiting for async action to complete */
	bool bWaitingForAsyncAction = false;

	// ========================================
	// TURN MANAGER EVENT HANDLERS
	// ========================================

	UFUNCTION()
	void HandleTurnStarted(AActor *Actor, int32 TurnNumber);

	UFUNCTION()
	void HandleTurnEnded(AActor *Actor, int32 TurnNumber);

	UFUNCTION()
	void HandleCombatEnded(int32 FinalTurnCount);

	// ========================================
	// INTERNAL METHODS
	// ========================================

	void SetCombatState(ECombatState NewState);
	/** Prepare all combatants' loadouts for battle */
	void PrepareAllLoadoutsForBattle();

	/** Consume used items from all combatants' inventories after battle */
	void ConsumeAllUsedItems();
	void BindTurnManagerEvents();
	void UnbindTurnManagerEvents();

	// Status effect processing (delegates to StatusEffectManager)
	void ProcessStartOfTurnEffects(AActor *Actor);
	void ProcessEndOfTurnEffects(AActor *Actor);

	// Action request (broadcasts to UI/AI)
	void RequestActionFromActor(AActor *Actor);

	// Async action callback
	void HandleAsyncActionCompleted(const FActionResult &Result);

	// Win condition
	ECombatState CheckWinCondition();
	int32 CountLivingMembers(const TArray<AActor *> &Team);
	bool IsActorAlive(AActor *Actor) const;

	// Team helpers
	int32 GetActorTeamIndex(AActor *Actor) const;
	TArray<AActor *> GetEnemyTeam(AActor *Actor) const;

	// Result building
	FCombatResult BuildCombatResult();

	// ========================================
	// BROKEN DARKNESS HELPERS
	// ========================================

	/** Get BrokenDarknessManager from an actor */
	UBrokenDarknessManager *GetBrokenDarknessManager(AActor *Actor) const;

	/** Get all combatants within range of an actor (excludes the actor itself) */
	TArray<AActor *> GetCombatantsInRange(AActor *Origin, float Range);

	/** Process BD overflow effects for an actor (aura damage, self-damage, drain) */
	void ProcessBrokenDarknessOverflow(AActor *Actor);
};