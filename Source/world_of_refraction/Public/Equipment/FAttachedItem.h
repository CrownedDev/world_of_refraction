// FAttachedItem.h
// Design-time attachment slot for UEquipmentDataBase (weapons + rings).
// Replaces SlottedCrystal after the storage-split commits land.
//
// Discriminated by Kind: when Refined, designer picks (Type, Tier) from the
// dropdowns; when Evolution, designer picks an evolution item asset. Other
// fields are hidden in the editor via EditConditionHides.
//
// Item is typed as UEvolutionItemData* during the transition window; the commit 3
// rename retargets it to UEvolutionItemData*.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/EAttachedItemKind.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Inventory/ItemTier.h"
#include "FAttachedItem.generated.h"

class UEvolutionItemData;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FAttachedItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item")
    EAttachedItemKind Kind = EAttachedItemKind::None;

    /** Refined crystal type. Only visible when Kind == Refined. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Crystal", EditConditionHides))
    ECrystalType CrystalType = ECrystalType::Garnet;

    /** Tier of the attachment's crystal identity. Visible for Refined and
     *  Whetstone — a whetstone carries identity FCrystalId{Whetstone, RefinedTier}.
     *  RefinedType is hidden for Whetstone (the type is implied by Kind). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Crystal || Kind == EAttachedItemKind::WeaponStone", EditConditionHides))
    EItemTier CrystalTier = EItemTier::F_Tier;

    /** Evolution item asset. Only visible when Kind == Evolution.
     *  Retargets to UEvolutionItemData* in commit 3. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Evolution", EditConditionHides))
    UEvolutionItemData *Evolution = nullptr;
};
