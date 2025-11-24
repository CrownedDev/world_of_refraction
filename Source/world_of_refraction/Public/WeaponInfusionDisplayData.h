// WeaponInfusionDisplayData.h
// Weapon infusion display - effects on weapons

#pragma once

#include "CoreMinimal.h"
#include "InfusionDisplayData.h"

#include "WeaponInfusionDisplayData.generated.h"

/**
 * Weapon Infusion Display Data Asset
 * Visual effects for weapon infusion
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UWeaponInfusionDisplayData : public UInfusionDisplayData
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "InfusionDisplay")
    FString GetDisplayTypeName() const
    {
        return TEXT("Weapon");
    }
};