// FRingInventoryEntry.h
// Ring inventory entry with unified crystal system
//
// ARCHITECTURE: Runtime inventory instance, separate from RingData asset
// RingData = immutable template (spells, tier, default crystal for editor preview)
// FRingInventoryEntry = mutable runtime state (ACTUAL attached crystal + custom spells)
//
// IMPORTANT: When checking crystal state at runtime, use FRingInventoryEntry.AttachedItem,
// NOT RingData.AttachedItem. The data asset may have a default crystal for editor
// testing, but the inventory entry is the runtime truth.
//
// NOTE: Rings require a crystal to function. An entry with no crystal
// is an "empty ring shell" - it takes inventory space but cannot be equipped for combat.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryConstants.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "Inventory/ItemTier.h"
#include "Inventory/ItemQuality.h"

#include "FRingInventoryEntry.generated.h"

class URingData;
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

    /** Stable per-entry session ID. Generated at CreateFromRing time via a
     *  static atomic counter (see FRingInventoryEntry.cpp); two entries
     *  referencing the same URingData asset get distinct IDs. Reserved for
     *  future equip/effect wiring (any ID-keyed effect ranges must use this,
     *  never a fabricated literal or an asset-name hash — those collide across
     *  duplicate-asset entries). */
    UPROPERTY(BlueprintReadOnly, Category = "Identity")
    int32 InstanceID = 0;

    /** Persistent owned-instance identity (shape-B loadout referencing). Minted
     *  ONCE at acquisition (UInventoryComponent::AddRing) — NOT by
     *  CreateFromRing, so loadout-inflated ephemeral entries carry an INVALID
     *  guid (= "not an owned instance") and a resolved-ref copy of an owned
     *  entry carries the owned guid verbatim. Distinct from the session-local
     *  int32 InstanceID above. */
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Identity")
    FGuid PersistentID;

    /** Per-instance TIER — seeded from the ring asset at acquisition, then MUTATED by the future
     *  leveling action (asset Tier is the starting value / fallback). Tier reads still use the asset
     *  Ring->Tier until the cluster-2 repoint, so this is written-but-unread now. */
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Identity")
    EItemTier Tier = EItemTier::F_Tier;

    /** Per-instance QUALITY — rolled at drop (weighted, Luck/perk-biased) and stamped here; never
     *  mutated by leveling. Placeholder default until the weighted-roll cluster; nothing reads it yet. */
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Identity")
    EItemQuality Quality = EItemQuality::F_Quality;

    /** Attached refined crystal or evolution item, discriminated by Kind. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring")
    FRuntimeAttachedItem AttachedItem;

    /** Spells assigned to this ring's crystal slots (lost when crystal removed) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring")
    TArray<USpellData *> AssignedSpells;

    /** Per-instance stat bonus state. Seeded from the asset's authored
     *  BaseStatBonus at CreateFromRing time (the asset's Generated layer is
     *  designer PREVIEW, never copied); toggle-ON acquisitions replace this with
     *  Base + a fresh per-instance roll (ApplyPickupRoll). This is what runtime
     *  damage and stat queries should read — not the asset layers. */
    UPROPERTY(BlueprintReadWrite, Category = "Engravings")
    FEquipmentStatBonus StatBonus;

    /** Per-instance status-buildup resistance (per-category, own zero-sum pool).
     *  Seeded from the asset's authored BaseResistance at CreateFromRing time;
     *  toggle-ON acquisitions replace with Base + a fresh roll — same flow as
     *  StatBonus. Read at buildup via ULoadoutComponent::GetActiveResistanceBonus. */
    UPROPERTY(BlueprintReadWrite, Category = "Engravings")
    FResistanceBonus ResistanceBonus;

    // ==================== ROLL POOLS (per-instance) ====================
    // Two independent roll economies — stats and resistance. MaxPool = the point
    // budget this instance's roll distributes (seeded at acquisition in U2). Pool =
    // reroll charge meter (future economy: rewards fill it, reroll unlocks at
    // Pool == MaxPool, spends to 0). STORAGE ONLY (U0) — nothing reads these yet;
    // all default 0 = inert.
    UPROPERTY(BlueprintReadWrite, Category = "Engravings")
    int32 StatPool = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Engravings")
    int32 StatMaxPool = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Engravings")
    int32 ResistancePool = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Engravings")
    int32 ResistanceMaxPool = 0;

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
