// FRingLoadoutEntry.h
// Ring loadout entry - ring equipped for combat
//
// ARCHITECTURE:
// FRingInventoryEntry = What you OWN (ring + crystal state + assigned spells)
// FRingLoadoutEntry = How you USE IT (just wraps the inventory entry for combat)

#pragma once

#include "CoreMinimal.h"
#include "LoadoutConstants.h"
#include "FRingInventoryEntry.h"
#include "FRingLoadoutEntry.generated.h"

class USpellData;

/**
 * FRingLoadoutEntry
 * Ring configured for combat - delegates to inventory entry for spells
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FRingLoadoutEntry
{
    GENERATED_BODY()

    /** The ring from inventory (includes crystal/evolution state and assigned spells) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring")
    FRingInventoryEntry RingEntry;

    // ==================== VALIDATION ====================

    /** Check if this loadout entry is valid (has ring) */
    bool IsValid() const
    {
        return RingEntry.Ring != nullptr;
    }

    /** Check if ring is evolved (grants stat traits) */
    bool IsEvolved() const
    {
        return RingEntry.IsEvolved();
    }

    // ==================== SPELL ACCESS ====================

    /** Get all assigned spells (delegates to inventory entry) */
    TArray<USpellData *> GetAllSpells() const
    {
        return RingEntry.GetSpells();
    }

    /** Get spell count (delegates to inventory entry) */
    int32 GetSpellCount() const
    {
        return RingEntry.GetSpellCount();
    }

    // ==================== VALIDATION HELPERS ====================

    /** Validate spells match ring element and are owned */
    bool ValidateSpells(const struct FSpellCollection &OwnedSpells) const;

    // ==================== INITIALIZATION ====================

    /** Initialize from ring (no-op now - spells already on inventory entry) */
    void InitializeFromRing();

    /** Clear all assignments */
    void Clear()
    {
        RingEntry = FRingInventoryEntry();
    }
};