// FWeaponInventoryEntry.h
// Weapon inventory entry with unified crystal system
//
// ARCHITECTURE: Runtime inventory instance, separate from WeaponData asset
// WeaponData = immutable template (stats, mesh, type, default crystal for editor preview)
// FWeaponInventoryEntry = mutable runtime state (ACTUAL attached crystal + custom spells)
//
// IMPORTANT: When checking crystal state at runtime, use FWeaponInventoryEntry.AttachedCrystal,
// NOT WeaponData.SlottedCrystal. The data asset may have a default crystal for editor
// testing, but the inventory entry is the runtime truth.

#pragma once

#include "CoreMinimal.h"
#include "InventoryConstants.h"
#include "SpellElement.h"
#include "FCrystalInventoryEntry.h"
#include "FWeaponInventoryEntry.generated.h"

class UWeaponData;
class UItemData;
class USpellData;

/**
 * FWeaponInventoryEntry
 * Represents a single weapon INSTANCE in inventory with its attached crystal state
 * Slot cost varies based on crystal: Base=1, Refined=2, Evolution=3
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

    /** Attached crystal with runtime spell customization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FCrystalInventoryEntry AttachedCrystal;

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
        return AttachedCrystal.IsValid();
    }

    /** Check if weapon is evolved (crystal grants evolution) */
    bool IsEvolved() const
    {
        return AttachedCrystal.GrantsEvolution();
    }

    /** Get slot cost based on current state */
    int32 GetSlotCost() const
    {
        if (!HasCrystal())
        {
            return InventoryConstants::WEAPON_BASE_SLOT_COST;
        }
        return InventoryConstants::GetWeaponSlotCost(true, IsEvolved());
    }

    /** Get weapon's current element (from attached crystal) */
    ESpellElement GetElement() const
    {
        if (HasCrystal())
        {
            return AttachedCrystal.GetElement();
        }
        return ESpellElement::Generic;
    }

    /** Check if weapon can cast spells (has crystal with spell capability) */
    bool CanCastSpells() const
    {
        return HasCrystal() && AttachedCrystal.CanHaveSpells();
    }

    // ==================== SPELL ACCESS ====================

    /** Get all spells from attached crystal (locked + custom) */
    TArray<USpellData*> GetSpells() const
    {
        return AttachedCrystal.GetAllSpells();
    }

    /** Get locked spells only (Evolution crystals) */
    TArray<USpellData*> GetLockedSpells() const
    {
        return AttachedCrystal.GetLockedSpells();
    }

    /** Get spell count */
    int32 GetSpellCount() const
    {
        return AttachedCrystal.GetAllSpells().Num();
    }

    // ==================== CRYSTAL OPERATIONS ====================

    /** Attach a crystal (creates new FCrystalInventoryEntry) */
    void AttachCrystal(UItemData* NewCrystal)
    {
        AttachedCrystal = FCrystalInventoryEntry::CreateFromCrystal(NewCrystal);
    }

    /** Remove crystal (clears the entry) */
    void RemoveCrystal()
    {
        AttachedCrystal = FCrystalInventoryEntry();
    }

    /** Get direct access to crystal entry for spell customization */
    FCrystalInventoryEntry& GetCrystalEntry()
    {
        return AttachedCrystal;
    }

    const FCrystalInventoryEntry& GetCrystalEntry() const
    {
        return AttachedCrystal;
    }

    // ==================== STAT MODIFIERS (Evolution only) ====================

    /** Check if weapon has stat modifiers (from evolution crystal) */
    bool HasStatModifiers() const
    {
        return AttachedCrystal.HasStatModifiers();
    }

    /** Get stat modifier summary */
    FString GetStatModifierSummary() const
    {
        return AttachedCrystal.GetStatModifierSummary();
    }

    // ==================== COMPARISON ====================

    bool operator==(const FWeaponInventoryEntry& Other) const
    {
        return Weapon == Other.Weapon && AttachedCrystal == Other.AttachedCrystal;
    }

    bool operator!=(const FWeaponInventoryEntry& Other) const
    {
        return !(*this == Other);
    }
};
