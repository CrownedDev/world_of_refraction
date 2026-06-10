// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillEffectManagerTestActor.generated.h"

class USkillEffectManager;
class UCharacterDataComponent;

/**
 * ASkillEffectManagerTestActor
 *
 * Automated test suite for SkillEffectManager.
 * Place in level and use context menu to run tests.
 *
 * Test Coverage:
 * - Effect application (new, stack, refresh)
 * - Effect removal (by ID, type, category)
 * - Turn processing (start, end, duration tick)
 * - Conditional triggers (HP threshold, events)
 * - Stacking rules (max stacks, refresh behavior)
 * - Query functions (counts, has effect, stat totals)
 */
UCLASS()
class WORLD_OF_REFRACTION_API ASkillEffectManagerTestActor : public AActor
{
	GENERATED_BODY()

public:
	ASkillEffectManagerTestActor();

	virtual void BeginPlay() override;

	// ========================================
	// TEST EXECUTION
	// ========================================

	/** Run all tests */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests")
	void RunAllTests();

	/** Auto-run tests on BeginPlay */
	UPROPERTY(EditAnywhere, Category = "Skill Effect Tests")
	bool bAutoRunTests = false;

	// ========================================
	// INDIVIDUAL TESTS
	// ========================================

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_ApplyEffect();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_StackingBehavior();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_DurationRefresh();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_RemoveByID();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_RemoveByType();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_RemoveAllBuffs();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_RemoveAllDebuffs();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_StartOfTurnProcessing();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_EndOfTurnProcessing();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_DOTDamage();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_DurationExpiration();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_ConditionalTrigger();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_PermanentEffects();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_StatModifierQuery();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_SourceTracking();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_PhysicalDamageEffects();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Skill Effect Tests|Individual")
	void Test_SpeedBuffTurnManagerNotification();

private:
	// ========================================
	// TEST INFRASTRUCTURE
	// ========================================

	/** Get or create SkillEffectManager */
	USkillEffectManager *GetSkillEffectManager();

	/** Create a test actor with CharacterDataComponent */
	AActor *CreateTestActor(const FString &Name, int32 HP = 100, int32 EP = 50);

	/** Cleanup test actors */
	void CleanupTestActors();

	/** Test result tracking */
	int32 TestsPassed = 0;
	int32 TestsFailed = 0;

	/** Unique counter for actor names */
	int32 TestActorCounter = 0;

	/** Spawned test actors for cleanup */
	UPROPERTY()
	TArray<AActor *> SpawnedTestActors;

	/** Log test result */
	void LogTestResult(const FString &TestName, bool bPassed, const FString &Details = TEXT(""));

	/** Assert helper */
	bool AssertTrue(bool Condition, const FString &Message);
	bool AssertEqual(int32 Expected, int32 Actual, const FString &Message);
	bool AssertEqualFloat(float Expected, float Actual, const FString &Message, float Tolerance = 0.01f);
};