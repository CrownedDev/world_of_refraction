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

    // ---- Fusion authoring (flat). Visible only when Kind == Fusion. ----
    // Authored here as flat fields (mirrors CrystalType/CrystalTier above) so each
    // half's Type dropdown can grey out the wrong category via GetRestrictedEnumValues
    // — a per-half affordance FFusionId's nested FCrystalId halves can't carry. The
    // runtime pass maps these flat fields into FFusionId later (FFusionId is untouched).

    /** Fusion Half A — augment-stone only. Its Type dropdown greys gems (only stones
     *  selectable). Authoring convention: the stat-stone half. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Fusion", EditConditionHides,
                      GetRestrictedEnumValues = "GetRestrictedFusionHalfATypes"))
    ECrystalType FusionHalfAType = ECrystalType::DamageStone;

    /** Tier of Fusion Half A. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Fusion", EditConditionHides))
    EItemTier FusionHalfATier = EItemTier::F_Tier;

    /** Fusion Half B kind selector — true = crystal (gem) half, false = augment stone.
     *  Drives Half B's Type grey-out below. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Fusion", EditConditionHides))
    bool bFusionHalfBIsCrystal = true;

    /** Fusion Half B — crystal OR augment stone, per bFusionHalfBIsCrystal. Its Type
     *  dropdown greys the other category (stones when crystal, gems when stone). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Fusion", EditConditionHides,
                      GetRestrictedEnumValues = "GetRestrictedFusionHalfBTypes"))
    ECrystalType FusionHalfBType = ECrystalType::Garnet;

    /** Tier of Fusion Half B. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Fusion", EditConditionHides))
    EItemTier FusionHalfBTier = EItemTier::F_Tier;

    /** The wired sub-stat the fusion bonus targets. The dropdown greys every sub-stat
     *  no read-site queries (only the wired six are selectable); None is also greyed
     *  and still rejected by validation as the backstop. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Fusion", EditConditionHides,
                      GetRestrictedEnumValues = "GetRestrictedFusionBonusStats"))
    ESubStat FusionBonusStat = ESubStat::None;
};
