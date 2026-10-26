// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Styling/SlateBrush.h" // FSlateBrush value member (portrait placeholder cache)
#include "CharacterPanelWidget.generated.h"

class AActor;
class UCharacterData;
class UCharacterDataComponent;
class UImage;
class USkillEffectManager;
class UStatusBuildupManager;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UBrokenDarknessManager;
struct FActiveSkillEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPanelHovered, bool, bIsHovered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPanelClicked);

/** What a panel instance is currently showing (Arc 1.5b unification). */
UENUM(BlueprintType)
enum class EPanelDisplayMode : uint8
{
	/** Ghost slot — no character. */
	Vacant,
	/** Static template data from a UCharacterData asset — no actor, no delegates. */
	Roster,
	/** Full live combat panel bound to a spawned actor (InitialiseForActor). */
	Live
};

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
 * Lifecycle: Spawned by UCombatHUD as a standalone viewport widget
 * (or by debug spawn), given an actor via InitialiseForActor.
 */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UCharacterPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCharacterPanelWidget(const FObjectInitializer &ObjectInitializer);

	/** Bind to actor's CharacterDataComponent and SkillEffectManager. */
	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void InitialiseForActor(AActor *InActor);

	/** Roster mode: static template text from the asset alone — no actor, no
	 *  delegate binds, bBound stays false. bIsLeader suffixes the name with
	 *  " (You)" — slot 0 is the player per the design lock. Null Data routes to
	 *  SetVacant rather than asserting.
	 *
	 *  HP/EP now RENDER in Roster: InCurrentHP/InCurrentEP carry the party slot's
	 *  trial vitals. Pass PartyConstants::NO_CARRIED_VITALS (the -1 default) for
	 *  no carry — the bars then fill from the asset's computed maxima, so a hub
	 *  member reads full. Status stays collapsed either way: no buildup outside
	 *  battle.
	 *
	 *  ⚠️ The defaults are literal -1, not PartyConstants::NO_CARRIED_VITALS —
	 *  UHT cannot parse scope resolution in a UFUNCTION default argument. The
	 *  two must stay in sync; the .cpp compares against the named constant. */
	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void InitialiseForData(UCharacterData *Data, bool bIsLeader, int32 InCurrentHP = -1, int32 InCurrentEP = -1);

	/** Ghost slot: "Empty" name, everything else blank/collapsed, portrait
	 *  placeholder dimmed to GhostAlpha. */
	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void SetVacant();

	/** What this panel is currently showing. */
	UFUNCTION(BlueprintPure, Category = "Character Panel")
	EPanelDisplayMode GetDisplayMode() const { return DisplayMode; }

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

	/** Roster/hub portrait. Combat WBPs may omit it — the ref no-ops null. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitImage;

	/** PortraitImage opacity in the Vacant ghost state. WBP-tunable; default
	 *  from PanelGhostConstants (ported from the deleted PartyMemberCardWidget). */
	UPROPERTY(EditDefaultsOnly, Category = "Character Panel", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float GhostAlpha;

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

	/** Fills EffectsList each time the effect set changes. Native default writes a
	 *  pre-spawned pool of text rows (never creates widgets per call — TransBuffer rule).
	 *  A BP override replaces it; delete the WBP_CharacterPanel override to cut over. */
	UFUNCTION(BlueprintNativeEvent, Category = "Character Panel")
	void RebuildEffectsList(const TArray<FActiveSkillEffect> &ActiveEffects);
	virtual void RebuildEffectsList_Implementation(const TArray<FActiveSkillEffect> &ActiveEffects);

	/**
	 * Called once during InitialiseForActor to populate name/class/element/world stats.
	 * Native default fills NameText, ClassElementText and WorldStatsText; BP may override.
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

	/** Upper bound on visible effect rows; overflow collapses into a "+N more" last row. */
	static constexpr int32 MaxEffectRows = 8;

	/** Native effect rows, spawned once into EffectsList (only when no BP override). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> EffectRowPool;

	void SpawnEffectRowPool();

	EPanelDisplayMode DisplayMode = EPanelDisplayMode::Vacant;

	/** Roster-mode data source — HARD ref, pins the asset like FPartyMember does. */
	UPROPERTY()
	TObjectPtr<UCharacterData> RosterData;

	/** Roster-mode leader flag — drives the " (You)" name suffix. */
	bool bRosterLeader = false;

	/** The WBP-authored portrait placeholder brush, captured before the first C++
	 *  write so a null Portrait and the Vacant state restore the designer's
	 *  placeholder instead of a stale portrait (ported from PartyMemberCardWidget). */
	FSlateBrush DefaultPortraitBrush;
	bool bPortraitBrushCached = false;

	void CachePortraitBrushOnce();

	/** Portrait for Data (null = placeholder brush), full opacity. */
	void UpdatePortrait(UCharacterData *Data);

	/** The static-text data source for the current mode: Live = the bound
	 *  component's asset, Roster = RosterData, Vacant = null. */
	UCharacterData *GetDisplayData() const;

	/** Collapse (Vacant) or restore (Live) ALL runtime rows. Roster drives the
	 *  three groups separately — vitals and world stats on the carry sentinel,
	 *  status always off — via the helpers below. */
	void SetRuntimeRowsVisible(bool bVisible);

	/** HP/EP bars + texts. Roster shows these only for a carried (in-trial) slot. */
	void SetVitalsRowsVisible(bool bVisible);

	/** Status bar/text ONLY. Roster keeps these collapsed in every case: buildup is
	 *  battle-only and is cleared between battles. */
	void SetStatusRowsVisible(bool bVisible);

	/** World stats (MSB) row. Split out of SetStatusRowsVisible: it tracks the
	 *  vitals, not status — a trial roster shows it, a hub roster does not. */
	void SetWorldStatsRowVisible(bool bVisible);

	/** Fill the HP/EP rows in Roster mode from the asset's computed maxima,
	 *  overlaid with carried values when InCurrentHP is not NO_CARRIED_VITALS. */
	void ApplyRosterVitals(UCharacterData *Data, int32 InCurrentHP, int32 InCurrentEP);

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