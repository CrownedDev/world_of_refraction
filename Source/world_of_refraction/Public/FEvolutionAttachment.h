// FEvolutionAttachment.h
// Runtime evolution-item attachment. Holds the evolution asset pointer plus
// per-instance CurrentDurability. No GUID — slotted evolution items are
// destroyed on removal from their holder (locked decision), so no instance-
// retrieval pathway exists.
//
// Item is typed as UItemData* during the transition window; the commit 3
// rename retargets it to UEvolutionItemData* with zero call-site impact.
//
// Used as the Evolution branch of FRuntimeAttachedItem.

#pragma once

#include "CoreMinimal.h"
#include "FEvolutionAttachment.generated.h"

class UItemData;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FEvolutionAttachment
{
    GENERATED_BODY()

    /** Evolution item asset. Retargets to UEvolutionItemData* in commit 3. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Evolution")
    UItemData *Item = nullptr;

    /** Per-instance durability. Only meaningful when the item's bCanBreak is
     *  true (post-commit-3) — today that maps to bImmuneToBreaking == false. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Evolution")
    int32 CurrentDurability = 0;
};
