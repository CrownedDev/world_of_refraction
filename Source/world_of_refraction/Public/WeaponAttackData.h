// WeaponAttackData.h
// Weapon attack data asset - defines all weapon attack properties

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimMontage.h"
#include "Engine/Texture2D.h"
#include "EPhysicalDamageType.h"
#include "EStatusType.h"
#include "MovementData.h"
#include "FSkillEffect.h"

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
    FString Name = TEXT("Unnamed Attack");

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

    // Raw mode: folds StatusBuildup into BaseDamage at the orchestrator boundary; status bar doesn't move.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Type")
    bool bIsRawMode = false;

    // Status buildup per hit (fills status bar)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Type",
              meta = (EditCondition = "!bIsRawMode", EditConditionHides, ClampMin = "0"))
    int32 StatusBuildup = 10;

    // ==================== INFUSION ====================

    // If true, the orchestrator rejects this attack when bIsInfused (or an infusion source) is set.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
    bool bImmuneToInfusion = false;

    // ==================== EFFECTS ====================

    /**
     * Effects applied by this attack (max 5).
     * Empty by default — most physical attacks have no triggered effects.
     * Future attack designs (e.g. weapon with built-in bleed buff) populate here.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects",
              meta = (TitleProperty = "EffectType"))
    TArray<FSkillEffect> Effects;

    // ==================== ANIMATION ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage *AttackMontage = nullptr;

    // Animation playback speed (1.0 = normal, affected by Animation Speed stat)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float BaseAnimSpeed = 1.0f;

    // ==================== MOVEMENT ====================

    /** How the attacker approaches the target (nullptr = use character default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
    UMovementData *ApproachData = nullptr;

    /** Distance from target to stop and execute attack (units) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
    float ExecutionRange = 100.0f;

    // ==================== PRESENTATION ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    UTexture2D *Icon = nullptr;

    // ==================== UTILITY FUNCTIONS ====================

    UFUNCTION(BlueprintPure, Category = "Attack")
    FString GetDisplayName() const { return Name; }

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

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};