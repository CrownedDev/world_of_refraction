// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterPanelWidget.generated.h"

class AActor;
class UCharacterDataComponent;
class UStatusEffectManager;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
struct FStatusEffect;

/**
 * UCharacterPanelWidget
 *
 * Native C++ base for WBP_CharacterPanel. One panel per character.
 *
 * Displays: HP/EP/Status bars (with text), name/class/element, world stats,
 *           buff/debuff text list.
 *
 * State sources:
 *   - HP/EP → UCharacterDataComponent (delegates)
 *   - Status buildup → UStatusEffectManager::OnStatusBuildupChanged
 *   - Buffs/debuffs → UStatusEffectManager (effect delegates)
 *   - Death → UCharacterDataComponent::OnDied
 *
 * Lifecycle: Created by UCombatHUDRoot, given an actor via InitialiseForActor.
 */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UCharacterPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind to actor's CharacterDataComponent and StatusEffectManager. */
	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void InitialiseForActor(AActor* InActor);

	/** Unbind delegates. Called by HUD root or on destruct. */
	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void TeardownPanel();

protected:
	virtual void NativeDestruct() override;
	virtual void BeginDestroy() override;

	// ========================================
	// BindWidget — match names in WBP_CharacterPanel
	// ========================================

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UProgressBar* HPBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* HPText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UProgressBar* EPBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* EPText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UProgressBar* StatusBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* StatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* ClassElementText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* WorldStatsText;

	/** Container for buff/debuff text rows. BP populates per RebuildBuffDebuffList. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UVerticalBox* BuffDebuffList;

	// ========================================
	// Delegate handlers (Phase 1 will implement)
	// ========================================

	UFUNCTION() void HandleHPChanged(int32 CurrentHP, int32 MaxHP);
	UFUNCTION() void HandleEPChanged(int32 CurrentEP, int32 MaxEP);
	UFUNCTION() void HandleStatusBuildupChanged(AActor* Target, float Current, float Max);
	UFUNCTION() void HandleEffectApplied(AActor* Target, const FStatusEffect& Effect);
	UFUNCTION() void HandleEffectRemoved(AActor* Target, const FStatusEffect& Effect);
	UFUNCTION() void HandleEffectDurationChanged(AActor* Target, const FStatusEffect& Effect, int32 RemainingTurns);
	UFUNCTION() void HandleDied(AActor* DeadActor);

	/** BP fills the BuffDebuffList from this array each time it changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character Panel")
	void RebuildBuffDebuffList(const TArray<FStatusEffect>& ActiveEffects);

private:
	UPROPERTY()
	TWeakObjectPtr<AActor> BoundActor;

	UPROPERTY()
	TWeakObjectPtr<UCharacterDataComponent> BoundCharData;

	UPROPERTY()
	TWeakObjectPtr<UStatusEffectManager> BoundStatusManager;

	bool bBound = false;
};
