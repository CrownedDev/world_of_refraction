// InventoryComponent.h
// Character inventory storage and management
//
// ARCHITECTURE:
// This component stores all character OWNERSHIP - what spells, abilities,
// weapons, rings, and items a character possesses.
//
// Loadout configuration (how items are equipped for combat) is separate.
// This is the "warehouse", LoadoutComponent is the "battle gear".

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryConstants.h"
#include "FSpellCollection.h"
#include "FAbilityCollection.h"
#include "FWeaponInventoryEntry.h"
#include "FRingInventoryEntry.h"
#include "FItemCrystalInventory.h"
#include "InventoryComponent.generated.h"

class USpellData;
class UAbilityData;
class UWeaponData;
class URingData;
class UItemData;
class UEvolutionData;

/**
 * UInventoryComponent
 * Manages all character inventory (what they OWN)
 * 
 * Attach to any actor that needs inventory (player characters, party members)
 * Provides validation, capacity management, and Blueprint access
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInventoryComponent();

    // ==================== INVENTORY DATA ====================

    /** Learned spells (max 50) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Spells")
    FSpellCollection Spells;

    /** Learned abilities (max 50) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Abilities")
    FAbilityCollection Abilities;

    /** Owned weapons with crystal/evolution state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Weapons")
    TArray<FWeaponInventoryEntry> Weapons;

    /** Owned rings with crystal/evolution state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Rings")
    TArray<FRingInventoryEntry> Rings;

    /** Consumable item crystals (tiered capacity) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Items")
    FItemCrystalInventory Items;

    /** Evolution crystals (max 10) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Evolution")
    TArray<UEvolutionData*> EvolutionCrystals;

    // ==================== SPELL OPERATIONS ====================

    /** Learn a new spell */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Spells")
    bool LearnSpell(USpellData* Spell);

    /** Unlearn a spell */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Spells")
    bool UnlearnSpell(USpellData* Spell);

    /** Check if spell is known */
    UFUNCTION(BlueprintPure, Category = "Inventory|Spells")
    bool HasSpell(USpellData* Spell) const;

    /** Get spells by element */
    UFUNCTION(BlueprintPure, Category = "Inventory|Spells")
    TArray<USpellData*> GetSpellsByElement(ESpellElement Element) const;

    // ==================== ABILITY OPERATIONS ====================

    /** Learn a new ability */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Abilities")
    bool LearnAbility(UAbilityData* Ability);

    /** Unlearn an ability */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Abilities")
    bool UnlearnAbility(UAbilityData* Ability);

    /** Check if ability is known */
    UFUNCTION(BlueprintPure, Category = "Inventory|Abilities")
    bool HasAbility(UAbilityData* Ability) const;

    /** Get abilities for weapon type */
    UFUNCTION(BlueprintPure, Category = "Inventory|Abilities")
    TArray<UAbilityData*> GetAbilitiesForWeaponType(EWeaponType WeaponType) const;

    // ==================== WEAPON OPERATIONS ====================

    /** Add a weapon to inventory */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weapons")
    bool AddWeapon(UWeaponData* Weapon, bool bCopyDefaultCrystal = false);

    /** Remove a weapon from inventory */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weapons")
    bool RemoveWeapon(int32 WeaponIndex);

    /** Get weapon count */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weapons")
    int32 GetWeaponCount() const { return Weapons.Num(); }

    /** Get weapon at index */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weapons")
    FWeaponInventoryEntry GetWeaponAt(int32 Index) const;

    /** Get total weapon slot cost */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weapons")
    int32 GetWeaponSlotCostTotal() const;

    /** Get remaining weapon capacity */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weapons")
    int32 GetRemainingWeaponCapacity() const;

    /** Can add weapon (checks capacity) */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weapons")
    bool CanAddWeapon(UWeaponData* Weapon) const;

    // ==================== WEAPON CRYSTAL OPERATIONS ====================

    /** Attach crystal to weapon */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weapons")
    bool AttachCrystalToWeapon(int32 WeaponIndex, UItemData* Crystal);

    /** Remove crystal from weapon (returns removed crystal) */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weapons")
    UItemData* RemoveCrystalFromWeapon(int32 WeaponIndex);

    /** Apply evolution to weapon */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weapons")
    bool ApplyEvolutionToWeapon(int32 WeaponIndex, UEvolutionData* Evolution);

    // ==================== RING OPERATIONS ====================

    /** Add a ring to inventory */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Rings")
    bool AddRing(URingData* Ring, bool bCopyDefaultCrystal = false);

    /** Remove a ring from inventory */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Rings")
    bool RemoveRing(int32 RingIndex);

    /** Get ring count */
    UFUNCTION(BlueprintPure, Category = "Inventory|Rings")
    int32 GetRingCount() const { return Rings.Num(); }

    /** Get ring at index */
    UFUNCTION(BlueprintPure, Category = "Inventory|Rings")
    FRingInventoryEntry GetRingAt(int32 Index) const;

    /** Get total ring slot cost */
    UFUNCTION(BlueprintPure, Category = "Inventory|Rings")
    int32 GetRingSlotCostTotal() const;

    /** Get remaining ring capacity */
    UFUNCTION(BlueprintPure, Category = "Inventory|Rings")
    int32 GetRemainingRingCapacity() const;

    /** Can add ring (checks capacity) */
    UFUNCTION(BlueprintPure, Category = "Inventory|Rings")
    bool CanAddRing(URingData* Ring) const;

    // ==================== RING CRYSTAL OPERATIONS ====================

    /** Attach crystal to ring */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Rings")
    bool AttachCrystalToRing(int32 RingIndex, UItemData* Crystal);

    /** Remove crystal from ring (returns removed crystal) */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Rings")
    UItemData* RemoveCrystalFromRing(int32 RingIndex);

    /** Apply evolution to ring */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Rings")
    bool ApplyEvolutionToRing(int32 RingIndex, UEvolutionData* Evolution);

    // ==================== ITEM OPERATIONS ====================

    /** Add item crystal to inventory */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    bool AddItem(UItemData* Item);

    /** Remove item crystal from inventory */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    bool RemoveItem(UItemData* Item);

    /** Check if item is in inventory */
    UFUNCTION(BlueprintPure, Category = "Inventory|Items")
    bool HasItem(UItemData* Item) const;

    /** Get items by type */
    UFUNCTION(BlueprintPure, Category = "Inventory|Items")
    TArray<UItemData*> GetItemsByType(ECrystalType Type) const;

    /** Get items by tier */
    UFUNCTION(BlueprintPure, Category = "Inventory|Items")
    TArray<UItemData*> GetItemsByTier(EItemTier Tier) const;

    // ==================== EVOLUTION OPERATIONS ====================

    /** Add evolution crystal */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Evolution")
    bool AddEvolutionCrystal(UEvolutionData* Evolution);

    /** Remove evolution crystal */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Evolution")
    bool RemoveEvolutionCrystal(UEvolutionData* Evolution);

    /** Check if evolution crystal is owned */
    UFUNCTION(BlueprintPure, Category = "Inventory|Evolution")
    bool HasEvolutionCrystal(UEvolutionData* Evolution) const;

    /** Get evolution crystals by element */
    UFUNCTION(BlueprintPure, Category = "Inventory|Evolution")
    TArray<UEvolutionData*> GetEvolutionCrystalsByElement(ESpellElement Element) const;

    // ==================== UTILITY ====================

    /** Clear all inventory */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void ClearAll();

    /** Get inventory summary for debug */
    UFUNCTION(BlueprintPure, Category = "Inventory|Debug")
    FString GetInventorySummary() const;

protected:
    virtual void BeginPlay() override;

#if WITH_EDITOR
    /** Context menu debug */
    UFUNCTION(CallInEditor, Category = "Debug")
    void DebugLogInventory();
#endif
};
