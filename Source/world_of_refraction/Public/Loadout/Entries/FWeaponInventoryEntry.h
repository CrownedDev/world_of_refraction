// FWeaponInventoryEntry.h
// Weapon inventory entry with unified crystal system
//
// ARCHITECTURE: Runtime inventory instance, separate from WeaponData asset
// WeaponData = immutable template (stats, mesh, type, default crystal for editor preview)
// FWeaponInventoryEntry = mutable runtime state (ACTUAL attached crystal + custom spells)
//
// IMPORTANT: When checking crystal state at runtime, use FWeaponInventoryEntry.AttachedItem,
// NOT WeaponData.AttachedItem. The data asset may have a default crystal for editor
// testing, but the inventory entry is the runtime truth.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryConstants.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "FWeaponInventoryEntry.generated.h"

class UWeaponData;
class USpellData;

/**
 * FWeaponInventoryEntry
 * Represents a single weapon INSTANCE in inventory with its attached crystal state
 * Slot cost varies based on crystal: Base=1, Crystal=2, Evolution=3
 *
 * This is RUNTIME STATE - the actual crystal attached to this specific weapon instance.
 * Multiple characters can reference the same WeaponData but have different crystals attached.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FWeaponInventoryEntry
{
    GENERATED_BODY()

    /** The weapon data asset (immutable template) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UWeaponData *Weapon = nullptr;

    /** Stable per-instance identifier. Generated at CreateFromWeapon time
     *  via a static atomic counter (see FWeaponInventoryEntry.cpp). Two
     *  entries referencing the same UWeaponData asset get distinct IDs.
     *
     *  Callers of USkillEffectManager::ApplyWeaponBonuses must pass this
     *  field as WeaponID — never a fabricated literal, and never a hash
     *  of the asset name (would collide across duplicate-asset entries).
     *  RemoveWeaponBonuses must be paired with the same value used at
     *  apply time. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity")
    int32 InstanceID = 0;

    /** Persistent owned-instance identity (shape-B loadout referencing). Minted
     *  ONCE at acquisition (UInventoryComponent::AddWeapon) — NOT by
     *  CreateFromWeapon, so loadout-inflated ephemeral entries carry an INVALID
     *  guid (= "not an owned instance") and a U1c copy of an owned entry carries
     *  the owned guid verbatim. Distinct from the session-local int32 InstanceID
     *  above (load-bearing for skill-effect ID packing — never repurpose it). */
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Identity")
    FGuid PersistentID;

    /** Attached refined crystal or evolution item, discriminated by Kind. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FRuntimeAttachedItem AttachedItem;

    /** Spells assigned to this weapon's crystal slots (lost when crystal removed) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    TArray<USpellData *> AssignedSpells;

    /** Per-instance stat bonus state. Seeded from the asset's
     *  GetCombinedStatBonus() (BaseStatBonus + GeneratedStatBonus) at
     *  CreateFromWeapon time. This is what runtime damage and stat queries
     *  should read — not the asset's BaseStatBonus / GeneratedStatBonus
     *  layers. */
    UPROPERTY(BlueprintReadWrite, Category = "Mastery")
    FEquipmentStatBonus StatBonus;

    /** Per-instance status-buildup resistance (per-category, own zero-sum pool).
     *  Seeded from the asset's GetCombinedResistance() at CreateFromWeapon time —
     *  same template→instance flow as StatBonus. Read at buildup via
     *  ULoadoutComponent::GetActiveResistanceBonus. */
    UPROPERTY(BlueprintReadWrite, Category = "Mastery")
    FResistanceBonus ResistanceBonus;

    // ==================== ROLL POOLS (per-instance) ====================
    // Two independent roll economies — stats and resistance. MaxPool = the point
    // budget this instance's roll distributes (seeded at acquisition in U2). Pool =
    // reroll charge meter (future economy: rewards fill it, reroll unlocks at
    // Pool == MaxPool, spends to 0). STORAGE ONLY (U0) — nothing reads these yet;
    // all default 0 = inert.
    UPROPERTY(BlueprintReadWrite, Category = "Mastery")
    int32 StatPool = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Mastery")
    int32 StatMaxPool = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Mastery")
    int32 ResistancePool = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Mastery")
    int32 ResistanceMaxPool = 0;

    // ==================== FACTORY ====================

    /** Create entry from WeaponData, optionally copying default crystal */
    static FWeaponInventoryEntry CreateFromWeapon(UWeaponData *InWeapon, bool bCopyDefaultCrystal = false);

    // ==================== STATE QUERIES ====================

    /** Check if entry is valid (has weapon) */
    bool IsValid() const
    {
        return Weapon != nullptr;
    }

    /** Check if weapon has a crystal attached */
    bool HasCrystal() const
    {
        return !AttachedItem.IsEmpty();
    }

    /** Check if weapon is evolved (crystal grants evolution) */
    bool IsEvolved() const
    {
        return AttachedItem.IsEvolution();
    }

    /** Get slot cost based on current state */
    int32 GetSlotCost() const
    {
        if (!HasCrystal())
        {
            return InventoryConstants::WEAPON_BASE_SLOT_COST;
        }
        return InventoryConstants::GetWeaponSlotCost(true, IsEvolved());
    }

    /** Get weapon's current element (from attached crystal) */
    ESpellElement GetElement() const
    {
        return AttachedItem.GetElement();
    }

    /** Check if weapon can cast spells (any non-empty attachment qualifies) */
    bool CanCastSpells() const
    {
        return !AttachedItem.IsEmpty();
    }

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

    /** Check if weapon has stat modifiers (from evolution crystal) */
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

    bool operator==(const FWeaponInventoryEntry &Other) const
    {
        return Weapon == Other.Weapon && AttachedItem == Other.AttachedItem;
    }

    bool operator!=(const FWeaponInventoryEntry &Other) const
    {
        return !(*this == Other);
    }
};
