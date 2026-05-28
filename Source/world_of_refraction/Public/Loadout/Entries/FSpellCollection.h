// FSpellCollection.h
// Spell inventory collection for characters
//
// ARCHITECTURE: Runtime inventory state, separate from SpellData assets
// SpellData = immutable template, FSpellCollection = mutable runtime ownership

#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryConstants.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Definitions/SpellSchool.h"
#include "FSpellCollection.generated.h"

class USpellData;

/**
 * FSpellCollection
 * Manages a character's learned spell inventory (runtime state)
 * Capacity: 50 spells, supports learn/unlearn
 *
 * NOTE: This tracks which spells a character OWNS, not which are EQUIPPED.
 * Loadout system assigns owned spells to equipment slots.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSpellCollection
{
    GENERATED_BODY()

    /** Array of learned spells */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spells")
    TArray<USpellData *> LearnedSpells;

    // ==================== CAPACITY ====================

    /** Check if collection has room for more spells */
    bool CanLearn() const
    {
        return LearnedSpells.Num() < InventoryConstants::MAX_LEARNED_SPELLS;
    }

    /** Get remaining capacity */
    int32 GetRemainingCapacity() const
    {
        return InventoryConstants::MAX_LEARNED_SPELLS - LearnedSpells.Num();
    }

    /** Get current count */
    int32 GetCount() const
    {
        return LearnedSpells.Num();
    }

    /** Get max capacity */
    int32 GetMaxCapacity() const
    {
        return InventoryConstants::MAX_LEARNED_SPELLS;
    }

    // ==================== LEARN/UNLEARN ====================

    /** Learn a new spell (returns false if at capacity or already known) */
    bool LearnSpell(USpellData *Spell)
    {
        if (!Spell || !CanLearn() || HasSpell(Spell))
        {
            return false;
        }
        LearnedSpells.Add(Spell);
        return true;
    }

    /** Unlearn a spell (returns false if not known) */
    bool UnlearnSpell(USpellData *Spell)
    {
        if (!Spell)
        {
            return false;
        }
        return LearnedSpells.Remove(Spell) > 0;
    }

    // ==================== QUERIES ====================

    /** Check if spell is in collection */
    bool HasSpell(USpellData *Spell) const
    {
        return Spell && LearnedSpells.Contains(Spell);
    }

    /** Count occurrences of a spell (for duplication tracking) */
    int32 CountSpell(USpellData *Spell) const
    {
        if (!Spell)
        {
            return 0;
        }
        int32 Count = 0;
        for (const USpellData *S : LearnedSpells)
        {
            if (S == Spell)
            {
                Count++;
            }
        }
        return Count;
    }

    /** Get spells filtered by element */
    TArray<USpellData *> GetSpellsByElement(ESpellElement Element) const;

    /** Get spells filtered by school */
    TArray<USpellData *> GetSpellsBySchool(ESpellSchool School) const;

    /** Get spells filtered by element AND school */
    TArray<USpellData *> GetSpellsByElementAndSchool(ESpellElement Element, ESpellSchool School) const;

    // ==================== CLEAR ====================

    /** Remove all spells */
    void Clear()
    {
        LearnedSpells.Empty();
    }
};
