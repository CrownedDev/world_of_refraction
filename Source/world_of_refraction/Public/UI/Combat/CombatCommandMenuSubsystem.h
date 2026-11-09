// CombatCommandMenuSubsystem.h
// Drives the combat command menu. Builds capabilities from LoadoutComponent,
// exposes button arrays to Blueprint via delegates, handles selection routing.
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "Combat/WoRFightSubsystem.h"
#include "UI/Combat/FCombatCapabilities.h"
#include "UI/Combat/PieMenuButtonData.h"
#include "Character/ECharacterClass.h"
#include "Infusion/ElementColors.h"
#include "Combat/TargetType.h"
#include "Combat/Actions/ActionStructs.h"
#include "Skills/Definitions/ESpellSource.h"
#include "Skills/Definitions/ESpellElement.h"
#include "CombatCommandMenuSubsystem.generated.h"

class ULoadoutComponent;
enum class ECombatFreeCommand : uint8;
class UCharacterData;
class USpellData;
class UAbilityData;
class UBrokenDarknessManager;

// ==================== DELEGATES ====================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCommandMenuReady,
                                            const TArray<FPieMenuButtonData> &, Buttons);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCommandMenuClosed);

/** B-399 (study R2): the open menu's depth changed (CommandMenuRules::CameraDepthFor); Actor is whose menu it is.
 *  Closing the menu sends nothing (the camera holds for the action); the next open sends its depth if it differs. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCommandMenuDepthChanged, AActor *, Actor, int32, MenuDepth);

// ==================== MENU DEPTH ====================

enum class ECombatMenuDepth : uint8
{
    Closed,
    Main,
    Submenu,
    TargetCategory,
    TargetSelection
};

// ==================== WHERE THE MENU OPENS (B-232b) ====================

namespace CommandMenuRules
{
	/** The command menu opens on the machine whose player commands the member (Q-232, D-150): never for an AI seat;
	 *  with no run state (standalone, specs) the one player commands everything, as before. */
	inline bool OpensHere(bool bAIControlled, bool bHasRunState, bool bCommandedHere)
	{
		return !bAIControlled && (!bHasRunState || bCommandedHere);
	}

	/** B-391 (Q-391): a category key that works on the main menu only (one key per category, two presses to act,
	 *  FeelGuide 13). Defend and the infusion keys keep their own rules (B-093). */
	inline bool IsMainMenuKey(EPieMenuCategory Key)
	{
		switch (Key)
		{
		case EPieMenuCategory::Attack:
		case EPieMenuCategory::Abilities:
		case EPieMenuCategory::Refractions:
		case EPieMenuCategory::Items:
		case EPieMenuCategory::Flee:
		case EPieMenuCategory::SwitchWeapon:
		case EPieMenuCategory::ChangeRing:
			return true;
		default:
			return false;
		}
	}

	/** B-391 (Q-391): whether a key presses a button. The Skills key (Abilities) also presses a weapon resonance, and
	 *  the Spells key (Refractions) a breakthrough or a ring resonance, so each class's skills and spells share a key. */
	inline bool KeyPressesButton(EPieMenuCategory Key, EPieMenuCategory Button)
	{
		if (Key == Button)
		{
			return true;
		}
		if (Key == EPieMenuCategory::Abilities)
		{
			return Button == EPieMenuCategory::ResonateWeapon;
		}
		if (Key == EPieMenuCategory::Refractions)
		{
			return Button == EPieMenuCategory::Breakthrough || Button == EPieMenuCategory::ResonateRing;
		}
		return false;
	}

	/** B-399: the main menu (or a closed menu) and a sub-menu (Skills / Items / a spell source), as the camera sees
	 *  them (UCombatCameraDirector steps one closer for a sub-menu). */
	constexpr int32 MAIN_MENU_DEPTH = 0;
	constexpr int32 SUBMENU_DEPTH = 1;

	/** B-399 (study R2, R3, Q-399): the menu depth the camera follows. A target picker keeps the depth it was opened
	 *  from (Attack's picker the main menu's, a skill's the sub-menu's), so picking a target never moves the camera. */
	inline int32 CameraDepthFor(ECombatMenuDepth Depth, ECombatMenuDepth DepthBeforeTargets)
	{
		switch (Depth)
		{
		case ECombatMenuDepth::Submenu:
			return SUBMENU_DEPTH;
		case ECombatMenuDepth::TargetCategory:
		case ECombatMenuDepth::TargetSelection:
			return DepthBeforeTargets == ECombatMenuDepth::Submenu ? SUBMENU_DEPTH : MAIN_MENU_DEPTH;
		default:
			return MAIN_MENU_DEPTH;
		}
	}
}

// ==================== SUBSYSTEM ====================

