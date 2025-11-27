// BaseAttackData.h
// Base attack data asset - defines unarmed and weapon attacks

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimMontage.h"
#include "Engine/Texture2D.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "BaseAttackData.generated.h"

/**
 * Base Attack Data Asset
 * Defines attack properties for unarmed and weapon attacks
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UBaseAttackData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString AttackName = TEXT("Unnamed Attack");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("");

    // ==================== COMBAT ====================

    // Number of hits (1 or 2)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "1", ClampMax = "2"))
    int32 HitCount = 1;

    // Damage distribution per hit (percentages, should sum to ~100 for balance)
    // Single hit: [100], Double hit: [50, 50] or [60, 40], etc.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (EditCondition = "HitCount == 2"))
    float FirstHitPercent = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (EditCondition = "HitCount == 2"))
    float SecondHitPercent = 50.0f;

    // Energy cost when attack is infused with element
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0"))
    float InfusionEnergyCost = 10.0f;

    // ==================== ANIMATION ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage *AttackMontage = nullptr;

    // Animation playback speed (1.0 = normal, affected by Attack Speed stat)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float BaseAnimSpeed = 1.0f;

    // ==================== PRESENTATION ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    UTexture2D *Icon = nullptr;

    // ==================== UTILITY FUNCTIONS ====================

    UFUNCTION(BlueprintPure, Category = "Attack")
    FString GetDisplayName() const { return AttackName; }

    UFUNCTION(BlueprintPure, Category = "Attack")
    bool IsMultiHit() const { return HitCount > 1; }

    // Get damage multiplier for specific hit (0-indexed)
    UFUNCTION(BlueprintPure, Category = "Attack")
    float GetHitDamagePercent(int32 HitIndex) const;

    // Get total damage percent (for validation)
    UFUNCTION(BlueprintPure, Category = "Attack")
    float GetTotalDamagePercent() const;

    // Calculate actual anim speed with character's Attack Speed stat
    UFUNCTION(BlueprintPure, Category = "Attack")
    float CalculateAnimSpeed(float AttackSpeedMultiplier) const;

    // Get formatted attack summary
    UFUNCTION(BlueprintPure, Category = "Attack")
    FString GetAttackSummary() const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};