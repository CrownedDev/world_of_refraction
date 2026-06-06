// FBrokenCrystalPayload.h
// Discriminated payload describing a crystal break event. Introduced in
// commit 7a so the type exists; the OnCrystalBroken delegate signature
// reshape that consumes it lands atomically with the AttachedCrystal
// type swap in commit 7b.
//
// Refined breaks identify by FCrystalId (refined crystals have no asset
// pointer in the new model). Evolution breaks carry the item asset pointer.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/EAttachedItemKind.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "FBrokenCrystalPayload.generated.h"

class UEvolutionItemData;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FBrokenCrystalPayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Broken Crystal")
    EAttachedItemKind Kind = EAttachedItemKind::None;

    /** Meaningful when Kind == Refined. */
    UPROPERTY(BlueprintReadOnly, Category = "Broken Crystal")
    FCrystalId CrystalId;

    /** Meaningful when Kind == Evolution. */
    UPROPERTY(BlueprintReadOnly, Category = "Broken Crystal")
    UEvolutionItemData *Item = nullptr;
};
