// CombatCommandMenuSubsystem.h
// Drives the combat command menu. Builds capabilities from LoadoutComponent,
// exposes button arrays to Blueprint via delegates, handles selection routing.
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
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
class UCharacterData;
class USpellData;
class UBrokenDarknessManager;

// ==================== DELEGATES ====================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCommandMenuReady,
                                            const TArray<FPieMenuButtonData> &, Buttons);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCommandMenuClosed);

// ==================== MENU DEPTH ====================

enum class ECombatMenuDepth : uint8
{
    Closed,
    Main,
    Submenu,
    TargetCategory,
    TargetSelection
};

// ==================== SUBSYSTEM ====================

UCLASS()
class WORLD_OF_REFRACTION_API UCombatCommandMenuSubsystem : public UGameInstanceSubsystem
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

    /** Call when the turn ends or combat closes the menu. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void Close();

    // ==================== DELEGATES ====================

    UPROPERTY(BlueprintAssignable, Category = "Combat Command Menu|Events")
    FOnCommandMenuReady OnCommandMenuReady;

    UPROPERTY(BlueprintAssignable, Category = "Combat Command Menu|Events")
    FOnCommandMenuClosed OnCommandMenuClosed;

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

    // ==================== DEBUG ====================

    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu|Debug",
              meta = (DevelopmentOnly))
    void DebugLogCapabilities() const;

    /** Call this to re-broadcast the current main menu buttons — use when a widget binds late */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void RefreshMenu();

private:
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
    ETargetType PendingTargetType = ETargetType::SingleEnemy;
    /** Effective target type after the Allies/Enemies category step. Synced to
     *  PendingTargetType on entry; set to SingleAlly/SingleEnemy when a side is
     *  picked. ResolvedTargetType != PendingTargetType means the category step
     *  is active (used by Back routing and RebuildCurrentPicker). */
    ETargetType ResolvedTargetType = ETargetType::SingleEnemy;
    ECombatMenuDepth DepthBeforeTargetSelection = ECombatMenuDepth::Main;
    TWeakObjectPtr<AActor> CurrentActor;

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
    FPieMenuButtonData CreateSwitchRingButton() const;

    // ==================== SUBMENU BUILDERS ====================

    TArray<FPieMenuButtonData> BuildSpellSubmenu(EPieMenuCategory Source) const;
    TArray<FPieMenuButtonData> BuildAbilitySubmenu() const;
    TArray<FPieMenuButtonData> BuildSchoolButtons(const TArray<USpellData *> &Spells) const;
    TArray<FPieMenuButtonData> BuildSpellButtons(const TArray<USpellData *> &Spells,
                                                 EPieMenuSpellSchool School) const;
    TArray<FPieMenuButtonData> BuildItemsSubmenu() const;

    // ==================== SELECTION HANDLERS ====================

    void OpenAbilitySubmenu();
    void OpenSpellSubmenu(EPieMenuCategory Source);
    void ExecuteSwitchWeapon();
    void ExecuteSwitchRing();
    void OpenItemsSubmenu();

    // Target selection
    void OpenTargetSelection(EPieMenuCategory ActionCategory, const FPieMenuButtonData &ActionButton, ETargetType TargetType);
    /** Two-button Allies / Enemies view shown before the per-actor picker when
     *  the action's TargetType is SingleAnyone. Each button carries the narrowed
     *  ETargetType (SingleAlly / SingleEnemy) in DataIndex. */
    TArray<FPieMenuButtonData> BuildTargetCategoryButtons() const;
    /** Exit the picker back to the originating action menu (Submenu or Main):
     *  resets infusion, cancels the pending action, clears ResolvedTargetType. */
    void ExitTargetSelectionToActionMenu();
    TArray<FPieMenuButtonData> BuildTargetButtons(const TArray<AActor *> &Targets) const;
    TArray<FPieMenuButtonData> BuildGroupTargetButtons(ETargetType TargetType, const TArray<AActor *> &Targets) const;
    /** Rebuild the active target picker using ResolvedTargetType to pick between
     *  the per-actor picker (single targets) and the group confirm picker
     *  (auto-resolve targets: Self / AllEnemies / AllAllies / Everyone).
     *  ResolvedTargetType (not PendingTargetType) so a post-category-step picker
     *  rebuilds the narrowed single-team list, not the SingleAnyone set.
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
