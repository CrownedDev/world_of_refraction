// CrystalInventoryComponent.h
// Count-based inventory for crystals. Two pools, keyed on FCrystalId (Type+Tier):
//  - Crystals : DUAL-PURPOSE gem-family crystals — throwable AND slottable from ONE
//             bucket. The gem-merge (§13.5) collapsed the former GemItem + GemRefined
//             into this single pool: there is no item/refined split anymore. Both
//             throwing (consume) and slotting (attach-debit) draw from Crystals.
//  - Stones : augment stones — SLOT-ONLY (attach directly; no throw, no refined form).
//             Behaviour unchanged by the merge; only the name changed (was StoneItem).
// NOTE: augment stones ARE crystals too (FCrystalId covers both); the pool named
// "Crystals" is the dual-purpose GEM-FAMILY bucket, "Stones" the augment-stone bucket.
// GetCrystals()/GetStones() are per-pool; GetTotalCount()/GetCountForTier() aggregate both.
// IsGemType(Id.Type) DISPATCHES an Id to its pool (gem → Crystals, stone → Stones) — that
// axis STAYS; only the within-gem-family item/refined axis was removed. Callers use one
// type-agnostic API (AddCount/RemoveCount/GetCount/CanAddCount) and never name a pool.
// Caps are PER POOL: each enforces CRYSTAL_PER_TIER_CAP independently per tier (a full
// crystal shelf does not block stones, and vice-versa).
//
// Evolution crystals live in UEvolutionInventoryComponent as instances with
// FGuids — counts aren't enough because trade APIs need identity. This
// component never stores evolution items.
//
// Identity by (Type, Tier) is sufficient because refined/item crystals are
// fungible — two refined Garnet (F) crystals are interchangeable. Per-attachment
// durability lives on the holder, not here.
//
// B-203 / B-322 (D-097, D-117): a party member's component is bound to its pouch in UPoolSubsystem
// (BindSharedItems, at spawn). While bound, every call goes to the pouch: AddCount carries what fits and
// stores the rest (so it never fails for space), RemoveCount takes from the pouch, and every read
// (GetCount, GetCountForTier, GetTotalCount, GetStackCount, GetHeldStacks) is the pouch's (B-313b).
// B-317: the Crystals / Stones maps are no longer a copy of the pouch; a bound component leaves them empty.
// Unbound components (enemies, tests) keep their own counts in them.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Inventory/ItemTier.h"
#include "CrystalInventoryComponent.generated.h"

class UPoolSubsystem;

UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UCrystalInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCrystalInventoryComponent();

    // ==================== STORAGE ====================
    // Two pools: Crystals (dual-purpose — one bucket, no item/refined split) and Stones
    // (slot-only). The Id-keyed methods dispatch here by IsGemType(Id.Type); callers
    // never touch these directly.

    /** DUAL-PURPOSE gem-family crystals (throwable AND slottable), counted per (Type, Tier).
     *  The gem-merge (§13.5) collapsed the former GemItem + GemRefined into this one
     *  bucket — slotting and throwing both debit it. Unbound components only: empty while
     *  bound to a pouch (B-317; read the pouch through GetCount / GetHeldStacks). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Crystals")
    TMap<FCrystalId, int32> Crystals;

    /** AUGMENT STONES (slot-only; attach directly, no refined form), counted per
     *  (Type, Tier). Renamed from StoneItem in the gem-merge; behaviour unchanged.
     *  Unbound components only: empty while bound to a pouch (B-317). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Crystals")
    TMap<FCrystalId, int32> Stones;

    // ==================== WRITE ====================

    /** Add Count crystals at Id — routed to Crystals or Stones by IsGemType(Id.Type).
     *  Returns false on cap exhaustion or non-positive Count — no partial writes. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Crystals")
    bool AddCount(FCrystalId Id, int32 Count = 1);

    /** Remove up to Count crystals at Id (routed by IsGemType). Clamps to available;
     *  returns the number actually removed (0 if Count <= 0 or Id absent). Removes the
     *  TMap entry entirely when the count reaches 0. Used by slotting/throwing debits. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Crystals")
    int32 RemoveCount(FCrystalId Id, int32 Count = 1);

    // ==================== READ ====================

    /** Count at exact Id in its routed pool (Crystals or Stones). 0 when absent. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    int32 GetCount(FCrystalId Id) const;

    /** Combined Crystals + Stones count across all Types at the given Tier (display total).
     *  Per-pool cap checks use SumAtTier on a single pool, not this sum. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    int32 GetCountForTier(EItemTier Tier) const;

    /** Sum of counts across both pools (Crystals + Stones). Convenience for summary / debug. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    int32 GetTotalCount() const;

    /** Number of distinct (Type, Tier) STACKS across both pools — the combined TMap
     *  entry count, not the summed quantity. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    int32 GetStackCount() const;

    /** Empty both pools (Crystals / Stones). Used by re-init so a reload doesn't accumulate. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Crystals")
    void ClearAll();

    /** B-313b: every stack held, gems and stones together: the member's pouch while bound, else this component's own
     *  counts (an enemy, a test, a member before its first draw). */
    TMap<FCrystalId, int32> GetHeldStacks() const;

    // ==================== CAPS ====================

    /** True iff AddCount(Id, Count) would not exceed Id's pool's per-tier cap. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    bool CanAddCount(FCrystalId Id, int32 Count = 1) const;

    // ==================== THE CHARACTER'S POUCH (B-203, B-322) ====================

    /** Use the character's pouch (CharacterKey, UPoolSubsystem::GetCharacterKey) from now on: reads and changes go
     *  to Pool, and this component's own counts are emptied. ClearAll unbinds. */
    void BindSharedItems(UPoolSubsystem *Pool, FName CharacterKey);

    bool IsSharingItems() const { return SharedPool.IsValid(); }
    FName GetPouchCharacterKey() const { return PouchKey; }
    /** B-313: the pool holding this member's pouch (null when unbound). Readers that mean the pouch go here
     *  (UPoolSubsystem::GetPouch) rather than to this component's Crystals / Stones copy. */
    UPoolSubsystem *GetPouchPool() const { return SharedPool.Get(); }

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void UnbindSharedItems();

    /** B-313b: the bound pouch's stacks (UPoolSubsystem::GetCarriedItems); null when unbound. */
    const TMap<FCrystalId, int32> *GetPouchStacks() const;

    TWeakObjectPtr<UPoolSubsystem> SharedPool;
    /** Whose pouch this component uses while SharedPool is set. */
    FName PouchKey;

    // ==================== POOL DISPATCH ====================
    // Select the pool for a Type by the gem/stone axis (IsGemType): gem → Crystals,
    // stone → Stones. One dispatch, in one place — the item/refined axis is gone.

    TMap<FCrystalId, int32> &PoolFor(ECrystalType Type);
    const TMap<FCrystalId, int32> &PoolFor(ECrystalType Type) const;

    /** Sum of counts at Tier within a single pool — the per-pool tier sum the cap
     *  check uses (so gem and stone caps stay independent). */
    static int32 SumAtTier(const TMap<FCrystalId, int32> &Pool, EItemTier Tier);
};
