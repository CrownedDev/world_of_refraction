// FCrystalId.h
// Universal crystal identifier — type plus tier. Used wherever a crystal is
// referenced in the new model after the crystal/evolution refactor: refined
// attachments, item-crystal slots, count-table keys, effect-table lookup keys.
// Designed for TMap use — operator== and GetTypeHash both defined.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Inventory/ItemTier.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "FCrystalId.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FCrystalId
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal")
    ECrystalType Type = ECrystalType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal")
    EItemTier Tier = EItemTier::F_Tier;

    FCrystalId() = default;

    FCrystalId(ECrystalType InType, EItemTier InTier)
        : Type(InType), Tier(InTier) {}

    /** B-353 (D-142): the sub-stat a StatStone amplifies; None for every other kind. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal",
              meta = (EditCondition = "Type == ECrystalType::StatStone", EditConditionHides))
    ESubStat StoneStat = ESubStat::None;

    FCrystalId(ECrystalType InType, EItemTier InTier, ESubStat InStoneStat)
        : Type(InType), Tier(InTier), StoneStat(InStoneStat) {}

    bool operator==(const FCrystalId &Other) const
    {
        return Type == Other.Type && Tier == Other.Tier && StoneStat == Other.StoneStat;
    }

    bool operator!=(const FCrystalId &Other) const
    {
        return !(*this == Other);
    }
};

FORCEINLINE uint32 GetTypeHash(const FCrystalId &Id)
{
    return HashCombine(HashCombine(
        ::GetTypeHash(static_cast<uint8>(Id.Type)),
        ::GetTypeHash(static_cast<uint8>(Id.Tier))),
        ::GetTypeHash(static_cast<uint8>(Id.StoneStat)));
}
