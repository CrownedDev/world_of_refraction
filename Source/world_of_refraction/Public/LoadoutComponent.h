// LoadoutComponent.h
// Active combat loadout and runtime battle state
//
// ARCHITECTURE:
// InventoryComponent = What you OWN (warehouse)
// LoadoutComponent = What you're USING in battle (equipped gear)
//
// Players can save multiple loadout configurations.
// At battle start, active loadout is validated against inventory.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ECharacterClass.h"
#include "FCombatLoadout.h"
#include "FItemLoadoutSlot.h"
#include "LoadoutComponent.generated.h"

class UInventoryComponent;
class USpellData;
class UAbilityData;
class UItemData;

/** Delegate for loadout changes (query GetActiveLoadout() for data) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadoutChanged, int32, NewLoadoutIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoadoutItemUsed, int32, SlotIndex, UItemData *, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadoutValidationFailed, const FString &, Reason);

/**
 * ULoadoutComponent
 * Manages active combat loadout and runtime battle state
 *
 * Responsibilities:
 * - Store saved loadout configurations
 * - Track active loadout for current battle
 * - Validate loadouts against inventory
 * - Track runtime state (item uses, etc.)
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API ULoadoutComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULoadoutComponent();

    // ==================== CONFIGURATION ====================

    /** Character class (determines loadout rules) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Config")
    ECharacterClass CharacterClass = ECharacterClass::Generic;

    /** Maximum saved loadouts per character */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Config")
    int32 MaxSavedLoadouts = 5;

    // ==================== SAVED LOADOUTS ====================

    /** Saved loadout configurations (C++ access - not serialized via UPROPERTY) */
    TArray<FCombatLoadout> SavedLoadouts;

    /** Index of currently active loadout */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Saved")
    int32 ActiveLoadoutIndex = 0;

    // ==================== ACTIVE LOADOUT ====================

    /** Get the currently active loadout (C++ only - use specific getters for Blueprint) */
    FCombatLoadout GetActiveLoadout() const;

    /** Get active loadout by reference (C++ only) */
    bool GetActiveLoadoutRef(FCombatLoadout &OutLoadout) const;

    /** Set active loadout index */
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    bool SetActiveLoadoutIndex(int32 Index);

    /** Get active loadout index */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    int32 GetActiveLoadoutIndex() const { return ActiveLoadoutIndex; }

    /** Get active loadout name */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    FString GetActiveLoadoutName() const;

    // ==================== LOADOUT MANAGEMENT ====================

    /** Create a new empty loadout */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Management")
    int32 CreateNewLoadout(const FString &LoadoutName);

    /** Delete a saved loadout */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Management")
    bool DeleteLoadout(int32 Index);

    /** Duplicate a loadout */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Management")
    int32 DuplicateLoadout(int32 SourceIndex, const FString &NewName);

    /** Rename a loadout */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Management")
    bool RenameLoadout(int32 Index, const FString &NewName);

    /** Get loadout count */
    UFUNCTION(BlueprintPure, Category = "Loadout|Management")
    int32 GetLoadoutCount() const { return SavedLoadouts.Num(); }

    /** Get loadout names for UI */
    UFUNCTION(BlueprintPure, Category = "Loadout|Management")
    TArray<FString> GetLoadoutNames() const;

    // ==================== VALIDATION ====================

    /** Validate active loadout against inventory */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Validation")
    bool ValidateActiveLoadout(UInventoryComponent *Inventory);

    /** Validate specific loadout against inventory */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Validation")
    bool ValidateLoadout(int32 Index, UInventoryComponent *Inventory);

    /** Get validation errors for loadout */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Validation")
    TArray<FString> GetValidationErrors(int32 Index, UInventoryComponent *Inventory) const;

    // ==================== BATTLE PREPARATION ====================

    /** Prepare loadout for battle (reset item uses, validate) */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Battle")
    bool PrepareForBattle(UInventoryComponent *Inventory);

    /** Check if loadout is ready for battle */
    UFUNCTION(BlueprintPure, Category = "Loadout|Battle")
    bool IsReadyForBattle() const { return bIsReadyForBattle; }

    // ==================== RUNTIME COMBAT STATE ====================

    /** Use an item from loadout (decrements uses) */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Combat")
    bool UseItem(int32 SlotIndex);

    /** Get remaining uses for item slot */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    int32 GetItemRemainingUses(int32 SlotIndex) const;

    /** Get all available abilities from active loadout */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    TArray<UAbilityData *> GetAvailableAbilities() const;

    /** Get all available spells from active loadout */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    TArray<USpellData *> GetAvailableSpells() const;

    /** Get usable item slots (C++ only - use GetUsableItemCount for Blueprint) */
    TArray<FItemLoadoutSlot> GetUsableItems() const;

    /** Get count of usable item slots (Blueprint friendly) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    int32 GetUsableItemCount() const;

    // ==================== POST-BATTLE ====================

    /** Consume items from inventory based on uses during battle */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Battle")
    void ConsumeUsedItems(UInventoryComponent *Inventory);

    /** Get items that will be consumed after battle */
    UFUNCTION(BlueprintPure, Category = "Loadout|Battle")
    TArray<UItemData *> GetItemsToConsume() const;

    /** Reset battle state (call after battle ends) */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Battle")
    void ResetBattleState();

    // ==================== QUICK SETUP ====================

    /** Initialize loadout from CharacterData and Inventory */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Setup")
    void InitializeFromCharacterData(UCharacterData *CharacterData, UInventoryComponent *Inventory);

    /** Auto-populate loadout from inventory (best available) */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Setup")
    bool AutoPopulateLoadout(int32 LoadoutIndex, UInventoryComponent *Inventory);

    /** Clear all equipment from loadout */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Setup")
    void ClearLoadout(int32 LoadoutIndex);

    // ==================== EVENTS ====================

    /** Fired when active loadout changes (use GetActiveLoadout() to get data) */
    UPROPERTY(BlueprintAssignable, Category = "Loadout|Events")
    FOnLoadoutChanged OnLoadoutChanged;

    /** Fired when item is used in combat */
    UPROPERTY(BlueprintAssignable, Category = "Loadout|Events")
    FOnLoadoutItemUsed OnItemUsed;

    /** Fired when loadout validation fails */
    UPROPERTY(BlueprintAssignable, Category = "Loadout|Events")
    FOnLoadoutValidationFailed OnValidationFailed;

protected:
    virtual void BeginPlay() override;

    /** Is loadout ready for battle? */
    UPROPERTY(BlueprintReadOnly, Category = "Loadout|Runtime")
    bool bIsReadyForBattle = false;

    /** Ensure at least one loadout exists */
    void EnsureDefaultLoadout();

#if WITH_EDITOR
    /** Debug log loadout state */
    UFUNCTION(CallInEditor, Category = "Debug")
    void DebugLogLoadout();
#endif
};