// WeaponAttackData.h
// Weapon attack data asset - defines all weapon attack properties

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimMontage.h"
#include "Engine/Texture2D.h"
#include "EPhysicalDamageType.h"
#include "MovementData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "WeaponAttackData.generated.h"

/**
 * Weapon Attack Data Asset
 * Defines attack properties including animation, damage distribution, and physical type
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UWeaponAttackData : public UPrimaryDataAsset
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
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (EditCondition = "HitCount == 2"))
    float FirstHitPercent = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (EditCondition = "HitCount == 2"))
    float SecondHitPercent = 50.0f;

    // Energy cost when attack is infused with element
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0"))
    float InfusionEnergyCost = 10.0f;

    // ==================== PHYSICAL DAMAGE TYPE ====================

    // Physical damage type - determines status effect (Bleed/Stun/ArmorBreak)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Type")
    EPhysicalDamageType PhysicalDamageType = EPhysicalDamageType::Slash;

    // Status buildup per hit (fills status bar)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Type", meta = (ClampMin = "0"))
    int32 StatusBuildup = 10;

    // ==================== ANIMATION ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage *AttackMontage = nullptr;

    // Animation playback speed (1.0 = normal, affected by Animation Speed stat)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float BaseAnimSpeed = 1.0f;

    // ==================== MOVEMENT ====================

    /** How the attacker approaches the target (nullptr = use character default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
    UMovementData *MovementData = nullptr;

    /** Distance from target to stop and execute attack (units) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
    float ExecutionRange = 100.0f;

    // ==================== PRESENTATION ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    UTexture2D *Icon = nullptr;

    // ==================== UTILITY FUNCTIONS ====================

    UFUNCTION(BlueprintPure, Category = "Attack")
    FString GetDisplayName() const { return AttackName; }

    UFUNCTION(BlueprintPure, Category = "Attack")
    bool IsMultiHit() const { return HitCount > 1; }

    UFUNCTION(BlueprintPure, Category = "Attack")
    float GetHitDamagePercent(int32 HitIndex) const;

    UFUNCTION(BlueprintPure, Category = "Attack")
    float GetTotalDamagePercent() const;

    UFUNCTION(BlueprintPure, Category = "Attack")
    float CalculateAnimSpeed(float AnimationSpeedMultiplier) const;

    UFUNCTION(BlueprintPure, Category = "Attack")
    FString GetAttackSummary() const;

    UFUNCTION(BlueprintPure, Category = "Attack")
    FString GetDamageTypeName() const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};