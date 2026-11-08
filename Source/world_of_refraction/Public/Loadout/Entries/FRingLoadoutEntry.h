// FRingLoadoutEntry.h
// Ring loadout entry - ring equipped for combat
//
// ARCHITECTURE:
// FRingInventoryEntry = What you OWN (ring + crystal state + assigned spells)
// FRingLoadoutEntry = How you USE IT (just wraps the inventory entry for combat)

#pragma once

#include "CoreMinimal.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
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

    /** Number of locked spell slots (from ring's preset).
     *  Mirrors FWeaponLoadoutEntry::GetLockedAbilityCount — returns the full
     *  PresetSpells.Num() when bSpellsLocked is true, else 0 (all customisable). */
    int32 GetLockedSpellCount() const;

    /** Number of customisable spell slots (MAX_RING_SPELLS - locked). */
    int32 GetCustomizableSpellCount() const;

    /** Get all spells (locked + customisable, in that order). Replaces the
     *  earlier delegate-only behaviour; for non-conjured rings this is
     *  identical to RingEntry.GetSpells() since LockedCount is 0. */
    /** bAsIfIntact (B-197): the spells a broken crystal would offer intact, of which it keeps
     *  the F-tier ones castable (D-083); otherwise a broken crystal offers only the presets. */
    TArray<USpellData *> GetAllSpells(bool bAsIfIntact = false) const;

    /** GetAllSpells before the slot cap (B-196). */
    TArray<USpellData *> GetAllSpellsUncapped(bool bAsIfIntact = false) const;

    /** Spell slots the attached gem / evolution (Q-147) or the ring's tier allows. */
    int32 GetSpellSlotCap() const;

    /** Spells the cap drops (0 for a locked ring, whose presets are never capped). */
    int32 GetSpellsOverCap() const;

    // ==================== ONE SPELL-SLOT LIST (B-351d, D-145) ====================

    /** The one spell list slot by slot (null = an empty slot): the pick (while the crystal can give spells, or
     *  bAsIfIntact), else the ring's preset spell at that position; GetSpellSlotCap slots. A locked ring: its presets. */
    TArray<USpellData *> GetSpellSlots(bool bAsIfIntact = false) const;

    /** Replace what Slot holds with Spell; activates the list. False for a locked ring or past the cap. */
    bool ReplaceSpell(int32 Slot, USpellData *Spell);

    /** Bring back the preset (or empty) Slot held before a replace; activates the list. False if nothing was replaced. */
    bool RestoreSpell(int32 Slot);

    /** Debug: the spell list slot by slot and whether it is active. */
    FString GetSpellSlotString() const;

    /** Get only the locked/preset spells (empty when bSpellsLocked is false). */
    TArray<USpellData *> GetLockedSpells() const;

    /** Get only the customisable spells (RingEntry.AssignedSpells). */
    TArray<USpellData *> GetCustomizableSpells() const;

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