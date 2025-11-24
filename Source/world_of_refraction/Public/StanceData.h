// StanceData.h
// Stance data asset - idle pose customization

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimMontage.h"
#include "Engine/Texture2D.h"

#include "StanceData.generated.h"

/**
 * Stance Data Asset
 * Defines idle pose for characters (unarmed) and weapons (armed)
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UStanceData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString StanceName = TEXT("Unnamed Stance");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("");

    // ==================== ANIMATION ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage *IdleAnimMontage = nullptr;

    // ==================== PRESENTATION ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    UTexture2D *Icon = nullptr;

    // ==================== UTILITY ====================

    UFUNCTION(BlueprintPure, Category = "Stance")
    FString GetDisplayName() const { return StanceName; }

    UFUNCTION(BlueprintPure, Category = "Stance")
    bool HasAnimation() const { return IdleAnimMontage != nullptr; }
};