// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TurnOrderStripWidget.generated.h"

class AActor;
class UTurnManager;
class UPanelWidget;
class UTurnOrderSlotWidget;

/**
 * UTurnOrderStripWidget
 *
 * Native C++ base for WBP_TurnOrderStrip. Shows current actor + N upcoming turns.
 *
 * Pattern: pre-spawn (PreviewCount + 1) slot widgets at combat start, reuse them.
 * On each OnTurnStarted, refresh slot data from TurnManager::PreviewTurnOrder.
 *
 * Lesson from prior session: creating new slot widgets per turn caused TransBuffer
 * leaks. Pre-spawn pattern avoids this entirely.
 */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UTurnOrderStripWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Spawn slots and bind to TurnManager::OnTurnStarted. */
	UFUNCTION(BlueprintCallable, Category = "Turn Order")
	void InitialiseForCombat();

	/** Unbind delegates and clear slot widgets. */
	UFUNCTION(BlueprintCallable, Category = "Turn Order")
	void TeardownStrip();

	/** Number of upcoming turns to display after current actor. Total slots = 1 + this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn Order")
	int32 PreviewCount = 4;

	/** Slot widget class. Set in WBP defaults (e.g. WBP_TurnOrderSlot_New). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn Order")
	TSubclassOf<UTurnOrderSlotWidget> SlotWidgetClass;

protected:
	virtual void NativeDestruct() override;
	virtual void BeginDestroy() override;

	/** Container that holds the slot widgets (typically a Horizontal Box). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UPanelWidget *SlotContainer;

	UFUNCTION()
	void HandleTurnStarted(AActor *Actor, int32 TurnNumber);

private:
	UPROPERTY()
	TArray<UTurnOrderSlotWidget *> Slots;

	TWeakObjectPtr<UTurnManager> CachedTurnManager;

	bool bInitialised = false;

	int32 CurrentTurnNumber = 0;

	/** Create PreviewCount+1 slot widgets and add them to SlotContainer. */
	void SpawnSlots();

	/** Pull current actor + preview from TurnManager and update each slot. */
	void RefreshSlots();
};