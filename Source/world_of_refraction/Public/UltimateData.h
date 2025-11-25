// UltimateData.h
// Ultimate ability data asset for World of Refraction

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RefractionElement.h"
#include "TargetType.h"
#include "AbilityEffectType.h"
#include "EUltimateType.h"
#include "EUltimateCooldownType.h"
#include "EStatScalingType.h"
#include "StatConstants.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "UltimateData.generated.h"

// Forward declaration
class UCharacterData;

/**
 * Ultimate Data Asset - Powerful cinematic abilities
 * Can be equipped by characters or granted by evolutions
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UUltimateData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString UltimateName = TEXT("Unnamed Ultimate");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ERefractionElement Element = ERefractionElement::Fire;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EUltimateType UltimateType = EUltimateType::Damage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("Ultimate ability description...");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString Theme = TEXT(""); // Flavor text for cinematics

    // ==================== REQUIREMENTS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements",
              meta = (ClampMin = "StatConstants::MIN_WORLD_STAT_LEVEL", ClampMax = "StatConstants::MAX_WORLD_STAT_LEVEL"))
    int32 RequiredWorldMind = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements",
              meta = (ClampMin = "StatConstants::MIN_WORLD_STAT_LEVEL", ClampMax = "StatConstants::MAX_WORLD_STAT_LEVEL"))
    int32 RequiredWorldBody = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements",
              meta = (ClampMin = "StatConstants::MIN_WORLD_STAT_LEVEL", ClampMax = "StatConstants::MAX_WORLD_STAT_LEVEL"))
    int32 RequiredWorldSpirit = 0;

    // ==================== COOLDOWN ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooldown")
    EUltimateCooldownType CooldownType = EUltimateCooldownType::OncePerBattle;

    // Only used if CooldownType == TurnBased
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooldown", meta = (EditCondition = "CooldownType == EUltimateCooldownType::TurnBased", ClampMin = "1", ClampMax = "20"))
    int32 TurnCooldown = 10;

    // ==================== COST ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (ClampMin = "0", ClampMax = "150"))
    int32 EnergyCost = 80;

    // ==================== TARGETING ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targeting")
    ETargetType TargetType = ETargetType::SingleEnemy;

    // ==================== PRIMARY EFFECT ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Primary")
    EAbilityEffectType PrimaryEffectType = EAbilityEffectType::None;

    // Base damage (before multipliers)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Primary", meta = (ClampMin = "0"))
    int32 BaseDamage = 300;

    // Base healing (for heal-type ultimates)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Primary", meta = (ClampMin = "0"))
    int32 BaseHealing = 0;

    // Buff/debuff percentage
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Primary", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float EffectPercentage = 0.0f;

    // Effect duration in turns
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Primary", meta = (ClampMin = "0"))
    int32 EffectDuration = 0;

    // ==================== SECONDARY EFFECT ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Secondary")
    bool bHasSecondaryEffect = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Secondary", meta = (EditCondition = "bHasSecondaryEffect"))
    EAbilityEffectType SecondaryEffectType = EAbilityEffectType::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Secondary", meta = (EditCondition = "bHasSecondaryEffect", ClampMin = "0"))
    int32 SecondaryValue = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Secondary", meta = (EditCondition = "bHasSecondaryEffect", ClampMin = "0"))
    int32 SecondaryDuration = 0;

    // ==================== STATUS EFFECT ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Status")
    bool bAppliesStatus = false;

    // Status buildup amount (triggers at 100)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect|Status", meta = (EditCondition = "bAppliesStatus", ClampMin = "0", ClampMax = "100"))
    int32 StatusBuildup = 0;

    // ==================== SCALING ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scaling")
    EStatScalingType ScalingType = EStatScalingType::Spirit;

    // ==================== SPECIAL FLAGS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Special")
    bool bIgnoresDefense = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Special")
    bool bCanCrit = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Special")
    bool bGrantsInvulnerability = false; // During animation

    // ==================== PRESENTATION ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    UTexture2D *Icon = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    FLinearColor UltimateColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    USoundBase *ActivationSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    UParticleSystem *UltimateVFX = nullptr;

    // Animation montage for cinematic
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    UAnimMontage *UltimateAnimation = nullptr;

    // ==================== UTILITY FUNCTIONS ====================

    UFUNCTION(BlueprintPure, Category = "Ultimate")
    FString GetDisplayName() const { return UltimateName; }

    UFUNCTION(BlueprintPure, Category = "Ultimate")
    FString GetElementName() const;

    UFUNCTION(BlueprintPure, Category = "Ultimate")
    FString GetUltimateTypeName() const;

    UFUNCTION(BlueprintPure, Category = "Ultimate")
    FString GetCooldownDescription() const;

    UFUNCTION(BlueprintPure, Category = "Ultimate")
    FString GetScalingTypeName() const;

    UFUNCTION(BlueprintPure, Category = "Ultimate")
    bool IsOncePerBattle() const { return CooldownType == EUltimateCooldownType::OncePerBattle; }

    // ==================== REQUIREMENT FUNCTIONS ====================

    // Check if character meets element requirements
    UFUNCTION(BlueprintPure, Category = "Ultimate|Requirements")
    bool MeetsElementRequirement(const UCharacterData *Character) const;

    // Check if character meets world stat requirements
    UFUNCTION(BlueprintPure, Category = "Ultimate|Requirements")
    bool MeetsWorldStatRequirements(const UCharacterData *Character) const;

    // Check if character can use this ultimate (element + world stats)
    UFUNCTION(BlueprintPure, Category = "Ultimate|Requirements")
    bool CanCharacterUse(const UCharacterData *Character) const;

    // Get requirements as display string
    UFUNCTION(BlueprintPure, Category = "Ultimate|Requirements")
    FString GetRequirementsString() const;

    // ==================== CALCULATION FUNCTIONS ====================

    // Calculate damage with character stats
    UFUNCTION(BlueprintPure, Category = "Ultimate|Calculation")
    float CalculateDamage(const UCharacterData *Character) const;

    // Calculate healing with character stats
    UFUNCTION(BlueprintPure, Category = "Ultimate|Calculation")
    float CalculateHealing(const UCharacterData *Character) const;

    // Calculate bonus crit chance (Mind scaling)
    UFUNCTION(BlueprintPure, Category = "Ultimate|Calculation")
    float CalculateBonusCritChance(const UCharacterData *Character) const;

    // Calculate crit damage multiplier (Mind scaling)
    UFUNCTION(BlueprintPure, Category = "Ultimate|Calculation")
    float CalculateCritMultiplier(const UCharacterData *Character) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(TArray<FText> &ValidationErrors) override;
#endif
};