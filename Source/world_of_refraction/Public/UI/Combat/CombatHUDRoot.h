// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatHUDRoot.generated.h"

class ACombatOrchestrator;
class UTurnOrderStripWidget;
class UCombatActionMenuBase;
class UDefensePromptWidget;

/**
 * UCombatHUDRoot
 *
 * Native C++ base for WBP_CombatHUDRoot. Top-level HUD container.
 * Owns lifecycle for child HUD widgets (turn order, menu, defense prompt).
 * Character panels are owned and spawned by BP_CombatOrchestrator, not this HUD.
 *
 * See Docs/Postmortems/CombatHUD_Redesign_Design.md for full architecture.
 *
 * Lifecycle:
 *   1. BP_CombatOrchestrator::OnCombatStartedUI creates this widget
 *   2. Calls InitialiseForCombat(orchestrator)
 *   3. Adds to viewport
 *   4. On combat end: orchestrator calls TeardownForCombatEnd, then RemoveFromParent
 *   5. BeginDestroy is defensive cleanup for PIE end paths
 */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UCombatHUDRoot : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called once at combat start. Binds children to subsystems. */
	UFUNCTION(BlueprintCallable, Category = "Combat HUD")
	void InitialiseForCombat(ACombatOrchestrator *Orchestrator);

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
	UTurnOrderStripWidget *TurnOrderStrip;

	UPROPERTY(BlueprintReadOnly, Category = "Combat HUD", meta = (BindWidgetOptional))
	UCombatActionMenuBase *CommandMenu;

	UPROPERTY(BlueprintReadOnly, Category = "Combat HUD", meta = (BindWidgetOptional))
	UDefensePromptWidget *DefensePrompt;

private:
	UPROPERTY()
	TWeakObjectPtr<ACombatOrchestrator> CurrentOrchestrator;

	bool bInitialised = false;
};
