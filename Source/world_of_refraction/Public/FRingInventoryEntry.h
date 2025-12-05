// FRingInventoryEntry.h
// Ring inventory entry with crystal and evolution state
//
// ARCHITECTURE: Runtime inventory instance, separate from RingData asset
// RingData = immutable template (spells, tier, default crystal for editor preview)
// FRingInventoryEntry = mutable runtime state (ACTUAL attached crystal, evolution)
//
// IMPORTANT: When checking crystal state at runtime, use FRingInventoryEntry fields,
// NOT RingData.SlottedCrystal. The data asset may have a default crystal for editor
// testing, but the inventory entry is the runtime truth.
//
// NOTE: Rings require a crystal to function. An entry with AttachedCrystal = nullptr
// is an "empty ring shell" - it takes inventory space but cannot be equipped for combat.

#pragma once

#include "CoreMinimal.h"
#include "InventoryConstants.h"
#include "SpellElement.h"
#include "FRingInventoryEntry.generated.h"

class URingData;
class UItemData;
class UEvolutionData;

/**
 * FRingInventoryEntry
 * Represents a single ring INSTANCE in inventory with its attached crystal/evolution state
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
    URingData* Ring = nullptr;

    /** Attached crystal - RUNTIME STATE (nullptr = empty ring, cannot be used in combat) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring")
    UItemData* AttachedCrystal = nullptr;

    /** Evolution data - RUNTIME STATE (nullptr = not evolved) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring")
    UEvolutionData* Evolution = nullptr;

    // ==================== FACTORY ====================

    /** Create entry from RingData, optionally copying default crystal */
    static FRingInventoryEntry CreateFromRing(URingData* InRing, bool bCopyDefaultCrystal = false);

    // ==================== STATE QUERIES ====================

    /** Check if entry is valid (has ring) */
    bool IsValid() const
    {
        return Ring != nullptr;
    }

    /** Check if ring has a crystal (required for combat use) */
    bool HasCrystal() const
    {
        return AttachedCrystal != nullptr;
    }

    /** Check if ring can be used in combat (has crystal) */
    bool CanBeEquipped() const
    {
        return IsValid() && HasCrystal();
    }

    /** Check if ring is evolved */
    bool IsEvolved() const
    {
        return Evolution != nullptr;
    }

    /** Get slot cost based on current state */
    int32 GetSlotCost() const
    {
        return InventoryConstants::GetRingSlotCost(IsEvolved());
    }

    /** Get ring's element from attached crystal or evolution */
    ESpellElement GetElement() const;

    // ==================== CRYSTAL OPERATIONS ====================

    /** Attach a crystal (returns old crystal if replaced) */
    UItemData* AttachCrystal(UItemData* NewCrystal)
    {
        UItemData* OldCrystal = AttachedCrystal;
        AttachedCrystal = NewCrystal;
        return OldCrystal; // Caller responsible for handling old crystal
    }

    /** Remove crystal (returns the removed crystal) */
    UItemData* RemoveCrystal()
    {
        UItemData* OldCrystal = AttachedCrystal;
        AttachedCrystal = nullptr;
        return OldCrystal;
    }

    // ==================== EVOLUTION OPERATIONS ====================

    /** Apply evolution (replaces existing crystal) */
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

    bool operator==(const FRingInventoryEntry& Other) const
    {
        return Ring == Other.Ring && 
               AttachedCrystal == Other.AttachedCrystal && 
               Evolution == Other.Evolution;
    }
};
