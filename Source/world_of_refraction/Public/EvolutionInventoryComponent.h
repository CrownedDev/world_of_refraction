#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EvolutionInventoryComponent.generated.h"

/**
 * UEvolutionInventoryComponent — instance-based storage for evolution items.
 *
 * Empty placeholder. Future commits will add:
 * - TArray<FEvolutionInventoryEntry> storing owned evolution items with FGuid InstanceIDs
 * - Hard cap of 5 (MAX_EVOLUTION_ITEMS)
 * - Add / Remove / GetByInstanceID / Contains methods
 * - Trading API hooks (move instance between components)
 * - Wiring to the character actor
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UEvolutionInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEvolutionInventoryComponent();
};
