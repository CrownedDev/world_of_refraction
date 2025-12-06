// CombatMenuSubsystem.h
// Combat menu logic - generates buttons based on character class
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/Combat/PieMenuButtonData.h"
#include "ECharacterClass.h"
#include "EInfusionSourceOption.h"
#include "LoadoutComponent.h"
#include "CharacterDataComponent.h"
#include "CombatMenuSubsystem.generated.h"

class UCharacterData;
class UWeaponData;
class URingData;
class USpellData;
class UAbilityData;
class UItemData;

// Grid size limits
constexpr int32 MAX_SPELLS_PER_SCHOOL = 6;
constexpr int32 MAX_RINGS = 5;

/**
 * UCombatMenuSubsystem
 *
 * GameInstanceSubsystem that generates menu buttons based on character class.
 *
 * Main Menu by Class:
 *
 * Generic:
 *   - Attack (basic attack with current weapon)
 *   - Abilities (opens ability grid from current weapon)
 *   - Items (opens item grid)
 *   - Switch Weapon (toggles bUsePrimary, refreshes menu)
 *
 * Caster:
 *   - Refractions (opens schools list -> spell grid)
 *   - Items (opens item grid)
 *   - Switch Weapon (toggles armed/unarmed)
 *
 * Resonator:
 *   - ChangeRing (opens ring grid, max 5 rings)
 *   - Resonate (opens schools list -> spell grid from current ring)
 *   - Items (opens item grid)
 *   - Switch Weapon (toggles)
 *
 * Sub-menus:
 *   - Schools: Destruction, Conjuration, Enhancement, Restoration
 *   - Spell Grid: Max 6 spells per school
 *   - Ring Grid: Max 5 rings
 */
UCLASS()
class WORLD_OF_REFRACTION_API UCombatMenuSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ==================== SUBSYSTEM LIFECYCLE ====================

	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	// ==================== MAIN MENU ====================

	/**
	 * Get main menu buttons for a character
	 * Returns different buttons based on character class
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Menu")
	TArray<FPieMenuButtonData> GetMainMenuButtons(UCharacterData *CharacterData);

	/** Get main menu buttons from an actor (uses LoadoutComponent when available) */
	UFUNCTION(BlueprintCallable, Category = "Combat Menu")
	TArray<FPieMenuButtonData> GetMainMenuButtonsForActor(AActor *Actor);

	// ==================== SUB-MENUS ====================

	/**
	 * Get schools list (Caster: from innate spells, Resonator: from current ring)
	 * Only shows schools that have spells equipped
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Menu|Sub-Menus")
	TArray<FPieMenuButtonData> GetSchoolsButtons(UCharacterData *CharacterData);

	/**
	 * Get available infusion sources for current character
	 * Shows sources based on class and loadout
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Menu|Sub-Menus")
	TArray<FPieMenuButtonData> GetInfusionSourceButtons(UCharacterData *CharacterData);

	/**
	 * Get spells for a specific school (max 6)
	 * Caster: from innate spells
	 * Resonator: from current active ring
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Menu|Sub-Menus")
	TArray<FPieMenuButtonData> GetSpellsForSchool(UCharacterData *CharacterData, EPieMenuSpellSchool School);

	/**
	 * Get abilities from current weapon (Generic)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Menu|Sub-Menus")
	TArray<FPieMenuButtonData> GetAbilitiesButtons(UCharacterData *CharacterData);

	/**
	 * Get equipped rings (Resonator only, max 5)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Menu|Sub-Menus")
	TArray<FPieMenuButtonData> GetRingsButtons(UCharacterData *CharacterData);

	/**
	 * Get usable items
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Menu|Sub-Menus")
	TArray<FPieMenuButtonData> GetItemsButtons(UCharacterData *CharacterData);

	// ==================== ACTIONS ====================

	/**
	 * Execute Switch Weapon - toggles bUsePrimary on character
	 * Returns true if switch was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Menu|Actions")
	bool ExecuteSwitchWeapon(UCharacterData *CharacterData);

	/**
	 * Execute ring switch - changes active ring for Resonator
	 * Returns true if switch was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Menu|Actions")
	bool ExecuteRingSwitch(UCharacterData *CharacterData, int32 RingIndex);

	// ==================== SELECTION HANDLING ====================

	/**
	 * Handle main menu button selection
	 * Routes to appropriate sub-menu or executes action
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Menu")
	void HandleButtonSelection(const FPieMenuButtonData &ButtonData);

	// ==================== EVENTS ====================

	/** Broadcast when a sub-menu should open */
	UPROPERTY(BlueprintAssignable, Category = "Combat Menu|Events")
	FOnPieMenuButtonSelected OnOpenSubMenu;

	/** Broadcast when an action should execute (Attack, Spell, Ability, Item) */
	UPROPERTY(BlueprintAssignable, Category = "Combat Menu|Events")
	FOnPieMenuButtonSelected OnExecuteAction;

	/** Broadcast when menu should refresh (after Switch Weapon or ChangeRing) */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuRefreshRequested);
	UPROPERTY(BlueprintAssignable, Category = "Combat Menu|Events")
	FOnMenuRefreshRequested OnMenuRefreshRequested;

	/** Broadcast when menu should close */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuCloseRequested);
	UPROPERTY(BlueprintAssignable, Category = "Combat Menu|Events")
	FOnMenuCloseRequested OnMenuCloseRequested;

	/** Broadcast when menu state changes */
	UPROPERTY(BlueprintAssignable, Category = "Combat Menu|Events")
	FOnPieMenuStateChanged OnStateChanged;

	// ==================== STATE ====================

	/** Current menu state */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Menu|State")
	EPieMenuState CurrentState = EPieMenuState::Closed;

	/** Currently selected school (for spell grid) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Menu|State")
	EPieMenuSpellSchool SelectedSchool = EPieMenuSpellSchool::Destruction;

	/** Current character being displayed */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Menu|State")
	TWeakObjectPtr<UCharacterData> CurrentCharacter;

	/** Current actor (for LoadoutComponent access) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Menu|State")
	TWeakObjectPtr<AActor> CurrentActor;

	// ==================== STATE MANAGEMENT ====================

	/** Set menu state and broadcast change */
	UFUNCTION(BlueprintCallable, Category = "Combat Menu|State")
	void SetMenuState(EPieMenuState NewState);

	/** Navigate back from current state */
	UFUNCTION(BlueprintCallable, Category = "Combat Menu|State")
	void NavigateBack();

	// ==================== DEBUG ====================

	UFUNCTION(BlueprintCallable, Category = "Combat Menu|Debug", meta = (DevelopmentOnly))
	void DebugLogMenuState() const;

