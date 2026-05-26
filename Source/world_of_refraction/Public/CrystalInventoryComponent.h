// CrystalInventoryComponent.h
// Count-based inventory for refined and item (consumable) crystals.
//
// Two independent pools, each capped per tier and keyed on FCrystalId (Type+Tier):
//  - ItemCrystals:    unrefined consumables (no asset — FCrystalId only)
//  - RefinedCrystals: refined slottable items (no asset — FCrystalId only)
//
// Evolution crystals live in UEvolutionInventoryComponent as instances with
// FGuids — counts aren't enough because trade APIs need identity. This
// component never stores evolution items.
//
// Storage is keyed on FCrystalId (Type + Tier). Identity by (Type, Tier) is
// sufficient because refined/item crystals are fungible — two refined
// Garnet (F) crystals are interchangeable. Per-attachment durability lives
// on the holder, not here.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FCrystalId.h"
#include "ItemTier.h"
#include "CrystalInventoryComponent.generated.h"

UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UCrystalInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCrystalInventoryComponent();

    // ==================== STORAGE ====================

    /** Unrefined consumable crystals, counted per (Type, Tier). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Inventory|Crystals")
    TMap<FCrystalId, int32> ItemCrystals;

    /** Refined slottable crystals, counted per (Type, Tier). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Inventory|Crystals")
    TMap<FCrystalId, int32> RefinedCrystals;

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

    /** Sum of item-crystal + refined-crystal counts across all (Type, Tier).
     *  Convenience for inventory summary / debug display. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    int32 GetTotalCount() const;

    // ==================== CAPS ====================

    /** True iff AddItemCount(Id, Count) would not exceed the per-tier cap. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    bool CanAddItemCount(FCrystalId Id, int32 Count = 1) const;

    /** True iff AddRefinedCount(Id, Count) would not exceed the per-tier cap. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Crystals")
    bool CanAddRefinedCount(FCrystalId Id, int32 Count = 1) const;
};
