// CrystalInventoryComponent.h
// Count-based inventory for crystals. Two pools, keyed on FCrystalId (Type+Tier):
//  - Gems   : DUAL-PURPOSE gem crystals — throwable AND slottable from ONE bucket.
//             The gem-merge (§13.5) collapsed the former GemItem + GemRefined into
//             this single pool: there is no item/refined split anymore. Both throwing
//             (consume) and slotting (attach-debit) draw from Gems.
//  - Stones : augment stones — SLOT-ONLY (attach directly; no throw, no refined form).
//             Behaviour unchanged by the merge; only the name changed (was StoneItem).
// IsGemType(Id.Type) DISPATCHES an Id to its pool (gem → Gems, stone → Stones) — that
// axis STAYS; only the within-gems item/refined axis was removed. Callers use one
// type-agnostic API (AddCount/RemoveCount/GetCount/CanAddCount) and never name a pool.
// Caps are PER POOL: gems and stones each enforce CRYSTAL_PER_TIER_CAP independently
// per tier (a full gem shelf does not block stones, and vice-versa).
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
    // Two pools: Gems (dual-purpose — one bucket, no item/refined split) and Stones
    // (slot-only). The Id-keyed methods dispatch here by IsGemType(Id.Type); callers
    // never touch these directly.

    /** DUAL-PURPOSE gem crystals (throwable AND slottable), counted per (Type, Tier).
     *  The gem-merge (§13.5) collapsed the former GemItem + GemRefined into this one
     *  bucket — slotting and throwing both debit it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Inventory|Crystals")
    TMap<FCrystalId, int32> Gems;

    /** AUGMENT STONES (slot-only; attach directly, no refined form), counted per
     *  (Type, Tier). Renamed from StoneItem in the gem-merge; behaviour unchanged. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Inventory|Crystals")
    TMap<FCrystalId, int32> Stones;

    // ==================== WRITE ====================

    /** Add Count crystals at Id — routed to Gems or Stones by IsGemType(Id.Type).
     *  Returns false on cap exhaustion or non-positive Count — no partial writes. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Crystals")
    bool AddCount(FCrystalId Id, int32 Count = 1);

    /** Remove up to Count crystals at Id (routed by IsGemType). Clamps to available;
     *  returns the number actually removed (0 if Count <= 0 or Id absent). Removes the
     *  TMap entry entirely when the count reaches 0. Used by slotting/throwing debits. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Crystals")
    int32 RemoveCount(FCrystalId Id, int32 Count = 1);

    // ==================== READ ====================

    /** Count at exact Id in its routed pool (Gems or Stones). 0 when absent. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    int32 GetCount(FCrystalId Id) const;

    /** Combined Gems + Stones count across all Types at the given Tier (display total).
     *  Per-pool cap checks use SumAtTier on a single pool, not this sum. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    int32 GetCountForTier(EItemTier Tier) const;

    /** Sum of counts across both pools (Gems + Stones). Convenience for summary / debug. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    int32 GetTotalCount() const;

    /** Number of distinct (Type, Tier) STACKS across both pools — the combined TMap
     *  entry count, not the summed quantity. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    int32 GetStackCount() const;

    /** Empty both pools (Gems / Stones). Used by re-init so a reload doesn't accumulate. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Crystals")
    void ClearAll();

    // ==================== CAPS ====================

    /** True iff AddCount(Id, Count) would not exceed Id's pool's per-tier cap. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    bool CanAddCount(FCrystalId Id, int32 Count = 1) const;

private:
    // ==================== POOL DISPATCH ====================
    // Select the pool for a Type by the gem/stone axis (IsGemType): gem → Gems,
    // stone → Stones. One dispatch, in one place — the item/refined axis is gone.

    TMap<FCrystalId, int32> &PoolFor(ECrystalType Type);
    const TMap<FCrystalId, int32> &PoolFor(ECrystalType Type) const;

    /** Sum of counts at Tier within a single pool — the per-pool tier sum the cap
     *  check uses (so gem and stone caps stay independent). */
    static int32 SumAtTier(const TMap<FCrystalId, int32> &Pool, EItemTier Tier);
};
