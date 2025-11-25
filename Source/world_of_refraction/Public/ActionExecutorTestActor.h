// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ActionExecutorTestActor.generated.h"

class UActionExecutor;
class UStatusEffectManager;
class UCharacterDataComponent;
class UCharacterData;
class USpellData;
class UAbilityData;
class UItemData;
class UBaseAttackData;

/**
 * AActionExecutorTestActor
 * 
 * Test actor for validating ActionExecutor functionality.
 * Place in level and use context menu to run tests.
 * 
 * Tests cover:
 * - Action validation (energy, stun, silence)
 * - Spell execution with infusion
 * - Ability execution with infusion
 * - Item execution
 * - Attack execution
 * - Damage application with defense/crit
 * - Multi-hit processing
 * - Status effect application
 */
UCLASS()
class WORLD_OF_REFRACTION_API AActionExecutorTestActor : public AActor
{
	GENERATED_BODY()

public:
	AActionExecutorTestActor();

	// ==================== TEST SUITE ====================

	/** Run all tests */
	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests")
	void RunAllTests();

	// ==================== INDIVIDUAL TESTS ====================

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Validation")
	void Test_ValidationBasic();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Validation")
	void Test_ValidationEnergy();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Validation")
	void Test_ValidationStunned();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Validation")
	void Test_ValidationSilenced();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Execution")
	void Test_ExecuteSpell();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Execution")
	void Test_ExecuteSpellInfused();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Execution")
	void Test_ExecuteAbility();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Execution")
	void Test_ExecuteAbilityInfused();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Execution")
	void Test_ExecuteItem();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Execution")
	void Test_ExecuteAttack();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Execution")
	void Test_ExecuteDefend();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Infusion")
	void Test_InfusionMultipliers();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Damage")
	void Test_DamageApplication();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Damage")
	void Test_CriticalHits();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Damage")
	void Test_DefenseReduction();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Damage")
	void Test_MultiHit();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Effects")
	void Test_StatusEffectApplication();

	UFUNCTION(CallInEditor, Category = "ActionExecutor Tests|Effects")
	void Test_HealingApplication();

private:
	// ==================== TEST HELPERS ====================

	/** Create a test actor with CharacterDataComponent */
	AActor* CreateTestActor(const FString& Name, int32 MaxHP = 100, int32 MaxEP = 100);

	/** Create test spell data */
	USpellData* CreateTestSpell(const FString& Name, int32 Damage, int32 EnergyCost, int32 Hits = 1);

	/** Create test ability data */
	UAbilityData* CreateTestAbility(const FString& Name, int32 Damage, int32 EnergyCost, int32 Hits = 1);

	/** Create test item data */
	UItemData* CreateTestItem(const FString& Name, EItemEffectType EffectType, float Value);

	/** Create test attack data */
	UBaseAttackData* CreateTestAttack(const FString& Name, int32 Damage, int32 Hits = 1);

	/** Get ActionExecutor subsystem */
	UActionExecutor* GetActionExecutor() const;

	/** Get StatusEffectManager subsystem */
	UStatusEffectManager* GetStatusEffectManager() const;

	/** Log test result */
	void LogTestResult(const FString& TestName, bool bPassed, const FString& Details = TEXT(""));

	/** Cleanup spawned test actors */
	void CleanupTestActors();

	// ==================== TEST STATE ====================

	UPROPERTY()
	TArray<AActor*> SpawnedTestActors;

	UPROPERTY()
	TArray<UObject*> CreatedTestAssets;

	int32 TestsPassed = 0;
	int32 TestsFailed = 0;
};
