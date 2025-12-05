// FWeaponInventoryEntry.h
// Weapon inventory entry with crystal and evolution state
//
// ARCHITECTURE: Runtime inventory instance, separate from WeaponData asset
// WeaponData = immutable template (stats, mesh, type, default crystal for editor preview)
// FWeaponInventoryEntry = mutable runtime state (ACTUAL attached crystal, evolution)
//
// IMPORTANT: When checking crystal state at runtime, use FWeaponInventoryEntry fields,
// NOT WeaponData.SlottedCrystal. The data asset may have a default crystal for editor
// testing, but the inventory entry is the runtime truth.

#pragma once

#include "CoreMinimal.h"
#include "InventoryConstants.h"
#include "SpellElement.h"
#include "FWeaponInventoryEntry.generated.h"

class UWeaponData;
class UItemData;
class UEvolutionData;

/**
 * FWeaponInventoryEntry
 * Represents a single weapon INSTANCE in inventory with its attached crystal/evolution state
 * Slot cost varies based on attachments: Base=1, Crystal=2, Evolution=3
 * 
 * This is RUNTIME STATE - the actual crystal attached to this specific weapon instance.
 * Multiple characters can reference the same WeaponData but have different crystals attached.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FWeaponInventoryEntry
{
    GENERATED_BODY()

    /** The weapon data asset (immutable template) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UWeaponData* Weapon = nullptr;

    /** Attached crystal - RUNTIME STATE (nullptr = base weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UItemData* AttachedCrystal = nullptr;

    /** Evolution data - RUNTIME STATE (nullptr = not evolved) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UEvolutionData* Evolution = nullptr;

    // ==================== FACTORY ====================

    /** Create entry from WeaponData, optionally copying default crystal */
    static FWeaponInventoryEntry CreateFromWeapon(UWeaponData* InWeapon, bool bCopyDefaultCrystal = false);

    // ==================== STATE QUERIES ====================

    /** Check if entry is valid (has weapon) */
    bool IsValid() const
    {
        return Weapon != nullptr;
    }

    /** Check if weapon has a crystal attached */
    bool HasCrystal() const
    {
        return AttachedCrystal != nullptr;
    }

    /** Check if weapon is evolved */
    bool IsEvolved() const
    {
        return Evolution != nullptr;
    }

    /** Get slot cost based on current state */
    int32 GetSlotCost() const
    {
        return InventoryConstants::GetWeaponSlotCost(HasCrystal(), IsEvolved());
    }

    /** Get weapon's current element (from crystal or evolution) */
    ESpellElement GetElement() const;

    /** Check if weapon can cast spells (has crystal or evolution) */
    bool CanCastSpells() const
    {
        return HasCrystal() || IsEvolved();
    }

    // ==================== CRYSTAL OPERATIONS ====================

    /** Attach a crystal (returns old crystal if replaced) */
    UItemData* AttachCrystal(UItemData* NewCrystal)
    {
        UItemData* OldCrystal = AttachedCrystal;
        AttachedCrystal = NewCrystal;
        return OldCrystal; // Caller responsible for destroying old crystal
    }

    /** Remove crystal (returns the removed crystal) */
    UItemData* RemoveCrystal()
    {
        UItemData* OldCrystal = AttachedCrystal;
        AttachedCrystal = nullptr;
        return OldCrystal;
    }

    // ==================== EVOLUTION OPERATIONS ====================

    /** Apply evolution crystal (consumes it, replaces existing crystal) */
    bool ApplyEvolution(UEvolutionData* NewEvolution)
    {
        if (!NewEvolution)
        {
            return false;
        }
        
        // Evolution replaces crystal
        AttachedCrystal = nullptr;
        Evolution = NewEvolution;
        return true;
    }

    // ==================== COMPARISON ====================

    bool operator==(const FWeaponInventoryEntry& Other) const
    {
        return Weapon == Other.Weapon && 
               AttachedCrystal == Other.AttachedCrystal && 
               Evolution == Other.Evolution;
    }
};