UCLASS()
class WORLD_OF_REFRACTION_API UCombatCommandMenuSubsystem : public UWoRFightSubsystem
{
    GENERATED_BODY()

public:
    // ==================== LIFECYCLE ====================

    virtual void Initialize(FSubsystemCollectionBase &Collection) override;

    // ==================== COMBAT REGISTRATION ====================

    /** Set the active combat orchestrator. Binds turn lifecycle events. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void SetCombatOrchestrator(class ACombatOrchestrator *Orchestrator);

    /** Clear combat orchestrator. Unbinds events and closes menu. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void ClearCombatOrchestrator();

    virtual void Deinitialize() override;

    // ==================== BLUEPRINT API ====================

    /** Call when a player turn starts. Builds capabilities and broadcasts OnCommandMenuReady. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void OpenForActor(AActor *Actor);

    /** Call when the player clicks a button. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void HandleSelection(const FPieMenuButtonData &ButtonData);

    /** Call when the player clicks Back. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void HandleBack();

    /** B-093: a key press for a menu button (Defend, CycleSource, CycleLevel). Presses the
     *  button the menu would show for it now, through HandleSelection, so the key obeys every
     *  rule the button does: Defend from the main menu or a submenu, the infusion keys in the
     *  target picker when it offers them. False (nothing done) when there's no such button.
     *  B-391: the category keys (CommandMenuRules::IsMainMenuKey) press their main-menu button, from the main menu
     *  only (CommandMenuRules::KeyPressesButton: the first enabled match in menu order). */
    bool PressMenuKey(EPieMenuCategory Category);

    /** Call when the turn ends or combat closes the menu. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void Close();

    // ==================== DELEGATES ====================

    UPROPERTY(BlueprintAssignable, Category = "Combat Command Menu|Events")
    FOnCommandMenuReady OnCommandMenuReady;

    UPROPERTY(BlueprintAssignable, Category = "Combat Command Menu|Events")
    FOnCommandMenuClosed OnCommandMenuClosed;

    /** B-399: once per menu-depth change (CommandMenuRules::CameraDepthFor), never for a redraw or a scroll. The
     *  combat camera listens (whichever HUD draws the menu). */
    UPROPERTY(BlueprintAssignable, Category = "Combat Command Menu|Events")
    FOnCommandMenuDepthChanged OnMenuDepthChanged;

    /** B-399: the depth last broadcast on OnMenuDepthChanged. */
    int32 GetCameraMenuDepth() const { return BroadcastMenuDepth; }

    // ==================== STATE QUERIES ====================

    UFUNCTION(BlueprintPure, Category = "Combat Command Menu")
    bool IsOpen() const { return bIsOpen; }

    UFUNCTION(BlueprintPure, Category = "Combat Command Menu")
    bool IsAtMainMenu() const { return CurrentDepth == ECombatMenuDepth::Main; }

    UFUNCTION(BlueprintPure, Category = "Combat Command Menu")
    const FCombatCapabilities &GetCurrentCapabilities() const { return CurrentCapabilities; }

    /** Get the actor whose menu is currently open (or null if closed). */
    UFUNCTION(BlueprintPure, Category = "Combat Command Menu")
    AActor *GetCurrentActor() const { return CurrentActor.Get(); }

    /** B-034: the infusion level the open target picker will stamp on the action (0..2).
     *  The menu owns it; the actor's InfusionVFXComponent only shows it. */
    UFUNCTION(BlueprintPure, Category = "Combat Command Menu")
    int32 GetPendingInfusionLevel() const { return PendingInfusionLevel; }

    // ==================== DEBUG ====================

    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu|Debug",
              meta = (DevelopmentOnly))
    void DebugLogCapabilities() const;

    /** Call this to re-broadcast the current main menu buttons — use when a widget binds late */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void RefreshMenu();

    /** The Schools sub-menu: one button per school the spells hold, in school order, then Back (D-163: up to six
     *  plus Back, a gamepad radial's eight slots). Public and static for the headless spec (B-374). */
    static TArray<FPieMenuButtonData> BuildSchoolButtons(const TArray<USpellData *> &Spells);

    /** B-232c (client): the server ran a free step for Actor (ACombatPlayerController::Client_FreeCommandApplied);
     *  the open menu redraws from the stepped loadout. */
    void HandleFreeCommandApplied(AActor *Actor, ECombatFreeCommand Command);

