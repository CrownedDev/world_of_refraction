// FRingInventoryEntry.h
// Ring inventory entry with unified crystal system
//
// ARCHITECTURE: Runtime inventory instance, separate from RingData asset
// RingData = immutable template (spells, tier, default crystal for editor preview)
// FRingInventoryEntry = mutable runtime state (ACTUAL attached crystal + custom spells)
//
// IMPORTANT: When checking crystal state at runtime, use FRingInventoryEntry.AttachedCrystal,
// NOT RingData.SlottedCrystal. The data asset may have a default crystal for editor
// testing, but the inventory entry is the runtime truth.
//
// NOTE: Rings require a crystal to function. An entry with no crystal
// is an "empty ring shell" - it takes inventory space but cannot be equipped for combat.

#pragma once

#include "CoreMinimal.h"
#include "InventoryConstants.h"
#include "ESpellElement.h"
#include "FCrystalInventoryEntry.h"

#include "FRingInventoryEntry.generated.h"

class URingData;
class UItemData;
class USpellData;

/**
 * FRingInventoryEntry
 * Represents a single ring INSTANCE in inventory with its attached crystal state
 * Slot cost: Base=1 (with crystal), Evolved=2
 *
 * This is RUNTIME STATE - the actual crystal attached to this specific ring instance.
 * Multiple characters can reference the same RingData but have different crystals attached.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FRingInventoryEntry
{
    GENERATED_BODY()

    /** The ring data asset (immutable template) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring")
    URingData *Ring = nullptr;

    /** Attached crystal with runtime spell customization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring")
    FCrystalInventoryEntry AttachedCrystal;

    /** Spells assigned to this ring's crystal slots (lost when crystal removed) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring")
    TArray<USpellData *> AssignedSpells;

    // ==================== FACTORY ====================

    /** Create entry from RingData, optionally copying default crystal */
    static FRingInventoryEntry CreateFromRing(URingData *InRing, bool bCopyDefaultCrystal = false);

    // ==================== STATE QUERIES ====================

    /** Check if entry is valid (has ring) */
    bool IsValid() const
    {
        return Ring != nullptr;
    }

    /** Check if ring has a crystal (required for combat use) */
    bool HasCrystal() const
    {
        return AttachedCrystal.IsValid();
    }

    /** Check if ring can be used in combat (has valid crystal) */
    bool CanBeEquipped() const
    {
        return IsValid() && HasCrystal();
    }

    /** Check if ring is evolved (crystal grants evolution) */
    bool IsEvolved() const
    {
        return AttachedCrystal.GrantsEvolution();
    }

    /** Get slot cost based on current state */
    int32 GetSlotCost() const
    {
        return InventoryConstants::GetRingSlotCost(IsEvolved());
    }

    /** Get ring's element from attached crystal. Returns Generic for missing
     *  OR broken crystals — a broken crystal cannot channel its element.
     *  Body in FRingInventoryEntry.cpp because it reads UItemData fields
     *  via the attached crystal entry (full type only forward-declared here). */
    ESpellElement GetElement() const;

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

    /** Check if ring has stat modifiers (from evolution crystal) */
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

    bool operator==(const FRingInventoryEntry &Other) const
    {
        return Ring == Other.Ring && AttachedCrystal == Other.AttachedCrystal;
    }

    bool operator!=(const FRingInventoryEntry &Other) const
    {
        return !(*this == Other);
    }
};
