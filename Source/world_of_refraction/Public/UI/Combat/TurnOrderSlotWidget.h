// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TurnOrderSlotWidget.generated.h"

class AActor;
class UTextBlock;

/**
 * UTurnOrderSlotWidget
 *
 * Native C++ base for WBP_TurnOrderSlot. Single slot in the turn order strip.
 *
 * Pre-spawned by UTurnOrderStripWidget once at combat start. Reused per turn
 * via InitialiseSlot — no widget creation/destruction during combat.
 *
 * Display: actor name, turn number, active/inactive state, bonus-turn state.
 * BP handles the active/inactive AND bonus visual difference (color tint, scale, badge).
 */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UTurnOrderSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Configure slot data. Called once per slot per refresh.
	 *
	 * @param InActor       Actor for this slot (may be null = empty slot)
	 * @param InTurnNumber  Display turn number (1 = current, 2 = next, etc.)
	 * @param bIsActive     True for slot 0 (current actor), false for upcoming
	 * @param bIsBonus      True if this slot is a scheduled bonus turn (Emerald) — the WBP
	 *                      graph reads this to render a distinct badge/tint. Always false for
	 *                      the active (current-actor) slot and for normal upcoming turns.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Turn Order Slot")
	void InitialiseSlot(AActor *InActor, int32 InTurnNumber, bool bIsActive, bool bIsBonus);

	// ========================================
	// BindWidget — match names in WBP_TurnOrderSlot
	// ========================================

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock *NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock *TurnNumberText;
};