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
#include "ESpellElement.h"
#include "FCrystalInventoryEntry.h"
#include "FEquipmentStatBonus.h"
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
    UWeaponData *Weapon = nullptr;

    /** Stable per-instance identifier. Generated at CreateFromWeapon time
     *  via a static atomic counter (see FWeaponInventoryEntry.cpp). Two
     *  entries referencing the same UWeaponData asset get distinct IDs.
     *
     *  Callers of USkillEffectManager::ApplyWeaponBonuses must pass this
     *  field as WeaponID — never a fabricated literal, and never a hash
     *  of the asset name (would collide across duplicate-asset entries).
     *  RemoveWeaponBonuses must be paired with the same value used at
     *  apply time. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity")
    int32 InstanceID = 0;

    /** Attached crystal with runtime spell customization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FCrystalInventoryEntry AttachedCrystal;

    /** Spells assigned to this weapon's crystal slots (lost when crystal removed) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    TArray<USpellData *> AssignedSpells;

    /** Per-instance stat bonus state. Seeded from
     *  UWeaponData::DefaultStatBonus at CreateFromWeapon time; bLocked is
     *  driven by UWeaponData::bStatBonusLocked. Mutated at runtime by
     *  SpendPendingPoints / AddPendingPoints. This is what runtime damage
     *  and stat queries should read — not the asset's DefaultStatBonus. */
    UPROPERTY(BlueprintReadWrite, Category = "Mastery")
    FEquipmentStatBonus StatBonus;

    // ==================== FACTORY ====================

    /** Create entry from WeaponData, optionally copying default crystal */
    static FWeaponInventoryEntry CreateFromWeapon(UWeaponData *InWeapon, bool bCopyDefaultCrystal = false);

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

    /** Get assigned spells (only when crystal can currently provide spells —
     *  excludes broken crystals). Use AttachedCrystal.HasCrystal() if you need
     *  "is a crystal physically slotted" regardless of state. */
    TArray<USpellData *> GetSpells() const
    {
        return AttachedCrystal.CanProvideSpells() ? AssignedSpells : TArray<USpellData *>();
    }

    /** Get spell count (returns 0 for broken crystals) */
    int32 GetSpellCount() const
    {
        return AttachedCrystal.CanProvideSpells() ? AssignedSpells.Num() : 0;
    }

    // ==================== CRYSTAL OPERATIONS ====================

    /** Attach a crystal (creates new FCrystalInventoryEntry) */
    void AttachCrystal(UItemData *NewCrystal)
    {
        AttachedCrystal = FCrystalInventoryEntry::CreateFromCrystal(NewCrystal);
    }

    /** Remove crystal (clears spells too - vendor must reassign) */
    void RemoveCrystal()
    {
        AttachedCrystal = FCrystalInventoryEntry();
        AssignedSpells.Empty();
    }

    /** Get direct access to crystal entry for spell customization */
    FCrystalInventoryEntry &GetCrystalEntry()
    {
        return AttachedCrystal;
    }

    const FCrystalInventoryEntry &GetCrystalEntry() const
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

    bool operator==(const FWeaponInventoryEntry &Other) const
    {
        return Weapon == Other.Weapon && AttachedCrystal == Other.AttachedCrystal;
    }

    bool operator!=(const FWeaponInventoryEntry &Other) const
    {
        return !(*this == Other);
    }
};
