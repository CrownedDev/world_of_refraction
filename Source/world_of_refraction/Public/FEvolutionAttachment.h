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
#include "FEvolutionAttachment.generated.h"

class UEvolutionItemData;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FEvolutionAttachment
{
    GENERATED_BODY()

    /** Evolution item asset. Retargets to UEvolutionItemData* in commit 3. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Evolution")
    UEvolutionItemData *Item = nullptr;

    /** Per-instance durability. Only meaningful when the item's bCanBreak is true. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Evolution")
    int32 CurrentDurability = 0;

    /** True iff the item exists, can break, and current durability has reached zero. */
    bool IsBroken() const;

    /** Apply wear. Returns true iff this wear broke the item (durability
     *  transitioned from >0 to 0). Skips when Item is null, Amount <= 0, or
     *  durability is already at zero.
     *
     *  bForceWear (default false): when false, also skips if bCanBreak is
     *  false (per-asset opt-in). When true, bypasses ONLY the bCanBreak
     *  gate — all other guards still apply. Intended for callers whose
     *  mechanic is intrinsic (e.g. Broken Darkness) where per-asset opt-in
     *  is fragile; FEvolutionAttachment itself stays BD-agnostic. Default
     *  preserves existing behavior for every other caller. */
    bool ApplyWear(int32 Amount, bool bForceWear = false);

    /** Repair between combats. Clamps to Item->MaxDurability. Returns actual
     *  amount repaired. No-op when Item is null or Amount <= 0. */
    int32 RepairBetweenCombats(int32 Amount);
};