private:
    /** B-232c: on a client, a free step goes to the server (ACombatPlayerController::RequestFreeCommand) instead of
     *  running here; true when it was sent. */
    bool SendFreeCommandToServer(ECombatFreeCommand Command);

    // ==================== ORCHESTRATOR BINDING ====================

    UPROPERTY()
    TWeakObjectPtr<class ACombatOrchestrator> CurrentOrchestrator;

    UFUNCTION()
    void HandlePlayerActionRequested(AActor *Actor);

    UFUNCTION()
    void HandleActionExecuted(AActor *Actor, const struct FActionResult &Result);

    // ==================== STATE ====================

    FCombatCapabilities CurrentCapabilities;
    ECombatMenuDepth CurrentDepth = ECombatMenuDepth::Closed;
    bool bIsOpen = false;
    EPieMenuCategory ActiveSubmenuSource = EPieMenuCategory::None;

    EPieMenuCategory ActiveSubmenuType = EPieMenuCategory::None;
    // Pending action awaiting target selection
    EPieMenuCategory PendingActionCategory = EPieMenuCategory::None;
    FString PendingActionID;
    TWeakObjectPtr<UObject> PendingActionData;
    // Slot index for identity-by-index buttons (consumables). DataReference is
    // null for these, so the index must survive the target-selection round-trip.
    int32 PendingActionDataIndex = -1;
    ETargetType PendingTargetType = ETargetType::Enemy;
    /** Effective target type after the Allies/Enemies category step. Synced to
     *  PendingTargetType on entry; set to Ally/Enemy when a side is
     *  picked. ResolvedTargetType != PendingTargetType means the category step
     *  is active (used by Back routing and RebuildCurrentPicker). */
    ETargetType ResolvedTargetType = ETargetType::Enemy;
    /** Count axis carried alongside the type through the picker. Drives the
     *  group-vs-single decision (All -> group confirm; Single/Double -> per-actor
     *  picker). Narrowed to Single after the Allies/Enemies category step. */
    ETargetCount PendingTargetCount = ETargetCount::Single;
    ETargetCount ResolvedTargetCount = ETargetCount::Single;
    /** B-034: the pending action's infusion level. Reset to 0 on every picker entry and exit. */
    int32 PendingInfusionLevel = 0;
    ECombatMenuDepth DepthBeforeTargetSelection = ECombatMenuDepth::Main;
    TWeakObjectPtr<AActor> CurrentActor;

    /** B-399: while the menu is open, broadcasts OnMenuDepthChanged when the camera depth differs from the last one
     *  sent. Runs at the end of every call that can change the depth (OpenForActor, HandleSelection, HandleBack). */
    void BroadcastMenuDepthIfChanged();
    int32 BroadcastMenuDepth = CommandMenuRules::MAIN_MENU_DEPTH;

    /** BrokenDarknessManager of the actor whose menu is open — bound to
     *  OnAlignmentChanged for live BD spell-pool refresh. */
    TWeakObjectPtr<UBrokenDarknessManager> BoundBDManager;

    // ==================== MAIN MENU ====================

    TArray<FPieMenuButtonData> BuildMainMenuButtons() const;

    // ==================== BUTTON CREATORS ====================

    FPieMenuButtonData CreateAttackButton() const;
    FPieMenuButtonData CreateAbilitiesButton() const;
    FPieMenuButtonData CreateResonateWeaponButton() const;
    FPieMenuButtonData CreateRefractionsButton() const;
    FPieMenuButtonData CreateBreakthroughButton() const;
    FPieMenuButtonData CreateResonateRingButton() const;
    FPieMenuButtonData CreateItemsButton() const;
    FPieMenuButtonData CreateSwitchWeaponButton() const;
    /** D-091: Flee, greyed with the reason when the fight can't be fled. */
    FPieMenuButtonData CreateFleeButton() const;
    /** OBSOLETE (B-346): the main menu no longer adds this button; seats are fixed for the fight
     *  (Q-21-audit, PA-13). Kept until PIE; the switch stays for debug (wor.Combat.SwitchControl).
     *  D-026: "Let AI Play" for a non-leader party member on its turn. */
    FPieMenuButtonData CreateHandToAIButton() const;
    bool CanHandToAI() const;
    /** D-096 placeholder: "Step Forward" / "Step Back", greyed with the reason. */
    FPieMenuButtonData CreateMoveRowButton(bool bForward) const;
    /** ET-19: greys a switch button, with the reason, once the turn's free switch is used. */
    void GreyIfTurnSwitchUsed(FPieMenuButtonData &Button) const;
    FPieMenuButtonData CreateSwitchRingButton() const;
    FPieMenuButtonData CreateDefendButton() const;

    // ==================== SUBMENU BUILDERS ====================

    TArray<FPieMenuButtonData> BuildSpellSubmenu(EPieMenuCategory Source) const;
    TArray<FPieMenuButtonData> BuildAbilitySubmenu() const;
    TArray<FPieMenuButtonData> BuildAbilityButtonsFrom(const TArray<UAbilityData *> &Abilities) const;
    TArray<FPieMenuButtonData> BuildSpellButtons(const TArray<USpellData *> &Spells,
                                                 EPieMenuSpellSchool School) const;
    TArray<FPieMenuButtonData> BuildItemsSubmenu() const;

    // ==================== SELECTION HANDLERS ====================

    void OpenAbilitySubmenu();
    void OpenSpellSubmenu(EPieMenuCategory Source);
    void ExecuteSwitchWeapon();
    void ExecuteMoveRow(bool bForward);
    void ExecuteSwitchRing();
    void OpenItemsSubmenu();

    // Target selection
    void OpenTargetSelection(EPieMenuCategory ActionCategory, const FPieMenuButtonData &ActionButton, ETargetType TargetType, ETargetCount TargetCount);
    /** Two-button Allies / Enemies view shown before the per-actor picker when
     *  the action's TargetType is Anyone (single/double). Each button carries the
     *  narrowed ETargetType (Ally / Enemy) in DataIndex. */
    TArray<FPieMenuButtonData> BuildTargetCategoryButtons() const;
    /** Exit the picker back to the originating action menu (Submenu or Main):
     *  resets infusion, cancels the pending action, clears ResolvedTargetType. */
    void ExitTargetSelectionToActionMenu();
    TArray<FPieMenuButtonData> BuildTargetButtons(const TArray<AActor *> &Targets) const;
    TArray<FPieMenuButtonData> BuildGroupTargetButtons(ETargetType TargetType, const TArray<AActor *> &Targets) const;

    /** B-254 (DRY audit A11): the infusion controls (cycle level with the source readout, cycle source) a target
     *  picker ends with; none for items or an infusion-locked action. */
    void AppendInfusionControls(TArray<FPieMenuButtonData> &Buttons) const;
    /** Rebuild the active target picker using ResolvedTargetType + ResolvedTargetCount
     *  to pick between the per-actor picker (Single/Double) and the group confirm
     *  picker (auto-resolve: Self, or any TargetType with count All).
     *  ResolvedTargetType (not PendingTargetType) so a post-category-step picker
     *  rebuilds the narrowed single-team list, not the full Anyone set.
     *  Called when an infusion control mutates state and the picker needs to
     *  re-broadcast with the same shape it was opened with. */
    TArray<FPieMenuButtonData> RebuildCurrentPicker() const;
    TArray<AActor *> ResolveTargets(ETargetType TargetType) const;
    void ConfirmActionWithTarget(AActor *SelectedTarget);
    void ConfirmActionWithTargets(const TArray<AActor *> &SelectedTargets);

    // ==================== ACTION SUBMISSION ====================

    FAction BuildActionFromButton(const FPieMenuButtonData &ButtonData) const;
    ESpellSource MapCategoryToSpellSource(EPieMenuCategory SubmenuCategory) const;
    void SubmitConfirmedAction(const FPieMenuButtonData &ButtonData);

    // ==================== HELPERS ====================

    ULoadoutComponent *GetLoadoutComponent() const;
    FLinearColor GetElementColor(int32 ElementIndex) const;
    class UInfusionVFXComponent *GetInfusionVFXComponent() const;
    /** D-001: a stunned actor's menu offers only Attack + Defend, with no infusion (D-023 A). */
    bool IsCurrentActorStunned() const;
    /** Infusion controls are inert for this pick: immune action/equipment, or a stunned actor. */
    bool IsInfusionLockedForPendingAction() const;
    /** B-034: store the pending infusion level (0 when infusion is locked for this pick, else
     *  clamped to 0..MAX_INFUSION_LEVEL) and show it on the actor's InfusionVFXComponent. */
    void SetPendingInfusionLevel(int32 Level);

    // ==================== CAPABILITIES / BD LIVE REFRESH ====================

    /** Rebuild CurrentCapabilities from CurrentActor's LoadoutComponent.
     *  Shared by OpenForActor, weapon/ring switches, and BD absorption refresh. */
    void RebuildCapabilities();

    /** Re-broadcast the menu view for the current depth — used after a live
     *  capability change so the open menu reflects it without a close/reopen. */
    void RebroadcastCurrentView();

    /** Bind / unbind CurrentActor's BrokenDarknessManager::OnAlignmentChanged so
     *  the menu refreshes when the absorbed element — and thus the castable BD
     *  spell pool — changes while the menu is open. */
    void BindBDAbsorptionDelegate();
    void UnbindBDAbsorptionDelegate();

    /** OnAlignmentChanged handler — rebuilds capabilities and re-broadcasts. */
    UFUNCTION()
    void HandleBDAlignmentChanged(AActor *Actor, ESpellElement OldElement, ESpellElement NewElement);
};
