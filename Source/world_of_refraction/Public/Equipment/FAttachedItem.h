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
#include "Equipment/Crystals/FFusionId.h"
#include "FAttachedItem.generated.h"

class UEvolutionItemData;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FAttachedItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item")
    EAttachedItemKind Kind = EAttachedItemKind::None;

    /** Crystal sub-type. Visible for Crystal (Garnet…Quartz) and AugmentStone
     *  (DamageStone vs AbilityStone) — lets the designer pick the stone variant. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Crystal || Kind == EAttachedItemKind::AugmentStone", EditConditionHides,
                      GetRestrictedEnumValues = "GetRestrictedCrystalTypes"))
    ECrystalType CrystalType = ECrystalType::Garnet;

    /** Tier of the attachment's crystal identity. Visible for Crystal and
     *  AugmentStone — a augment stone carries identity FCrystalId{sub-type, Tier},
     *  with the sub-type (Damage/Ability) picked via CrystalType above. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Crystal || Kind == EAttachedItemKind::AugmentStone", EditConditionHides))
    EItemTier CrystalTier = EItemTier::F_Tier;

    /** Evolution item asset. Only visible when Kind == Evolution.
     *  Retargets to UEvolutionItemData* in commit 3. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Evolution", EditConditionHides))
    UEvolutionItemData *Evolution = nullptr;

    /** Fusion halves + bonus stat. Only visible when Kind == Fusion. The half/pair
     *  rules (>=1 stat-stone, <=1 crystal, no evolution half) are enforced by
     *  author-time validation, not editor grey-out (mirrors the project's
     *  EditCondition-plus-validation stance over IDetailCustomization). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Fusion", EditConditionHides))
    FFusionId Fusion;
};
