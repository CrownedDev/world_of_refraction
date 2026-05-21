// FRingInventoryEntry.h
// Ring inventory entry with unified crystal system
//
// ARCHITECTURE: Runtime inventory instance, separate from RingData asset
// RingData = immutable template (spells, tier, default crystal for editor preview)
// FRingInventoryEntry = mutable runtime state (ACTUAL attached crystal + custom spells)
//
// IMPORTANT: When checking crystal state at runtime, use FRingInventoryEntry.AttachedItem,
// NOT RingData.SlottedCrystal. The data asset may have a default crystal for editor
// testing, but the inventory entry is the runtime truth.
//
// NOTE: Rings require a crystal to function. An entry with no crystal
// is an "empty ring shell" - it takes inventory space but cannot be equipped for combat.

#pragma once

#include "CoreMinimal.h"
#include "InventoryConstants.h"
#include "ESpellElement.h"
#include "FRuntimeAttachedItem.h"
#include "FEquipmentStatBonus.h"

#include "FRingInventoryEntry.generated.h"

class URingData;
class UEvolutionItemData;
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

    /** Stable per-instance identifier. Generated at CreateFromRing time
     *  via a static atomic counter (see FRingInventoryEntry.cpp). Two
     *  entries referencing the same URingData asset get distinct IDs.
     *
     *  Callers of USkillEffectManager::ApplyRingBonuses must pass this
     *  field as RingInstanceID — never a fabricated literal, and never a
     *  hash of the asset name (would collide across duplicate-asset
     *  entries). RemoveRingBonuses must be paired with the same value
     *  used at apply time. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity")
    int32 InstanceID = 0;

    /** Attached refined crystal or evolution item, discriminated by Kind. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring")
    FRuntimeAttachedItem AttachedItem;

    /** Spells assigned to this ring's crystal slots (lost when crystal removed) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring")
    TArray<USpellData *> AssignedSpells;

    /** Per-instance stat bonus state. Seeded from the asset's
     *  GetCombinedStatBonus() (BaseStatBonus + GeneratedStatBonus) at
     *  CreateFromRing time. This is what runtime damage and stat queries
     *  should read — not the asset's BaseStatBonus / GeneratedStatBonus
     *  layers. */
    UPROPERTY(BlueprintReadWrite, Category = "Engravings")
    FEquipmentStatBonus StatBonus;

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
        return !AttachedItem.IsEmpty();
    }

    /** Check if ring can be used in combat (has valid crystal) */
    bool CanBeEquipped() const
    {
        return IsValid() && HasCrystal();
    }

    /** Check if ring is evolved (crystal grants evolution) */
    bool IsEvolved() const
    {
        return AttachedItem.IsEvolution();
    }

    /** Get slot cost based on current state */
    int32 GetSlotCost() const
    {
        return InventoryConstants::GetRingSlotCost(IsEvolved());
    }

    /** Get ring's element from attached crystal. Returns Generic for missing
     *  OR broken crystals — a broken crystal cannot channel its element.
     *  Body in FRingInventoryEntry.cpp. */
    ESpellElement GetElement() const;

    // ==================== SPELL ACCESS ====================

    /** Get assigned spells (only when the attachment can currently provide spells —
     *  excludes empty/broken). Use HasCrystal() for "physically slotted regardless of state". */
    TArray<USpellData *> GetSpells() const
    {
        return AttachedItem.CanProvideSpells() ? AssignedSpells : TArray<USpellData *>();
    }

    /** Get spell count (returns 0 for empty/broken attachments) */
    int32 GetSpellCount() const
    {
        return AttachedItem.CanProvideSpells() ? AssignedSpells.Num() : 0;
    }
    // ==================== CRYSTAL OPERATIONS ====================

    /** Attach a crystal — branches refined vs evolution via FRuntimeAttachedItem::FromAsset. */
    void AttachCrystal(UEvolutionItemData *NewCrystal)
    {
        AttachedItem = FRuntimeAttachedItem::FromAsset(NewCrystal);
    }

    /** Remove crystal (clears spells too - vendor must reassign) */
    void RemoveCrystal()
    {
        AttachedItem = FRuntimeAttachedItem();
        AssignedSpells.Empty();
    }

    /** Get direct access to the attached item for spell customization */
    FRuntimeAttachedItem &GetAttachedItem()
    {
        return AttachedItem;
    }

    const FRuntimeAttachedItem &GetAttachedItem() const
    {
        return AttachedItem;
    }

    // ==================== STAT MODIFIERS (Evolution only) ====================

    /** Check if ring has stat modifiers (from evolution crystal) */
    bool HasStatModifiers() const
    {
        return AttachedItem.HasStatModifiers();
    }

    /** Get stat modifier summary */
    FString GetStatModifierSummary() const
    {
        return AttachedItem.GetStatModifierSummary();
    }

    // ==================== COMPARISON ====================

    bool operator==(const FRingInventoryEntry &Other) const
    {
        return Ring == Other.Ring && AttachedItem == Other.AttachedItem;
    }

    bool operator!=(const FRingInventoryEntry &Other) const
    {
        return !(*this == Other);
    }
};
