// FCrystalInventoryEntry.h
// Runtime crystal inventory entry - metadata wrapper
//
// ARCHITECTURE:
// UItemData = Immutable template (crystal type, tier, evolution data, stat mods)
// FCrystalInventoryEntry = Runtime state (which crystal is attached)
//
// Spells live on FWeaponInventoryEntry/FRingInventoryEntry, NOT here.

#pragma once

#include "CoreMinimal.h"
#include "ESpellElement.h"
#include "ECrystalCategory.h"
#include "FCrystalInventoryEntry.generated.h"

class UItemData;

/**
 * FCrystalInventoryEntry
 * Wraps a crystal (UItemData) with runtime-assigned custom spells
 * Used by FWeaponInventoryEntry, FRingInventoryEntry, and CharacterData
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FCrystalInventoryEntry
{
    GENERATED_BODY()

    /** The crystal data asset */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal")
    UItemData *Crystal = nullptr;

    // ==================== FACTORY ====================

    /** Create entry from crystal */
    static FCrystalInventoryEntry CreateFromCrystal(UItemData *InCrystal);

    // ==================== VALIDATION ====================

    /** Check if entry has a valid crystal */
    bool IsValid() const { return Crystal != nullptr; }

    /** Check if crystal grants evolution status */
    bool GrantsEvolution() const;

    /** Check if crystal can have spells (Refined or Evolution) */
    bool CanHaveSpells() const;

    /** Check if crystal is refined (non-evolution slottable) */
    bool IsRefined() const;

    /** Validate entry - crystal valid, custom spells within limits */
    bool Validate() const;

    // ==================== ELEMENT ACCESS ====================

    /** Get element from crystal */
    ESpellElement GetElement() const;

    // ==================== STAT MODIFIERS (Evolution only) ====================

    /** Check if crystal has stat modifiers */
    bool HasStatModifiers() const;

    /** Get stat modifier summary string */
    FString GetStatModifierSummary() const;

    /** Get Mind modifier percent */
    float GetMindModifierPercent() const;

    /** Get Body modifier percent */
    float GetBodyModifierPercent() const;

    /** Get Spirit modifier percent */
    float GetSpiritModifierPercent() const;

    // ==================== COMPARISON ====================

    bool operator==(const FCrystalInventoryEntry &Other) const
    {
        return Crystal == Other.Crystal;
    }

    bool operator!=(const FCrystalInventoryEntry &Other) const
    {
        return !(*this == Other);
    }
};
