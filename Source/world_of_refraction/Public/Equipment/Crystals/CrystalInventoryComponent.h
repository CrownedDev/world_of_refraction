// CrystalInventoryComponent.h
// Count-based inventory for refined and item (consumable) crystals.
//
// Storage keyed on FCrystalId (Type+Tier). The gem/item/refined model:
//  - gem vs augment-stone  (CrystalTypeHelpers::IsGemType(Type))
//  - gems have BOTH forms: item (unrefined consumable) AND refined (slottable)
//  - stones have ONLY an item form — they attach DIRECTLY (no refining), so there
//    is NO StoneRefined pool. Refined-pool ops on a stone are rejected (false/0).
// → three independent pools, each capped per tier:
//  - GemItem  / GemRefined : gems   (consumable / slottable)
//  - StoneItem             : stones (consumable; attach-only, no refined form)
// The Id-keyed methods below DISPATCH to the right pool by IsGemType(Id.Type),
// so external callers keep using one type-agnostic API — they never name a pool.
// Caps are PER CONTAINER: gems and stones each enforce CRYSTAL_PER_TIER_CAP
// independently per tier (a full gem shelf does not block stones, and vice-versa).
//
// Evolution crystals live in UEvolutionInventoryComponent as instances with
// FGuids — counts aren't enough because trade APIs need identity. This
// component never stores evolution items.
//
// Identity by (Type, Tier) is sufficient because refined/item crystals are
// fungible — two refined Garnet (F) crystals are interchangeable. Per-attachment
// durability lives on the holder, not here.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Inventory/ItemTier.h"
#include "CrystalInventoryComponent.generated.h"

UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UCrystalInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCrystalInventoryComponent();

    // ==================== STORAGE ====================
    // Three pools: gems have item + refined; stones have item only (no refined form —
    // they attach directly). The Id-keyed methods dispatch here by IsGemType(Id.Type);
    // callers never touch these directly.

    /** Unrefined consumable GEMS, counted per (Type, Tier). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Inventory|Crystals")
    TMap<FCrystalId, int32> GemItem;

    /** Refined slottable GEMS, counted per (Type, Tier). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Inventory|Crystals")
    TMap<FCrystalId, int32> GemRefined;

    /** AUGMENT STONES, counted per (Type, Tier). Stones attach directly (no refining),
     *  so this is their only pool — there is no StoneRefined. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Inventory|Crystals")
    TMap<FCrystalId, int32> StoneItem;

    // ==================== WRITE ====================

    /** Add Count item crystals at Id. Returns false on cap exhaustion or
     *  non-positive Count — no partial writes. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Crystals")
    bool AddItemCount(FCrystalId Id, int32 Count = 1);

    /** Add Count refined crystals at Id. Same per-tier cap, independent pool. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Crystals")
    bool AddRefinedCount(FCrystalId Id, int32 Count = 1);

    /** Remove up to Count item crystals at Id. Clamps to available; returns
     *  the number actually removed (0 if Count <= 0 or Id absent). Removes
     *  the TMap entry entirely when the count reaches 0. Used by auto-equip
     *  and runtime equip-transfer to debit inventory. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Crystals")
    int32 RemoveItemCount(FCrystalId Id, int32 Count = 1);

    /** Remove up to Count refined crystals at Id. Same semantics as
     *  RemoveItemCount but for the refined pool. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Crystals")
    int32 RemoveRefinedCount(FCrystalId Id, int32 Count = 1);

    // ==================== READ ====================

    /** Count at exact Id in the item-crystal pool. 0 when absent. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    int32 GetItemCount(FCrystalId Id) const;

    /** Count at exact Id in the refined-crystal pool. 0 when absent. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    int32 GetRefinedCount(FCrystalId Id) const;

    /** Sum of item-crystal counts across all Types at the given Tier. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    int32 GetItemCountForTier(EItemTier Tier) const;

    /** Sum of refined-crystal counts across all Types at the given Tier. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    int32 GetRefinedCountForTier(EItemTier Tier) const;

    /** Sum of item + refined counts across all three pools (GemItem + GemRefined +
     *  StoneItem). Convenience for inventory summary / debug display. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    int32 GetTotalCount() const;

    /** Number of distinct (Type, Tier) STACKS across all three pools — i.e. the
     *  combined TMap entry count, not the summed quantity. For diagnostics that
     *  previously read the raw maps' Num(). */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    int32 GetStackCount() const;

    /** Empty all three pools (GemItem / GemRefined / StoneItem). Used by re-init so a
     *  reload doesn't accumulate; replaces direct raw-map .Empty() calls. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Crystals")
    void ClearAll();

    // ==================== CAPS ====================

    /** True iff AddItemCount(Id, Count) would not exceed the per-tier cap. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    bool CanAddItemCount(FCrystalId Id, int32 Count = 1) const;

    /** True iff AddRefinedCount(Id, Count) would not exceed the per-tier cap. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    bool CanAddRefinedCount(FCrystalId Id, int32 Count = 1) const;

private:
    // ==================== POOL DISPATCH ====================
    // Select the container for a Type by the gem/stone axis (IsGemType). The
    // item/refined axis is chosen by which overload the caller is in. These keep
    // the gem/stone branch in one place so every Id-keyed method routes the same way.

    TMap<FCrystalId, int32> &ItemPoolFor(ECrystalType Type);
    const TMap<FCrystalId, int32> &ItemPoolFor(ECrystalType Type) const;

    /** GEM-ONLY refined pool. Gems → &GemRefined; stones → nullptr (stones have no
     *  refined form). Every refined method null-checks this and rejects (false/0),
     *  which is how "stones don't refine" is enforced in one place. */
    TMap<FCrystalId, int32> *RefinedPoolFor(ECrystalType Type);
    const TMap<FCrystalId, int32> *RefinedPoolFor(ECrystalType Type) const;

    /** Sum of counts at Tier within a single pool — the per-container tier sum
     *  the cap check uses (so gem and stone caps stay independent). */
    static int32 SumAtTier(const TMap<FCrystalId, int32> &Pool, EItemTier Tier);
};
