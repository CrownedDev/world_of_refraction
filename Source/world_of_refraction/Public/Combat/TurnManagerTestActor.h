// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TurnManagerTestActor.generated.h"

class UTurnManager;

/**
 * TurnManagerTestActor
 * Automated test suite for TurnManager system
 *
 * IMPORTANT: Tests require PIE (Play-in-Editor) context!
 * - GameInstanceSubsystems only exist during gameplay
 * - CallInEditor button will show error message with instructions
 * - Wire BeginPlay -> RunTest in Blueprint for automatic testing
 */
UCLASS()
class WORLD_OF_REFRACTION_API ATurnManagerTestActor : public AActor
{
	GENERATED_BODY()

public:
	ATurnManagerTestActor();

	virtual void BeginPlay() override;

	// ========================================
	// TEST EXECUTION
	// ========================================

	/** Run all tests - requires PIE context */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Testing")
	void RunTest();

	// ========================================
	// INDIVIDUAL TESTS
	// ========================================

	/** Test: Basic 3v3 combat initialization */
	UFUNCTION(BlueprintCallable, Category = "Testing")
	void Test_Basic3v3();

	/** Test: Speed ratio (2:1 fast vs slow) */
	UFUNCTION(BlueprintCallable, Category = "Testing")
	void Test_SpeedRatio();

	/** Test: Tie-breaking between equal-speed characters */
	UFUNCTION(BlueprintCallable, Category = "Testing")
	void Test_TieBreaking();

	/** Test: Dynamic speed changes (buffs/debuffs) */
	UFUNCTION(BlueprintCallable, Category = "Testing")
	void Test_SpeedChanges();

	/** Test: Death skips turns, resurrection rejoins */
	UFUNCTION(BlueprintCallable, Category = "Testing")
	void Test_DeathResurrection();

	// ========================================
	// TEST RESULTS
	// ========================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Testing")
	int32 TestsPassed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Testing")
	int32 TestsFailed = 0;

private:
	// ========================================
	// TEST HELPERS
	// ========================================

	/**
	 * Get TurnManager with null safety
	 * Handles both GameInstance and Subsystem null checks
	 * Returns nullptr and logs error if unavailable (e.g., outside PIE)
	 */
	UTurnManager* GetTurnManagerSafe();

	/** Create test character with specific stats */
	AActor* CreateTestCharacter(FString Name, int32 Mind, int32 Body, int32 Spirit,
		int32 TurnSpeed, int32 AttackSpeed);

	/** Clean up test actors after test */
	void CleanupTestActors(TArray<AActor*>& Actors);

	/** Print test result */
	void PrintTestResult(FString TestName, bool bPassed);
};