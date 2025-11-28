// WBPCommandMenu.h
// Master command menu controller with state machine
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommandMenuTypes.h"
#include "WBPCommandMenu.generated.h"

class UHorizontalBox;
class UWBPCommandList;
class UWBPCommandGrid;
class UCharacterData;
class UCharacterDataComponent;
class USpellData;
class UAbilityData;
class UItemData;
class URingData;
class URingManager;

/**
 * Master command menu controller
 * Manages state machine, input routing, and child widget visibility
 * 
 * State Flow:
 * - Generic: Main → AbilityGrid/ItemGrid
 * - Caster: Main → Schools → SpellGrid / ItemGrid
 * - Resonator: Main → Schools → SpellGrid / RingGrid / ItemGrid
 * 
 * Blueprint should bind:
 * - MenuContainer: HorizontalBox containing child widgets
 * - MainMenuList: WBPCommandList for main options
 * - SchoolsList: WBPCommandList for spell schools
 * - ActionGrid: WBPCommandGrid for spells/abilities/items/rings
 */
UCLASS(Abstract, Blueprintable)
class WORLD_OF_REFRACTION_API UWBPCommandMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	UWBPCommandMenu(const FObjectInitializer& ObjectInitializer);

	// ==================== MENU CONTROL ====================

	/** Open the menu for a character (pass the owning actor) */
	UFUNCTION(BlueprintCallable, Category = "Command Menu")
	void OpenMenu(AActor* InOwningActor);

	/** Close the menu */
	UFUNCTION(BlueprintCallable, Category = "Command Menu")
	void CloseMenu();

	/** Is the menu currently open? */
	UFUNCTION(BlueprintPure, Category = "Command Menu")
	bool IsMenuOpen() const { return CurrentState != ECommandMenuState::Closed; }

	/** Get current menu state */
	UFUNCTION(BlueprintPure, Category = "Command Menu")
	ECommandMenuState GetCurrentState() const { return CurrentState; }

	// ==================== NAVIGATION ====================

	/** Navigate up in current context */
	UFUNCTION(BlueprintCallable, Category = "Command Menu|Navigation")
	void NavigateUp();

	/** Navigate down in current context */
	UFUNCTION(BlueprintCallable, Category = "Command Menu|Navigation")
	void NavigateDown();

	/** Navigate left in current context */
	UFUNCTION(BlueprintCallable, Category = "Command Menu|Navigation")
	void NavigateLeft();

	/** Navigate right in current context */
	UFUNCTION(BlueprintCallable, Category = "Command Menu|Navigation")
	void NavigateRight();

	/** Confirm current selection */
	UFUNCTION(BlueprintCallable, Category = "Command Menu|Navigation")
	void Confirm();

	/** Go back / cancel */
	UFUNCTION(BlueprintCallable, Category = "Command Menu|Navigation")
	void Back();

	// ==================== EVENTS ====================

	/** Called when menu opens */
	UPROPERTY(BlueprintAssignable, Category = "Command Menu|Events")
	FOnCommandMenuOpened OnMenuOpened;

	/** Called when menu closes */
	UPROPERTY(BlueprintAssignable, Category = "Command Menu|Events")
	FOnCommandMenuClosed OnMenuClosed;

	/** Called when state changes */
	UPROPERTY(BlueprintAssignable, Category = "Command Menu|Events")
	FOnMenuStateChanged OnStateChanged;

	/** Called when Attack is selected */
	UPROPERTY(BlueprintAssignable, Category = "Command Menu|Events")
	FOnCommandSelected OnAttackSelected;

	/** Called when a spell is selected for casting */
	UPROPERTY(BlueprintAssignable, Category = "Command Menu|Events")
	FOnSpellSelected OnSpellSelected;

	/** Called when an ability is selected for use */
	UPROPERTY(BlueprintAssignable, Category = "Command Menu|Events")
	FOnAbilitySelected OnAbilitySelected;

	/** Called when an item is selected for use */
	UPROPERTY(BlueprintAssignable, Category = "Command Menu|Events")
	FOnItemSelected OnItemSelected;

	/** Called when a ring is selected (Resonator) */
	UPROPERTY(BlueprintAssignable, Category = "Command Menu|Events")
	FOnRingSelected OnRingSelected;

