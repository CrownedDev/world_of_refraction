// FRefinedAttachment.h
// Runtime refined-crystal attachment. Identified by FCrystalId (Type + Tier)
// with per-instance CurrentDurability. No asset pointer — refined crystals
// post-refactor are enum pairs, not asset references. No GUID — refined
// crystals are destroyed on removal from their holder, so no instance
// retrieval pathway exists.
//
// Used as the Refined branch of FRuntimeAttachedItem.

#pragma once

#include "CoreMinimal.h"
#include "FCrystalId.h"
#include "FRefinedAttachment.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FRefinedAttachment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Refined")
    FCrystalId Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Refined")
    int32 CurrentDurability = 0;

    FRefinedAttachment() = default;

    FRefinedAttachment(const FCrystalId &InId, int32 InDurability)
        : Id(InId), CurrentDurability(InDurability) {}
};
