// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatOrchestrator.generated.h"

class UTurnManager;
class UCharacterDataComponent;

/**
 * Combat state enum
 */
UENUM(BlueprintType)
enum class ECombatState : uint8
{
	Idle,           // No combat active
	Initializing,   // Setting up combatants
	InProgress,     // Combat active, processing turns
	Victory,        // Player team won
	Defeat,         // Enemy team won
	Draw            // Both teams eliminated (edge case)
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
	AActor* LastActorStanding = nullptr;
};

/**
 * Delegate signatures
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, ECombatState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatResultReady, const FCombatResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActorTurnStarted, AActor*, Actor, int32, TurnNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionRequested, AActor*, Actor);

/**
 * CombatOrchestrator - Coordinates all combat subsystems
 *
 * Responsibilities:
 * - Initialize/end combat via TurnManager
 * - Listen to turn events and coordinate responses
 * - Process status effects at turn boundaries (stub)
 * - Request actions from actors (stub - auto-advance for now)
 * - Check win conditions
 *
 * Future integrations:
 * - StatusEffectManager (start/end of turn processing)
 * - ActionExecutor (validate and execute actions)
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
	void StartCombat(const TArray<AActor*>& Team0, const TArray<AActor*>& Team1);

	/** Force end combat (e.g., flee, cutscene interrupt) */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ForceEndCombat(ECombatState ForcedState = ECombatState::Idle);

	/** Called when current actor's action completes (stub: call manually or auto-fires) */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void OnActionCompleted();

	// ========================================
	// QUERY
	// ========================================

	UFUNCTION(BlueprintPure, Category = "Combat")
	ECombatState GetCombatState() const { return CombatState; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	AActor* GetCurrentActor() const { return CurrentActor; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetCurrentTurnNumber() const { return CurrentTurnNumber; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	const TArray<AActor*>& GetTeam0() const { return Team0Combatants; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	const TArray<AActor*>& GetTeam1() const { return Team1Combatants; }

	// ========================================
	// EVENTS
	// ========================================

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnCombatStateChanged OnCombatStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnCombatResultReady OnCombatResultReady;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnActorTurnStarted OnActorTurnStarted;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnActionRequested OnActionRequested;

	// ========================================
	// CONFIGURATION
	// ========================================

	/** If true, auto-advances turn after delay (for testing without UI/AI) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	bool bAutoAdvanceTurns = true;

	/** Delay before auto-advancing (simulates action time) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug", meta = (EditCondition = "bAutoAdvanceTurns"))
	float AutoAdvanceDelay = 1.0f;

	// ========================================
	// DEBUG TOOLS
	// ========================================

	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void DebugPrintCombatState();

	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void DebugKillActor(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void DebugHealAllTeam(int32 TeamIndex);

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
	TArray<AActor*> Team0Combatants;

	UPROPERTY()
	TArray<AActor*> Team1Combatants;

	UPROPERTY()
	AActor* CurrentActor;

	UPROPERTY()
	int32 CurrentTurnNumber;

	UPROPERTY()
	UTurnManager* TurnManagerRef;

	FTimerHandle AutoAdvanceTimerHandle;

	// ========================================
	// TURN MANAGER EVENT HANDLERS
	// ========================================

	UFUNCTION()
	void HandleTurnStarted(AActor* Actor, int32 TurnNumber);

	UFUNCTION()
	void HandleTurnEnded(AActor* Actor, int32 TurnNumber);

	UFUNCTION()
	void HandleCombatEnded(int32 FinalTurnCount);

	// ========================================
	// INTERNAL METHODS
	// ========================================

	void SetCombatState(ECombatState NewState);
	void BindTurnManagerEvents();
	void UnbindTurnManagerEvents();

	// Status effect stubs (future: delegate to StatusEffectManager)
	void ProcessStartOfTurnEffects(AActor* Actor);
	void ProcessEndOfTurnEffects(AActor* Actor);

	// Action request stub (future: delegate to UI/AI managers)
	void RequestActionFromActor(AActor* Actor);

	// Win condition
	ECombatState CheckWinCondition();
	int32 CountLivingMembers(const TArray<AActor*>& Team);
	bool IsActorAlive(AActor* Actor);

	// Result building
	FCombatResult BuildCombatResult();
};