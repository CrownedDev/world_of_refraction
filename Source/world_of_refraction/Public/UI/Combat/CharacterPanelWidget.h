// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Skills/Definitions/ESpellElement.h"
#include "CharacterPanelWidget.generated.h"

class AActor;
class UCharacterDataComponent;
class USkillEffectManager;
class UStatusBuildupManager;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UBrokenDarknessManager;
struct FActiveSkillEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPanelHovered, bool, bIsHovered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPanelClicked);

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
 *   - Status buildup   -> UStatusBuildupManager::OnStatusBuildupChanged
 *   - Buffs/debuffs    -> USkillEffectManager::OnEffectApplied / Removed / DurationChanged
 *   - Death            -> UCharacterDataComponent::OnDied
 *
 * Lifecycle: Spawned by BP_CombatOrchestrator as a standalone viewport widget
 * (or by debug spawn), given an actor via InitialiseForActor.
 */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UCharacterPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind to actor's CharacterDataComponent and SkillEffectManager. */
	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void InitialiseForActor(AActor *InActor);

	/** Unbind delegates. Called by HUD root or on destruct. */
	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void TeardownPanel();

	/** Returns the actor this panel represents. Null if torn down. */
	UFUNCTION(BlueprintPure, Category = "Character Panel")
	AActor *GetBoundActor() const { return BoundActor.Get(); }

	UPROPERTY(BlueprintAssignable, Category = "Character Panel|Events")
	FOnPanelHovered OnPanelHovered;

	UPROPERTY(BlueprintAssignable, Category = "Character Panel|Events")
	FOnPanelClicked OnPanelClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void BeginDestroy() override;

	virtual void NativeOnMouseEnter(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

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

	/** Container for active skill-effect rows. BP populates via RebuildEffectsList.
	 *  Also receives a synthetic row for BD absorption stacks (StatusMultiplierBuff,
	 *  element-aligned) — surfaces stacks through the same pipeline as real effects,
	 *  no separate widget. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UVerticalBox *EffectsList;

	// ========================================
	// Delegate handlers
	// ========================================

	UFUNCTION()
	void HandleHPChanged(int32 CurrentHP, int32 MaxHP);
	UFUNCTION()
	void HandleEPChanged(int32 CurrentEP, int32 MaxEP);
	UFUNCTION()
	void HandleStatusBuildupChanged(AActor *Target, float Current, float Max, ESpellElement PendingElement, AActor *Source);
	UFUNCTION()
	void HandleEffectApplied(AActor *Target, const FActiveSkillEffect &Effect);
	UFUNCTION()
	void HandleEffectRemoved(AActor *Target, const FActiveSkillEffect &Effect);
	UFUNCTION()
	void HandleEffectDurationChanged(AActor *Target, const FActiveSkillEffect &Effect, int32 RemainingTurns);
	UFUNCTION()
	void HandleDied(AActor *DeadActor);

	/** Called when BD absorption energy changes */
	UFUNCTION()
	void HandleBDEnergyAbsorbed(AActor *Actor, float AmountAbsorbed, ESpellElement AbsorbedElement);

	/** Called when BD enters/exits overload state */
	UFUNCTION()
	void HandleBDOverloadStateChanged(AActor *Actor, bool bIsOverloaded);

	/** Called when a BD's absorption stack count changes (same alignment). */
	UFUNCTION()
	void HandleBDStacksChanged(AActor *Actor, ESpellElement Element, int32 NewStackCount);

	/** Called when a BD's alignment switches (different element absorbed). */
	UFUNCTION()
	void HandleBDAlignmentChanged(AActor *Actor, ESpellElement OldElement, ESpellElement NewElement);

	/** Called when a Darkness character transforms into Broken Darkness.
	 *  Refreshes every BD-affected display on the panel immediately so the
	 *  player doesn't have to wait for the next EP / status / stack broadcast
	 *  for the panel to look right. */
	UFUNCTION()
	void HandleBDTransformed(AActor *Actor);

	/** BP fills the EffectsList from this array each time it changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character Panel")
	void RebuildEffectsList(const TArray<FActiveSkillEffect> &ActiveEffects);

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

	TWeakObjectPtr<USkillEffectManager> BoundStatusManager;

	/** Bound buildup manager — owns OnStatusBuildupChanged post-split. */
	TWeakObjectPtr<UStatusBuildupManager> BoundBuildupManager;

	/** Bound BD manager for absorption-energy display (when applicable) */
	TWeakObjectPtr<UBrokenDarknessManager> BoundBDManager;

	bool bBound = false;

	void RefreshEffectsList();
	void SetBarSafe(UProgressBar *Bar, float Percent);
	void SetTextSafe(UTextBlock *Text, const FString &Value);

	/** Refresh the energy bar — picks the correct source based on character class/state */
	void RefreshEnergyBar();

	/** Apply element-coloured tint to the energy bar based on the character's
	 *  currently-displayed energy source */
	void ApplyEnergyBarTint();

	/** Tint the status buildup bar to the pending-cap element. A Broken Darkness
	 *  attacker darkens the tint (BlendedColor); Generic (physical damage) falls
	 *  back to a neutral fill. Called on every buildup change. */
	void ApplyStatusBarTint(ESpellElement PendingElement, AActor *Source);

	/** Apply EP-bar visibility rule for the currently bound character.
	 *  Hides bar+text for Resonator-without-weapon; shows otherwise.
	 *  Called once from InitialiseForActor. */
	void RefreshEPBarVisibility();
};