protected:
	// ==================== UUserWidget ====================

	virtual void NativeConstruct() override;

	// ==================== BLUEPRINT BINDABLE WIDGETS ====================

	/** Container for menu widgets */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Command Menu|Widgets")
	UHorizontalBox* MenuContainer;

	/** Main menu list (Attack/Abilities/Refractions/etc.) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Command Menu|Widgets")
	UWBPCommandList* MainMenuList;

	/** Schools list (Destruction/Conjuration/etc.) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Command Menu|Widgets")
	UWBPCommandList* SchoolsList;

	/** Action grid (Spells/Abilities/Items/Rings) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Command Menu|Widgets")
	UWBPCommandGrid* ActionGrid;

	// ==================== BLUEPRINT IMPLEMENTABLE ====================

	/** Called when state changes - implement for animations */
	UFUNCTION(BlueprintImplementableEvent, Category = "Command Menu|Events")
	void OnStateChangeAnimated(ECommandMenuState OldState, ECommandMenuState NewState);

	/** Called when menu opens - implement for animations */
	UFUNCTION(BlueprintImplementableEvent, Category = "Command Menu|Events")
	void OnMenuOpenAnimated();

	/** Called when menu closes - implement for animations */
	UFUNCTION(BlueprintImplementableEvent, Category = "Command Menu|Events")
	void OnMenuCloseAnimated();

private:
	// ==================== STATE ====================

	/** Current menu state */
	ECommandMenuState CurrentState = ECommandMenuState::Closed;

	/** Owning actor (for subsystem queries) */
	UPROPERTY()
	AActor* OwningActor = nullptr;

	/** Character data (derived from OwningActor's CharacterDataComponent) */
	UPROPERTY()
	UCharacterData* CharacterData = nullptr;

	/** Currently selected spell school (for SpellGrid) */
	ESpellSchool SelectedSchool = ESpellSchool::Destruction;
	
	/** Is a spell school currently selected? */
	bool bHasSelectedSchool = false;

	// ==================== STATE MACHINE ====================

	/** Transition to a new state */
	void SetMenuState(ECommandMenuState NewState);

	/** Update widget visibility based on state */
	void UpdateWidgetVisibility();

	// ==================== POPULATION ====================

	/** Populate main menu based on character class */
	void PopulateMainMenu();

	/** Populate schools list (filters empty schools) */
	void PopulateSchoolsList();

	/** Populate spell grid for selected school */
	void PopulateSpellGrid(ESpellSchool School);

	/** Populate ability grid */
	void PopulateAbilityGrid();

	/** Populate item grid */
	void PopulateItemGrid();

	/** Populate ring grid (Resonator only) */
	void PopulateRingGrid();

	// ==================== DATA QUERIES ====================

	/** Get available spell schools (non-empty) */
	TArray<ESpellSchool> GetAvailableSchools() const;

	/** Get spells for a school */
	TArray<USpellData*> GetSpellsForSchool(ESpellSchool School) const;

	/** Get equipped abilities */
	TArray<UAbilityData*> GetEquippedAbilities() const;

	/** Get usable items */
	TArray<UItemData*> GetUsableItems() const;

	/** Get available rings */
	TArray<URingData*> GetAvailableRings() const;

	/** Get active ring (Resonator) */
	URingData* GetActiveRing() const;

	// ==================== EVENT HANDLERS ====================

	/** Handle main menu button click */
	UFUNCTION()
	void HandleMainMenuClicked(const FCommandButtonData& ButtonData);

	/** Handle schools list button click */
	UFUNCTION()
	void HandleSchoolsClicked(const FCommandButtonData& ButtonData);

	/** Handle action grid button click */
	UFUNCTION()
	void HandleGridClicked(const FCommandButtonData& ButtonData);

	// ==================== HELPERS ====================

	/** Check if character is Generic class */
	bool IsGeneric() const;

	/** Check if character is Caster class */
	bool IsCaster() const;

	/** Check if character is Resonator class */
	bool IsResonator() const;

	/** Check if character has weapon equipped */
	bool HasWeaponEquipped() const;

	/** Get RingManager subsystem */
	URingManager* GetRingManager() const;
};
