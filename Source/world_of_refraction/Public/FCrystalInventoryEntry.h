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
#include "Misc/Guid.h"
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

    /** Current durability of this specific crystal instance. Initialized
     *  from Crystal->MaxDurability at CreateFromCrystal time. Per-instance
     *  state — wear/repair/break operations write here, not to the asset.
     *
     *  Defaults to 0; CreateFromCrystal seeds it from the asset's MaxDurability.
     *  A 0 here on a crystal with non-zero MaxDurability indicates either
     *  "broken" or "uninitialized" — IsBroken() returns true in both cases. */
    UPROPERTY(BlueprintReadWrite, Category = "Crystal")
    int32 CurrentDurability = 0;

    /** Unique identifier for this specific crystal instance. Generated
     *  at CreateFromCrystal time. Persists across saves, inventory
     *  transfers, and return-to-world operations. Anchors future systems
     *  that need to track specific crystals (trading, drops, scarcity). */
    UPROPERTY(BlueprintReadWrite, Category = "Crystal")
    FGuid InstanceID;

    // ==================== FACTORY ====================

    /** Create entry from crystal */
    static FCrystalInventoryEntry CreateFromCrystal(UItemData *InCrystal);

    // ==================== DURABILITY ====================

    /** True if this crystal instance is broken. Replicates UItemData::IsBroken
     *  semantics: requires the asset to be refined and not immune, plus
     *  per-instance CurrentDurability <= 0. */
    bool IsBroken() const;

    /** Apply wear to this crystal instance. Reduces CurrentDurability by
     *  Amount, clamped to zero. Returns true if this wear broke the crystal
     *  (durability transitioned from non-zero to zero this call). Caller
     *  (UCrystalManager) uses the return value to broadcast the unified
     *  OnCrystalBroken event.
     *
     *  Does NOT broadcast any delegate itself — the entry is data, not events.
     *  Does NOT check bImmuneToBreaking or bIsRefined — caller filters those. */
    bool ApplyWear(int32 Amount);

    /** Repair this crystal between combats. Increases CurrentDurability by
     *  Amount, clamped to Crystal->MaxDurability. No-op if Crystal is null
     *  or Amount <= 0. Returns the actual amount repaired (may be less than
     *  Amount if clamped). */
    int32 RepairBetweenCombats(int32 Amount);

    // ==================== VALIDATION ====================

    /** Check if entry has a valid crystal */
    bool IsValid() const { return Crystal != nullptr; }

    /** Check if crystal grants evolution status */
    bool GrantsEvolution() const;

    /** Check if crystal can have spells (Refined or Evolution) */
    bool CanHaveSpells() const;

    /** Check if crystal can currently provide spells in combat.
     *  True when: crystal exists, supports spells, and is not broken.
     *  Single source of truth for "crystal-gated spell access" — used by
     *  FWeaponInventoryEntry and FRingInventoryEntry GetSpells() paths.
     *  Element queries on broken crystals also fall back to Generic. */
    bool CanProvideSpells() const;

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
