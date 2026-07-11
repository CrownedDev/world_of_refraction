// FEvolutionAttachment.h
// Runtime evolution-item attachment. Holds the evolution asset pointer plus
// per-instance CurrentDurability. No GUID — slotted evolution items are
// destroyed on removal from their holder (locked decision), so no instance-
// retrieval pathway exists.
//
// Item is typed as UEvolutionItemData* during the transition window; the commit 3
// rename retargets it to UEvolutionItemData* with zero call-site impact.
//
// Used as the Evolution branch of FRuntimeAttachedItem.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "FEvolutionAttachment.generated.h"

class UEvolutionItemData;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FEvolutionAttachment
{
    GENERATED_BODY()

    /** Evolution item asset. Retargets to UEvolutionItemData* in commit 3. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Evolution")
    UEvolutionItemData *Item = nullptr;

    /** Per-instance (leveled) TIER — copied from the owning FEvolutionInventoryEntry at slot time
     *  (primary slot, in FCombatLoadout inflation), or seeded from the asset for the weapon/ring-
     *  attached form (no owned-instance source until the gear-attach op exists). All slotted
     *  budget/combat/display reads use THIS instance tier, NOT the asset Item->Tier. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Evolution")
    EItemTier Tier = EItemTier::F_Tier;

    /** Link to the owned FEvolutionInventoryEntry this attachment references (the gear-side mirror of
     *  FCombatLoadout::PrimaryEvolutionInstance, iii-b). INVALID = authored-locked (baked into the
     *  weapon/ring asset, no owned entry); VALID = player-attached (references an owned entry, set by
     *  the gear-attach op). Keystone for: the 5/run cap (skip valid — counted in Entries), gear-side
     *  leveled tier, gear-remove return, and combat-break resolution. Written-but-unread until the
     *  gear-side clusters consume it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Evolution")
    FGuid InstanceID;

    /** Per-instance durability. Only meaningful when the item's Breakability allows wear. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Evolution")
    int32 CurrentDurability = 0;

    // ==================== PER-INSTANCE ROLL STATE ====================
    // Carried from FEvolutionInventoryEntry when slotted (U3) — the slotted crystal
    // keeps its instance roll. Gameplay reads (resistance / pillar / infusion) will
    // read THIS plus the asset's Base in U3. Default-zero = inert until rolled.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Evolution")
    FEquipmentStatBonus GeneratedStatBonus;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Evolution")
    FResistanceBonus GeneratedResistance;

    // Roll pools — stats + resistance, two independent economies. STORAGE ONLY (U0);
    // default 0 = inert. Seeded/carried in U3.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Evolution")
    int32 StatPool = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Evolution")
    int32 StatMaxPool = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Evolution")
    int32 ResistancePool = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Evolution")
    int32 ResistanceMaxPool = 0;

    /** True iff the item exists, can break, and current durability has reached zero. */
    bool IsBroken() const;

    /** Apply wear. Returns true iff this wear broke the item (durability
     *  transitioned from >0 to 0). Skips when Item is null, Amount <= 0, or
     *  durability is already at zero.
     *
     *  bForceWear (default false): when false, also skips if Breakability blocks
     *  wear (BDBreakable without force, or Unbreakable). When true, bypasses ONLY
     *  the Breakability gate — all other guards still apply. Intended for callers whose
     *  mechanic is intrinsic (e.g. Broken Darkness) where per-asset opt-in
     *  is fragile; FEvolutionAttachment itself stays BD-agnostic. Default
     *  preserves existing behavior for every other caller. */
    bool ApplyWear(int32 Amount, bool bForceWear = false);

    /** Repair between combats. Clamps to Item->MaxDurability. Returns actual
     *  amount repaired. No-op when Item is null or Amount <= 0. */
    int32 RepairBetweenCombats(int32 Amount);
};
