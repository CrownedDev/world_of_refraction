#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CrystalInventoryComponent.generated.h"

/**
 * UCrystalInventoryComponent — count-based storage for refined crystals and
 * item crystals.
 *
 * Empty placeholder. Future commits will add:
 * - Count-table fields keyed by FCrystalId for ItemCrystals and RefinedCrystals
 * - Per-tier shared caps (20 per tier per pool, independent)
 * - Add / Remove / GetCount / HasCrystal methods
 * - Wiring to the character actor
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UCrystalInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCrystalInventoryComponent();
};
