// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatHUDRoot.generated.h"

class ACombatOrchestrator;
class UPanelWidget;
class UCharacterPanelWidget;
class UTurnOrderStripWidget;
class UCombatActionMenuBase;
class UDefensePromptWidget;

/**
 * UCombatHUDRoot
 *
 * Native C++ base for WBP_CombatHUDRoot. Top-level HUD container.
 * Owns lifecycle for all child HUD widgets (panels, turn order, menu, defense prompt).
 *
 * See Docs/Postmortems/CombatHUD_Redesign_Design.md for full architecture.
 *
 * Lifecycle:
 *   1. BP_CombatOrchestrator::OnCombatStartedUI creates this widget
 *   2. Calls InitialiseForCombat(orchestrator, team0, team1)
 *   3. Adds to viewport
 *   4. On combat end: orchestrator calls TeardownForCombatEnd, then RemoveFromParent
 *   5. BeginDestroy is defensive cleanup for PIE end paths
 */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UCombatHUDRoot : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called once at combat start. Spawns panels, binds children to subsystems. */
	UFUNCTION(BlueprintCallable, Category = "Combat HUD")
	void InitialiseForCombat(ACombatOrchestrator *Orchestrator,
							 const TArray<AActor *> &Team0,
							 const TArray<AActor *> &Team1);

	/** Called at combat end. Unbinds, clears panels, prepares for removal. */
	UFUNCTION(BlueprintCallable, Category = "Combat HUD")
	void TeardownForCombatEnd();

protected:
	virtual void NativeDestruct() override;
	virtual void BeginDestroy() override;

	// ========================================
	// BindWidget — auto-resolved from WBP layout
	// ========================================

	UPROPERTY(BlueprintReadOnly, Category = "Combat HUD", meta = (BindWidgetOptional))
	UPanelWidget *PlayerTeamContainer;

	UPROPERTY(BlueprintReadOnly, Category = "Combat HUD", meta = (BindWidgetOptional))
	UPanelWidget *EnemyTeamContainer;

	UPROPERTY(BlueprintReadOnly, Category = "Combat HUD", meta = (BindWidgetOptional))
	UTurnOrderStripWidget *TurnOrderStrip;

	UPROPERTY(BlueprintReadOnly, Category = "Combat HUD", meta = (BindWidgetOptional))
	UCombatActionMenuBase *CommandMenu;

	UPROPERTY(BlueprintReadOnly, Category = "Combat HUD", meta = (BindWidgetOptional))
	UDefensePromptWidget *DefensePrompt;

	// ========================================
	// Configuration
	// ========================================

	/** Subclass spawned per character for HP/EP/Status panels. Set in WBP defaults. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat HUD")
	TSubclassOf<UCharacterPanelWidget> CharacterPanelClass;

private:
	UPROPERTY()
	TWeakObjectPtr<ACombatOrchestrator> CurrentOrchestrator;

	UPROPERTY()
	TArray<UCharacterPanelWidget *> SpawnedPanels;

	bool bInitialised = false;
};
