// CombatCommandMenuSubsystem.h
// Drives the combat command menu. Builds capabilities from LoadoutComponent,
// exposes button arrays to Blueprint via delegates, handles selection routing.
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/Combat/FCombatCapabilities.h"
#include "UI/Combat/PieMenuButtonData.h"
#include "ECharacterClass.h"
#include "ElementColors.h"
#include "TargetType.h"
#include "CombatCommandMenuSubsystem.generated.h"

class ULoadoutComponent;
class UCharacterData;
class USpellData;

// ==================== DELEGATES ====================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCommandMenuReady,
                                            const TArray<FPieMenuButtonData> &, Buttons);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionSelected,
                                            const FPieMenuButtonData &, ButtonData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCommandMenuClosed);

// ==================== MENU DEPTH ====================

enum class ECombatMenuDepth : uint8
{
    Closed,
    Main,
    Submenu,
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
    FOnActionSelected OnActionSelected;

    UPROPERTY(BlueprintAssignable, Category = "Combat Command Menu|Events")
    FOnCommandMenuClosed OnCommandMenuClosed;

    // ==================== STATE QUERIES ====================

    UFUNCTION(BlueprintPure, Category = "Combat Command Menu")
    bool IsOpen() const { return bIsOpen; }

    UFUNCTION(BlueprintPure, Category = "Combat Command Menu")
    bool IsAtMainMenu() const { return CurrentDepth == ECombatMenuDepth::Main; }

    UFUNCTION(BlueprintPure, Category = "Combat Command Menu")
    const FCombatCapabilities &GetCurrentCapabilities() const { return CurrentCapabilities; }

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
    ECombatMenuDepth DepthBeforeTargetSelection = ECombatMenuDepth::Main;
    TWeakObjectPtr<AActor> CurrentActor;

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
    FPieMenuButtonData CreateInfuseButton() const;
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

    void ExecuteAttack();
    void OpenAbilitySubmenu();
    void OpenSpellSubmenu(EPieMenuCategory Source);
    void ExecuteSwitchWeapon();
    void ExecuteSwitchRing();
    void OpenItemsSubmenu();

    // Target selection
    void OpenTargetSelection(EPieMenuCategory ActionCategory, const FPieMenuButtonData &ActionButton, ETargetType TargetType);
    TArray<FPieMenuButtonData> BuildTargetButtons(const TArray<AActor *> &Targets) const;
    TArray<AActor *> ResolveTargets(ETargetType TargetType) const;
    void ConfirmActionWithTarget(AActor *SelectedTarget);

    // ==================== HELPERS ====================

    ULoadoutComponent *GetLoadoutComponent() const;
    FLinearColor GetElementColor(int32 ElementIndex) const;
};