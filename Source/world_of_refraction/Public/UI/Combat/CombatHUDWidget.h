// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatHUDWidget.generated.h"

class ACombatOrchestrator;
class UTurnManager;
class UPanelWidget;
class UUserWidget;
struct FActionResult;
class UCharacterPanelWidget;

/**
 * CombatHUDWidget
 *
 * Native C++ base for WBP_CombatHUD. Owns the combat HUD logic:
 * - Turn indicator strip (single or multi-slot, configurable)
 * - Listens to TurnManager events via CombatOrchestrator
 *
 * Visual layout (panels, slot widgets, command menu) stays in BP.
 * Widget instances are auto-bound at runtime via meta = (BindWidget).
 *
 * Setup:
 * 1. Reparent WBP_CombatHUD to UCombatHUDWidget
 * 2. Ensure widget names in Designer match the BindWidget property names
 * 3. Set TurnSlotWidgetClass to WBP_TurnOrderSlot in the BP defaults
 * 4. Configure TurnPreviewRange (1 = current actor only, N = current + N-1 future)
 */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UCombatHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCombatHUDWidget(const FObjectInitializer &ObjectInitializer);

	/** Bind the HUD to a combat orchestrator. Must be called once per combat session. */
	UFUNCTION(BlueprintCallable, Category = "Combat HUD")
	void BindToOrchestrator(ACombatOrchestrator *Orchestrator);

	/** Unbind from current orchestrator. Called automatically on destruct. */
	UFUNCTION(BlueprintCallable, Category = "Combat HUD")
	void UnbindFromOrchestrator();

	/** Number of upcoming turn slots to show (1 = current actor only). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD")
	int32 TurnPreviewRange = 1;

	/** Slot widget class spawned for each turn entry. Set to WBP_TurnOrderSlot in BP. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD")
	TSubclassOf<UUserWidget> TurnSlotWidgetClass;

	/** Character panel widget class spawned per team member. Set in BP defaults. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat HUD")
	TSubclassOf<class UCharacterPanelWidget> CharacterPanelWidgetClass;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void BeginDestroy() override;

	// ========================================
	// BindWidget — auto-wired to BP children by name
	// ========================================

	/** Container that holds the spawned turn order slots. Must exist in WBP_CombatHUD. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat HUD", meta = (BindWidget))
	UPanelWidget *TurnOrderStrip;

	/** Container for player team character panels (Team 0). Must exist in WBP_CombatHUD. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat HUD", meta = (BindWidget))
	UPanelWidget *PlayerTeamContainer;

	/** Container for enemy team character panels (Team 1). Must exist in WBP_CombatHUD. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat HUD", meta = (BindWidget))
	UPanelWidget *EnemyTeamContainer;

	// ========================================
	// Turn handling
	// ========================================

	/** Bound to TurnManager::OnTurnStarted. */
	UFUNCTION()
	void HandleTurnStarted(AActor *Actor, int32 TurnNumber);

	/** Bound to CombatOrchestrator::OnActionExecuted. */
	UFUNCTION()
	void HandleActionExecuted(AActor *Actor, const FActionResult &Result);

	/** Spawns and configures slot widgets up to TurnPreviewRange. Called once per combat. */
	void InitialiseTurnSlots();

	/** Removes all spawned slot widgets and clears the strip. */
	void ClearTurnSlots();

	/** Refreshes slot contents based on current actor and TurnManager preview. */
	void UpdateTurnSlots(AActor *CurrentActor);

	/** Calls the BP "Initialise" function on a slot widget with the given parameters. */
	void InitialiseSlot(UUserWidget *SlotWidget, AActor *Actor, bool bActive, int32 TurnNumber, int32 InTeamIndex);

	// ========================================
	// Team panel handling
	// ========================================

	/** Spawns character panels for both teams. Called from BP_CombatOrchestrator. */
	UFUNCTION(BlueprintCallable, Category = "Combat HUD")
	void InitialiseTeamPanels(const TArray<AActor *> &Team0, const TArray<AActor *> &Team1);

	/** Removes all spawned character panels. */
	UFUNCTION(BlueprintCallable, Category = "Combat HUD")
	void ClearTeamPanels();

private:
	/** Weak ref to the orchestrator we're bound to. */
	UPROPERTY()
	TWeakObjectPtr<ACombatOrchestrator> CurrentOrchestrator;

	/** Cached pointer to the turn manager. */
	UPROPERTY()
	TWeakObjectPtr<UTurnManager> CachedTurnManager;

	/** Spawned slot widgets. Index 0 = current actor, 1..N = upcoming. */
	UPROPERTY()
	TArray<UUserWidget *> SpawnedSlots;

	/** Spawned character panel widgets. Cleared on combat end. */
	UPROPERTY()
	TArray<class UCharacterPanelWidget *> SpawnedPanels;

	/** Tracks the current turn number for display. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat HUD", meta = (AllowPrivateAccess = "true"))
	int32 CurrentTurnNumber = 0;
};