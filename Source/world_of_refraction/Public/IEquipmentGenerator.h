// IEquipmentGenerator.h
// Shared interface for assets that expose CallInEditor stat-bonus roll
// buttons (UEquipmentDataBase, UItemData evolution crystals). Lets the
// generator logic stay co-located with each asset's own state while
// keeping a single contract for tooling and future callers.

#pragma once

#include "CoreMinimal.h"
#include "FEquipmentStatBonus.h"
#include "ItemTier.h"
#include "FPillarWeights.h"
#include "IEquipmentGenerator.generated.h"

UINTERFACE(MinimalAPI, BlueprintType,
    meta = (CannotImplementInterfaceInBlueprint))
class UEquipmentGenerator : public UInterface
{
    GENERATED_BODY()
};

class IEquipmentGenerator
{
    GENERATED_BODY()
public:
    // Roll substat points into StatBonus — only fires when
    // SubstatPoints >= tier budget
    virtual void RollSubstatPoints() = 0;

    // Roll pillar points into StatBonus — only fires when
    // PillarPoints >= tier budget
    virtual void RollPillarPoints() = 0;

    // Clear all generated bonus values
    virtual void ClearAllBonuses() = 0;

    // Access the stat bonus being generated
    virtual FEquipmentStatBonus& GetEditableStatBonus() = 0;

    // Get the tier for budget calculations
    virtual EItemTier GetGeneratorTier() const = 0;
};
