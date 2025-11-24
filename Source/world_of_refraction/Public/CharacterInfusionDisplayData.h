// CharacterInfusionDisplayData.h
// Character infusion display - Body or Aura effects

#pragma once

#include "CoreMinimal.h"
#include "InfusionDisplayData.h"
#include "ECharacterInfusionDisplayType.h"

#include "CharacterInfusionDisplayData.generated.h"

/**
 * Character Infusion Display Data Asset
 * Body or Aura effects for character infusion visuals
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UCharacterInfusionDisplayData : public UInfusionDisplayData
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
    ECharacterInfusionDisplayType DisplayType = ECharacterInfusionDisplayType::Body;

    UFUNCTION(BlueprintPure, Category = "InfusionDisplay")
    FString GetDisplayTypeName() const
    {
        switch (DisplayType)
        {
        case ECharacterInfusionDisplayType::Body:
            return TEXT("Body");
        case ECharacterInfusionDisplayType::Aura:
            return TEXT("Aura");
        default:
            return TEXT("Unknown");
        }
    }
};