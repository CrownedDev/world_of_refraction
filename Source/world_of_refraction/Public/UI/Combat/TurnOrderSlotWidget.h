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
 * Display: actor name, turn number, team color tint, active/inactive state.
 */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UTurnOrderSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Configure slot for the given actor. bIsActive=true for slot 0 (current turn). */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Turn Order Slot")
	void InitialiseSlot(AActor* Actor, int32 TurnNumber, bool bIsActive);

	/** Optional: BindWidget targets for native fallback display. BP can override. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TurnNumberText;
};