private:
	// ==================== MAIN MENU BUILDERS ====================

	void BuildGenericMainMenu(UCharacterData *CharacterData, TArray<FPieMenuButtonData> &OutButtons);
	void BuildCasterMainMenu(UCharacterData *CharacterData, TArray<FPieMenuButtonData> &OutButtons);
	void BuildResonatorMainMenu(UCharacterData *CharacterData, TArray<FPieMenuButtonData> &OutButtons);

	// ==================== BUTTON CREATORS ====================

	FPieMenuButtonData CreateAttackButton(UCharacterData *CharacterData);
	FPieMenuButtonData CreateAbilitiesButton(UCharacterData *CharacterData);
	FPieMenuButtonData CreateRefractionsButton(UCharacterData *CharacterData);
	FPieMenuButtonData CreateChangeRingButton(UCharacterData *CharacterData);
	FPieMenuButtonData CreateResonateButton(UCharacterData *CharacterData);
	FPieMenuButtonData CreateResonateWeaponButton(UCharacterData *CharacterData);
	FPieMenuButtonData CreateResonateRingButton(UCharacterData *CharacterData);
	FPieMenuButtonData CreateBreakthroughButton(UCharacterData *CharacterData);
	FPieMenuButtonData CreateItemsButton(UCharacterData *CharacterData);
	FPieMenuButtonData CreateSwitchWeaponButton(UCharacterData *CharacterData);

	// ==================== HELPERS ====================

	/** Get spells by school from character (handles Caster vs Resonator) */
	TArray<USpellData *> GetSpellsBySchool(UCharacterData *CharacterData, EPieMenuSpellSchool School) const;

	/** Get all spells from character (for school counting) */
	TArray<USpellData *> GetAllSpells(UCharacterData *CharacterData) const;

	/** Get abilities from current weapon */
	TArray<UAbilityData *> GetCurrentWeaponAbilities(UCharacterData *CharacterData) const;

	/** Count spells in a school */
	int32 CountSpellsInSchool(UCharacterData *CharacterData, EPieMenuSpellSchool School) const;

	/** Get element color for button tinting */
	FLinearColor GetElementColor(int32 ElementIndex) const;

	/** Check if character can switch weapons */
	bool CanSwitchWeapon(UCharacterData *CharacterData) const;

	/** Get LoadoutComponent from current actor */
	ULoadoutComponent *GetLoadoutComponent() const;
};
