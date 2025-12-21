// FCrystalInventoryEntry.h
// Runtime crystal inventory entry with custom spell support
//
// ARCHITECTURE:
// UItemData = Immutable template (crystal type, tier, locked spells, stat mods)
// FCrystalInventoryEntry = Runtime state (which crystal + player-assigned custom spells)
//
// Spell slot layout for Evolution crystals:
// [0..LockedSpellCount-1] = Locked spells (from ItemData.Spells, unchangeable)
// [LockedSpellCount..5] = Custom spells (player-assigned, stored in CustomSpells)
//
// Refined crystals have no locked spells - all 6 slots are customizable

#pragma once

#include "CoreMinimal.h"
#include "ESpellElement.h"
#include "ECrystalCategory.h"
#include "FCrystalInventoryEntry.generated.h"

class UItemData;
class USpellData;

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

    /** Player-assigned custom spells (fills slots after locked spells) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal")
    TArray<USpellData *> CustomSpells;

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

    // ==================== SPELL ACCESS ====================

    /** Get locked spells from crystal (Evolution only) */
    TArray<USpellData *> GetLockedSpells() const;

    /** Get all spells (locked + custom) */
    TArray<USpellData *> GetAllSpells() const;

    /** Get count of locked spell slots */
    int32 GetLockedSpellCount() const;

    /** Get count of customizable spell slots */
    int32 GetCustomSlotCount() const;

    /** Get total spell capacity (always 6 for Refined/Evolution) */
    int32 GetTotalSpellSlots() const;

    // ==================== SPELL MANAGEMENT ====================

    /** Add a custom spell (returns false if at capacity) */
    bool AddCustomSpell(USpellData *Spell);

    /** Remove a custom spell */
    bool RemoveCustomSpell(USpellData *Spell);

    /** Clear all custom spells */
    void ClearCustomSpells();

    /** Set custom spells directly (validates count) */
    bool SetCustomSpells(const TArray<USpellData *> &Spells);

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
        return Crystal == Other.Crystal && CustomSpells == Other.CustomSpells;
    }

    bool operator!=(const FCrystalInventoryEntry &Other) const
    {
        return !(*this == Other);
    }
};
