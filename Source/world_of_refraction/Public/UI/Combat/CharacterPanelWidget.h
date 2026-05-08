// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ESpellElement.h"
#include "CharacterPanelWidget.generated.h"

class AActor;
class UCharacterDataComponent;
class UStatusEffectManager;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UBrokenDarknessManager;
class URingManager;
class URingData;
class UItemData;
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
 *   - HP/EP            -> UCharacterDataComponent::OnHPChanged / OnEPChanged
 *   - Status buildup   -> UStatusEffectManager::OnStatusBuildupChanged
 *   - Buffs/debuffs    -> UStatusEffectManager::OnEffectApplied / Removed / DurationChanged
 *   - Death            -> UCharacterDataComponent::OnDied
 *
 * Lifecycle: Created by UCombatHUDRoot (or by debug spawn), given an actor via
 * InitialiseForActor.
 */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UCharacterPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind to actor's CharacterDataComponent and StatusEffectManager. */
	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void InitialiseForActor(AActor *InActor);

	/** Unbind delegates. Called by HUD root or on destruct. */
	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void TeardownPanel();

	/** Returns the actor this panel represents. Null if torn down. */
	UFUNCTION(BlueprintPure, Category = "Character Panel")
	AActor *GetBoundActor() const { return BoundActor.Get(); }

protected:
	virtual void NativeDestruct() override;
	virtual void BeginDestroy() override;

	// ========================================
	// BindWidget — match names in WBP_CharacterPanel
	// ========================================

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UProgressBar *HPBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock *HPText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UProgressBar *EPBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock *EPText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UProgressBar *StatusBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock *StatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock *NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock *ClassElementText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock *WorldStatsText;

	/** Container for buff/debuff text rows. BP populates via RebuildBuffDebuffList. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UVerticalBox *BuffDebuffList;

	// ========================================
	// Delegate handlers
	// ========================================

	UFUNCTION()
	void HandleHPChanged(int32 CurrentHP, int32 MaxHP);
	UFUNCTION()
	void HandleEPChanged(int32 CurrentEP, int32 MaxEP);
	UFUNCTION()
	void HandleStatusBuildupChanged(AActor *Target, float Current, float Max);
	UFUNCTION()
	void HandleEffectApplied(AActor *Target, const FStatusEffect &Effect);
	UFUNCTION()
	void HandleEffectRemoved(AActor *Target, const FStatusEffect &Effect);
	UFUNCTION()
	void HandleEffectDurationChanged(AActor *Target, const FStatusEffect &Effect, int32 RemainingTurns);
	UFUNCTION()
	void HandleDied(AActor *DeadActor);

	/** Called when BD absorption energy changes */
	UFUNCTION()
	void HandleBDEnergyAbsorbed(AActor *Actor, float AmountAbsorbed, ESpellElement AbsorbedElement);

	/** Called when BD enters/exits overload state */
	UFUNCTION()
	void HandleBDOverloadStateChanged(AActor *Actor, bool bIsOverloaded);

	/** Called when a ring crystal breaks (Resonator durability display) */
	UFUNCTION()
	void HandleRingCrystalBroken(AActor *Actor, URingData *Ring, UItemData *Crystal);

	/** Called when ring crystal durability changes per-cast (Resonator bar update) */
	UFUNCTION()
	void HandleRingDurabilityChanged(AActor *Actor, URingData *Ring, int32 NewDurability, int32 MaxDurability);

	/** BP fills the BuffDebuffList from this array each time it changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character Panel")
	void RebuildBuffDebuffList(const TArray<FStatusEffect> &ActiveEffects);

	/**
	 * Called once during InitialiseForActor to populate name/class/element/world stats.
	 * BP can override for custom formatting; native fallback fills NameText only.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Character Panel")
	void ApplyStaticText();
	virtual void ApplyStaticText_Implementation();

private:
	TWeakObjectPtr<AActor> BoundActor;

	TWeakObjectPtr<UCharacterDataComponent> BoundCharData;

	TWeakObjectPtr<UStatusEffectManager> BoundStatusManager;

	/** Bound BD manager for absorption-energy display (when applicable) */
	TWeakObjectPtr<UBrokenDarknessManager> BoundBDManager;

	/** Bound ring manager for Resonator durability display (when applicable) */
	TWeakObjectPtr<URingManager> BoundRingManager;

	bool bBound = false;

	void RefreshBuffDebuffList();
	void SetBarSafe(UProgressBar *Bar, float Percent);
	void SetTextSafe(UTextBlock *Text, const FString &Value);

	/** Refresh the energy bar — picks the correct source based on character class/state */
	void RefreshEnergyBar();

	/** Apply element-coloured tint to the energy bar based on the character's
	 *  currently-displayed energy source */
	void ApplyEnergyBarTint();
};