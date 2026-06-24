// FSpellCollection.h
// Spell inventory collection for characters
//
// ARCHITECTURE: Runtime inventory state, separate from SpellData assets
// SpellData = immutable template, FSpellCollection = mutable runtime ownership

#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryConstants.h"
#include "Inventory/ItemTier.h"
#include "Inventory/ItemQuality.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Definitions/SpellSchool.h"
#include "FSpellCollection.generated.h"

class USpellData;

/**
 * FSpellInstance
 * Per-instance owned-spell record (cluster i of the spell-instance arc). Wraps the spell asset
 * with the per-instance Tier/Quality/InstanceID that leveling + leveled dismantle will read.
 *
 * INERT until the equip-chain threading (cluster ii): the equip arrays + cast path still copy
 * .Spell OUT as a bare USpellData*, so combat reads the ASSET tier. Tier/Quality/InstanceID are
 * written at learn time but UNREAD by combat for now. Invalid InstanceID = legacy/asset-fallback.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSpellInstance
{
    GENERATED_BODY()

    /** The owned spell asset. The equip chain copies this out as a bare pointer (cluster i is inert). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Spell")
    USpellData *Spell = nullptr;

    /** Per-instance tier — seeded from the asset at learn time, mutated by leveling (deferred).
     *  UNREAD by combat until cluster ii threads it through the equip chain. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Spell")
    EItemTier Tier = EItemTier::F_Tier;

    /** Per-instance quality — placeholder until the weighted-roll cluster; never mutated by leveling. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Spell")
    EItemQuality Quality = EItemQuality::C_Quality;

    /** Stable per-instance identity, minted at learn time. Invalid (default) = legacy/asset-fallback. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Spell")
    FGuid InstanceID;

    FSpellInstance() = default;

    /** Acquisition ctor — mints the InstanceID. Tier/Quality are seeded by the caller (LearnSpell),
     *  which has the full USpellData type to read Spell->Tier. */
    explicit FSpellInstance(USpellData *InSpell)
        : Spell(InSpell), InstanceID(FGuid::NewGuid()) {}
};

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

    /** Array of learned spells (per-instance — wraps the asset with Tier/Quality/InstanceID). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spells")
    TArray<FSpellInstance> LearnedSpells;

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

    /** Learn a new spell (returns false if at capacity or already known). Body in FSpellCollection.cpp
     *  — it mints the per-instance InstanceID + seeds Tier from Spell->Tier (needs the full USpellData
     *  type, which the forward declaration here can't provide). */
    bool LearnSpell(USpellData *Spell);

    /** Unlearn a spell (returns false if not known). Matches on the wrapped asset (.Spell). */
    bool UnlearnSpell(USpellData *Spell)
    {
        if (!Spell)
        {
            return false;
        }
        return LearnedSpells.RemoveAll([Spell](const FSpellInstance &I) { return I.Spell == Spell; }) > 0;
    }

    // ==================== QUERIES ====================

    /** Check if spell is in collection (by asset). */
    bool HasSpell(USpellData *Spell) const
    {
        return Spell && LearnedSpells.ContainsByPredicate([Spell](const FSpellInstance &I) { return I.Spell == Spell; });
    }

    /** Count occurrences of a spell (for duplication tracking). */
    int32 CountSpell(USpellData *Spell) const
    {
        if (!Spell)
        {
            return 0;
        }
        int32 Count = 0;
        for (const FSpellInstance &I : LearnedSpells)
        {
            if (I.Spell == Spell)
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
