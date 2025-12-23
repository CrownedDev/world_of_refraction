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
#include "LoadoutData.h"
#include "LoadoutComponent.generated.h"

class UInventoryComponent;
class USpellData;
class UAbilityData;
class UItemData;
class URingData;
class UItemData;
struct FWeaponLoadoutEntry;
struct FRingLoadoutEntry;
class UStanceData;
class UWeaponAttackData;
class UAnimMontage;
class USpellData;

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

    /** Set active ring index for Resonator */
    void SetActiveRingIndex(int32 NewIndex);

    // ==================== LOADOUT MANAGEMENT ====================

    /** Create a new empty loadout */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Management")
    int32 CreateNewLoadout(const FString &LoadoutName);

    /** Create and populate a new loadout with spells, abilities, and items */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Management")
    int32 CreateAndConfigureLoadout(
        const FString &LoadoutName,
        UInventoryComponent *Inventory,
        const TArray<USpellData *> &SpellsToAdd,
        const TArray<UAbilityData *> &AbilitiesToAdd,
        const TArray<UItemData *> &ItemsToAdd);

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

    // ==================== COMBAT ACCESSORS ====================

    /** Get active weapon based on current loadout and bUsingPrimary state */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    UWeaponData *GetActiveWeapon() const;

    /** Get primary weapon (nullptr if using ring/evolution) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    UWeaponData *GetPrimaryWeapon() const;

    /** Get secondary weapon (Generic only) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    UWeaponData *GetSecondaryWeapon() const;

    /** Get primary ring (Generic/Caster with ring in primary slot) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    URingData *GetPrimaryRing() const;

    /** Get active ring for Resonator (from ring loadout) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    URingData *GetActiveRing() const;

    /** Get primary evolution crystal */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    UItemData *GetPrimaryEvolution() const;

    /** Get primary slot type */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    EPrimarySlotType GetPrimarySlotType() const;

    /** Get secondary slot type */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    ESecondarySlotType GetSecondarySlotType() const;

    /** Is currently using primary equipment? */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    bool IsUsingPrimary() const;

    /** Switch between primary/secondary (Generic) or armed/unarmed (Caster/Resonator) */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Combat")
    void ToggleEquipment();

    /** Check if character has weapon access in current state */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    bool HasWeaponAccess() const;

    /** Check if character is currently armed */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    bool IsArmed() const;

    /** Check if current loadout has evolution */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    bool IsEvolved() const;

    // ==================== EQUIPMENT STATE HELPERS ====================

    /** Check if character has secondary equipment (Generic only) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    bool HasSecondaryEquipment() const;

    /** Check if secondary slot has a ring (Generic only) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    bool HasRingInSecondary() const;

    // ==================== STANCE & ANIMATION ====================

    /** Get current stance based on armed state and active weapon */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    UStanceData *GetCurrentStance() const;

    /** Get current idle montage from stance */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    UAnimMontage *GetCurrentIdleMontage() const;

    /** Get current attack data from active weapon */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    UWeaponAttackData *GetCurrentAttack() const;

    /** Get current attack montage */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    UAnimMontage *GetCurrentAttackMontage() const;

    // ==================== SPELL ACCESS ====================

    /** Get all combat spells from active loadout */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    TArray<USpellData *> GetCombatSpells() const;

    /** Get spells from evolved weapon crystal */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    TArray<USpellData *> GetWeaponResonateSpells() const;

    /** Get spells from ring (Generic secondary or Caster primary) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    TArray<USpellData *> GetRingResonateSpells() const;

    // ==================== LOADOUT ENTRY ACCESSORS ====================

    /** Get active weapon loadout entry (full access to crystal, abilities, spells) */
    const FWeaponLoadoutEntry *GetActiveWeaponLoadout() const;

    /** Get primary weapon loadout entry */
    const FWeaponLoadoutEntry *GetPrimaryWeaponLoadout() const;

    /** Get secondary weapon loadout entry (Generic only) */
    const FWeaponLoadoutEntry *GetSecondaryWeaponLoadout() const;

    /** Get primary ring loadout entry (Generic/Caster with ring in primary slot) */
    const FRingLoadoutEntry *GetPrimaryRingLoadout() const;

    /** Get active ring loadout entry (Resonator) */
    const FRingLoadoutEntry *GetActiveRingLoadout() const;

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

    /**
     * Initialize from pre-configured LoadoutData asset (AI enemies)
     * Copies asset configuration into SavedLoadouts[0] and sets as active
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Init")
    void InitializeFromAsset(ULoadoutData *LoadoutAsset);

    /** Check if initialized from asset (AI) vs inventory (Player) */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    bool IsAssetBased() const { return bInitializedFromAsset; }

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

private:
    /** True if initialized from LoadoutData asset (AI), false if from inventory (Player) */
    bool bInitializedFromAsset = false;

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