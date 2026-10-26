// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TurnOrderSlotWidget.generated.h"

class AActor;
class UBorder;
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
 * Native default fills NameText / TurnNumberText and marks active/bonus slots
 * (scale + opacity + "+" badge on the number). A BP override replaces it.
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
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Turn Order Slot")
	void InitialiseSlot(AActor *InActor, int32 InTurnNumber, bool bIsActive, bool bIsBonus);
	virtual void InitialiseSlot_Implementation(AActor *InActor, int32 InTurnNumber, bool bIsActive, bool bIsBonus);

	// ========================================
	// BindWidget — match names in WBP_TurnOrderSlot
	// ========================================

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock *NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock *TurnNumberText;

	/** Tinted per slot state: bonus turn, active, waiting (B-003: ported from the WBP override). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UBorder *BackgroundBorder;
};