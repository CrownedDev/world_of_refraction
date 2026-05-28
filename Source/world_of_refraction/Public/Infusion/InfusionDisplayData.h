// InfusionDisplayData.h
// Base infusion display data asset - shared properties

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "NiagaraSystem.h"
#include "Infusion/EInfusionDisplayLocation.h"
#include "InfusionDisplayData.generated.h"

/**
 * Base Infusion Display Data Asset
 * Shared properties for character and weapon infusion visuals
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UInfusionDisplayData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString DisplayName = TEXT("Unnamed Infusion Display");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("");

    // ==================== DISPLAY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
    UNiagaraSystem *VFXSystem = nullptr;

    /** Where this VFX spawns on the character */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
    EInfusionDisplayLocation DisplayLocation = EInfusionDisplayLocation::Weapon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float EffectIntensity = 1.0f;

    // Optional: Override element color
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
    bool bOverrideElementColor = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display", meta = (EditCondition = "bOverrideElementColor"))
    FLinearColor ColorOverride = FLinearColor::White;

    // ==================== PRESENTATION ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    UTexture2D *Icon = nullptr;

    // ==================== UTILITY ====================

    UFUNCTION(BlueprintPure, Category = "InfusionDisplay")
    bool HasVFX() const { return VFXSystem != nullptr; }
};