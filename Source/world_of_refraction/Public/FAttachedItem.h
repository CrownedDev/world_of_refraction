// FAttachedItem.h
// Design-time attachment slot for UEquipmentDataBase (weapons + rings).
// Replaces SlottedCrystal after the storage-split commits land.
//
// Discriminated by Kind: when Refined, designer picks (Type, Tier) from the
// dropdowns; when Evolution, designer picks an evolution item asset. Other
// fields are hidden in the editor via EditConditionHides.
//
// Item is typed as UItemData* during the transition window; the commit 3
// rename retargets it to UEvolutionItemData*.

#pragma once

#include "CoreMinimal.h"
#include "EAttachedItemKind.h"
#include "CrystalType.h"
#include "ItemTier.h"
#include "FAttachedItem.generated.h"

class UItemData;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FAttachedItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item")
    EAttachedItemKind Kind = EAttachedItemKind::None;

    /** Refined crystal type. Only visible when Kind == Refined. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Refined", EditConditionHides))
    ECrystalType RefinedType = ECrystalType::Garnet;

    /** Refined crystal tier. Only visible when Kind == Refined. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Refined", EditConditionHides))
    EItemTier RefinedTier = EItemTier::F_Tier;

    /** Evolution item asset. Only visible when Kind == Evolution.
     *  Retargets to UEvolutionItemData* in commit 3. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Evolution", EditConditionHides))
    UItemData *Evolution = nullptr;
};
