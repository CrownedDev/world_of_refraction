// FRingLoadoutEntry.h
// Ring loadout entry - ring equipped for combat with assigned spells
//
// ARCHITECTURE:
// FRingInventoryEntry = What you OWN (ring + crystal state)
// FRingLoadoutEntry = How you USE IT (which spells assigned)
//
// Same ring can have different loadout configurations (multiple saved loadouts)

#pragma once

#include "CoreMinimal.h"
#include "LoadoutConstants.h"
#include "FRingInventoryEntry.h"
#include "FRingLoadoutEntry.generated.h"

class USpellData;

/**
 * FRingLoadoutEntry
 * Ring configured for combat with assigned spells
 * 
 * Spell slots: 6 total
 * Evolved rings may have locked spell slots (from EvolutionData)
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FRingLoadoutEntry
{
    GENERATED_BODY()

    /** The ring from inventory (includes crystal/evolution state) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring")
    FRingInventoryEntry RingEntry;

    /** Assigned spells (max 6, some may be locked if evolved) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spells")
    TArray<USpellData*> AssignedSpells;

    // ==================== VALIDATION ====================

    /** Check if this loadout entry is valid */
    bool IsValid() const
    {
        return RingEntry.IsValid() && RingEntry.CanBeEquipped();
    }

    /** Check if ring is evolved (may have locked spells) */
    bool IsEvolved() const
    {
        return RingEntry.IsEvolved();
    }

    /** Get number of locked spell slots (from evolution) */
    int32 GetLockedSpellCount() const;

    /** Get number of customizable spell slots */
    int32 GetCustomizableSpellCount() const
    {
        return LoadoutConstants::MAX_SPELL_SLOTS - GetLockedSpellCount();
    }

    // ==================== SPELL ACCESS ====================

    /** Get all spells (locked + customizable) */
    TArray<USpellData*> GetAllSpells() const;

    /** Get only the locked/evolution spells */
    TArray<USpellData*> GetLockedSpells() const;

    /** Get only the customizable spells */
    TArray<USpellData*> GetCustomizableSpells() const
    {
        return AssignedSpells;
    }

    /** Check if spell slot is locked */
    bool IsSpellSlotLocked(int32 SlotIndex) const
    {
        return SlotIndex < GetLockedSpellCount();
    }

    /** Get spell count */
    int32 GetSpellCount() const
    {
        return GetAllSpells().Num();
    }

    // ==================== VALIDATION HELPERS ====================

    /** Validate spells match ring element and are owned */
    bool ValidateSpells(const struct FSpellCollection& OwnedSpells) const;

    // ==================== INITIALIZATION ====================

    /** Initialize from ring, setting up for evolution if applicable */
    void InitializeFromRing();

    /** Clear all assignments */
    void Clear()
    {
        RingEntry = FRingInventoryEntry();
        AssignedSpells.Empty();
    }
};
