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

    /** Crystal sub-type. Visible for Crystal (Garnet…Quartz) and WeaponStone
     *  (DamageStone vs AbilityStone) — lets the designer pick the stone variant. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Crystal || Kind == EAttachedItemKind::WeaponStone", EditConditionHides))
    ECrystalType CrystalType = ECrystalType::Garnet;

    /** Tier of the attachment's crystal identity. Visible for Crystal and
     *  WeaponStone — a weapon stone carries identity FCrystalId{sub-type, Tier},
     *  with the sub-type (Damage/Ability) picked via CrystalType above. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Crystal || Kind == EAttachedItemKind::WeaponStone", EditConditionHides))
    EItemTier CrystalTier = EItemTier::F_Tier;

    /** Evolution item asset. Only visible when Kind == Evolution.
     *  Retargets to UEvolutionItemData* in commit 3. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Evolution", EditConditionHides))
    UEvolutionItemData *Evolution = nullptr;
};
