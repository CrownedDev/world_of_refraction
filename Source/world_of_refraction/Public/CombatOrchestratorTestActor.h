// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatOrchestrator.h"
#include "CombatOrchestratorTestActor.generated.h"

/**
 * Test actor for CombatOrchestrator
 * Place in level and click "Run All Tests" in Details panel during PIE
 */
UCLASS()
class WORLD_OF_REFRACTION_API ACombatOrchestratorTestActor : public AActor
{
	GENERATED_BODY()

public:
	ACombatOrchestratorTestActor();

	// ========================================
	// TEST EXECUTION
	// ========================================

	/** Run all tests (requires PIE - press ALT+P first) */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Combat Tests")
	void RunAllTests();

	/** Run single combat test with auto-advance */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Combat Tests")
	void Test_BasicCombatFlow();

	/** Test win condition (Team0 wins) */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Combat Tests")
	void Test_VictoryCondition();

	/** Test defeat condition (Team1 wins) */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Combat Tests")
	void Test_DefeatCondition();

	/** Test combat state transitions */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Combat Tests")
	void Test_StateTransitions();

	/** Test force end combat */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Combat Tests")
	void Test_ForceEndCombat();

	/** Test ActionExecutor integration (SubmitAction) */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Combat Tests")
	void Test_ActionExecutorIntegration();

	/** Test real attack execution with DA_Attack_Bolt */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Combat Tests")
	void Test_RealAttackExecution();

	/** Test spell execution through full pipeline */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Combat Tests")
	void Test_SpellExecution();

	/** Test ability execution through full pipeline */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Combat Tests")
	void Test_AbilityExecution();

	/** Test that actions properly apply status effects */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Combat Tests")
	void Test_StatusEffectFromAction();

	/** Test AOE attacks hitting multiple targets */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Combat Tests")
	void Test_MultiTargetAction();

	/** Test energy is properly deducted from actions */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Combat Tests")
	void Test_EnergyCost();

	// ========================================
	// CONFIGURATION
	// ========================================

	/** Delay between turns during auto-advance testing */
	UPROPERTY(EditAnywhere, Category = "Combat Tests")
	float TestTurnDelay = 0.5f;

	/** Number of turns to run in flow test */
	UPROPERTY(EditAnywhere, Category = "Combat Tests")
	int32 FlowTestTurnCount = 6;

	/** Auto-run tests on BeginPlay (must be in PIE) */
	UPROPERTY(EditAnywhere, Category = "Combat Tests")
	bool bAutoRunTests = false;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	ACombatOrchestrator* TestOrchestrator;

	// Test tracking
	int32 TestsPassed;
	int32 TestsFailed;
	int32 TestActorCounter;

	// Test helpers
	ACombatOrchestrator* GetOrCreateOrchestrator();
	AActor* CreateTestCharacter(const FString& Name, int32 Mind, int32 Body, int32 Spirit, int32 TurnSpeed, int32 TeamIndex);
	void CleanupTestActors(TArray<AActor*>& Actors);
	void PrintTestResult(const FString& TestName, bool bPassed);

	// Event handlers for async tests
	UFUNCTION()
	void OnTestCombatStateChanged(ECombatState NewState);

	UFUNCTION()
	void OnTestCombatResultReady(const FCombatResult& Result);

	UFUNCTION()
	void OnTestActionExecuted(AActor* Actor, const FActionResult& Result);

	// Async test state
	TArray<ECombatState> RecordedStateTransitions;
	FCombatResult LastCombatResult;
	FActionResult LastActionResult;
	bool bWaitingForResult;
	bool bActionExecuted;
};