// FCrystalId.h
// Universal crystal identifier — type plus tier. Used wherever a crystal is
// referenced in the new model after the crystal/evolution refactor: refined
// attachments, item-crystal slots, count-table keys, effect-table lookup keys.
// Designed for TMap use — operator== and GetTypeHash both defined.

#pragma once

#include "CoreMinimal.h"
#include "CrystalType.h"
#include "ItemTier.h"
#include "FCrystalId.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FCrystalId
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal")
    ECrystalType Type = ECrystalType::Garnet;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal")
    EItemTier Tier = EItemTier::F_Tier;

    FCrystalId() = default;

    FCrystalId(ECrystalType InType, EItemTier InTier)
        : Type(InType), Tier(InTier) {}

    bool operator==(const FCrystalId &Other) const
    {
        return Type == Other.Type && Tier == Other.Tier;
    }

    bool operator!=(const FCrystalId &Other) const
    {
        return !(*this == Other);
    }
};

FORCEINLINE uint32 GetTypeHash(const FCrystalId &Id)
{
    return HashCombine(
        ::GetTypeHash(static_cast<uint8>(Id.Type)),
        ::GetTypeHash(static_cast<uint8>(Id.Tier)));
}
