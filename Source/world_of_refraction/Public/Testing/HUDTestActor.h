// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUDTestActor.generated.h"

class UCharacterPanelWidget;

/**
 * AHUDTestActor
 *
 * Editor-driven test harness for the new HUD widgets (Phase 1+).
 * Lets you spawn individual HUD widgets in isolation without starting combat,
 * to verify bindings and behaviour against live actor state.
 *
 * Pattern matches TurnManagerTestActor / StatusEffectManagerTestActor /
 * CombatOrchestratorTestActor — drop into the level, set properties in Details
 * panel, click CallInEditor buttons during PIE.
 *
 * Usage:
 *   1. Drop into level
 *   2. In Details panel, set CharacterPanelClass = WBP_CharacterPanel_New
 *   3. Set TargetActor = a level-placed combat character
 *   4. Press Play
 *   5. Click "Spawn Character Panel" button
 *   6. Damage/heal the target via console or other debug tools, watch panel update
 *   7. Click "Clear All Panels" to reset
 */
UCLASS()
class WORLD_OF_REFRACTION_API AHUDTestActor : public AActor
{
	GENERATED_BODY()

public:
	AHUDTestActor();

	// ========================================
	// Configuration (set in Details panel)
	// ========================================

	/** Widget class to spawn. Set to WBP_CharacterPanel_New. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Test")
	TSubclassOf<UCharacterPanelWidget> CharacterPanelClass;

	/** Level-placed actor to bind the panel to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Test")
	TSoftObjectPtr<AActor> TargetActor;

	// ========================================
	// CallInEditor actions
	// ========================================

	/** Spawn a character panel bound to TargetActor. Requires PIE. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "HUD Test|Actions")
	void SpawnCharacterPanel();

	/** Remove all panels spawned by this actor. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "HUD Test|Actions")
	void ClearAllPanels();

	// ========================================
	// State manipulation (verify panel updates)
	// ========================================

	/** Damage amount applied per click of ApplyTestDamage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Test|Test Values")
	int32 TestDamageAmount = 25;

	/** EP cost applied per click of SpendTestEnergy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Test|Test Values")
	int32 TestEnergyCost = 20;

	/** Status buildup added per click of AddTestStatusBuildup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Test|Test Values")
	float TestStatusBuildup = 30.0f;

	/** Apply TestDamageAmount to TargetActor's HP. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "HUD Test|Actions")
	void ApplyTestDamage();

	/** Spend TestEnergyCost from TargetActor's EP. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "HUD Test|Actions")
	void SpendTestEnergy();

	/** Add TestStatusBuildup to TargetActor (uses StatusEffectManager). */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "HUD Test|Actions")
	void AddTestStatusBuildup();

private:
	UPROPERTY()
	TArray<UCharacterPanelWidget *> SpawnedPanels;
};
