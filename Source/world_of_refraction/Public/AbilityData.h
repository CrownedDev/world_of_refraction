#pragma once
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TargetType.h"
#include "AbilityEffectType.h"
#include <world_of_refraction/CombatConstants.h>

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "AbilityData.generated.h"

// Forward declaration
class UCharacterData;

/**
 * Ability Data Asset - Universal skills usable by all characters
 * Can be infused with character's innate element for status effects
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UAbilityData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString AbilityName = TEXT("Unnamed Ability");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("Ability description...");

    // ==================== MECHANICS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mechanics")
    int32 BaseDamage = 50;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mechanics")
    int32 BaseEnergyCost = 20;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mechanics")
    int32 HitCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mechanics")
    ETargetType TargetType = ETargetType::SingleEnemy;

    // ==================== REQUIREMENTS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements", meta = (ClampMin = "0"))
    int32 RequiredMind = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements", meta = (ClampMin = "0"))
    int32 RequiredBody = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements", meta = (ClampMin = "0"))
    int32 RequiredSpirit = 0;

    // ==================== INFUSION ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
    bool bCanBeInfused = true;

    // ==================== EFFECTS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    EAbilityEffectType EffectType = EAbilityEffectType::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float EffectMagnitude = 0.0f; // 0.2 = 20% buff/debuff

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects", meta = (ClampMin = "0"))
    int32 EffectDuration = 0; // Turns

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    int32 EffectValue = 0; // For flat values (energy restore, etc.)

    // ==================== VISUALS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UAnimMontage *CastAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UParticleSystem *NormalEffect = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UParticleSystem *InfusedEffect = nullptr;

    // ==================== REQUIREMENT CHECKS ====================

    UFUNCTION(BlueprintPure, Category = "Ability|Requirements")
    bool MeetsRequirements(UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Ability|Requirements")
    int32 GetTotalDeficit(UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Ability|Requirements")
    float CalculateRequirementPenalty(UCharacterData *Character) const;

    // ==================== DAMAGE CALCULATIONS ====================

    UFUNCTION(BlueprintPure, Category = "Ability|Damage")
    int32 CalculateDamage(UCharacterData *Character, bool bIsInfused) const;

    UFUNCTION(BlueprintPure, Category = "Ability|Damage")
    int32 CalculateNormalDamage(UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Ability|Damage")
    int32 CalculateInfusedDamage(UCharacterData *Character) const;

    // ==================== ENERGY CALCULATIONS ====================

    UFUNCTION(BlueprintPure, Category = "Ability|Energy")
    int32 CalculateEnergyCost(UCharacterData *Character, bool bIsInfused) const;

    UFUNCTION(BlueprintPure, Category = "Ability|Energy")
    int32 CalculateNormalEnergyCost(UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Ability|Energy")
    int32 CalculateInfusedEnergyCost(UCharacterData *Character) const;

    // ==================== STATUS BUILDUP ====================

    UFUNCTION(BlueprintPure, Category = "Ability|Status")
    int32 CalculateStatusBuildup(UCharacterData *Character) const;

    // ==================== HELPER FUNCTIONS ====================

    UFUNCTION(BlueprintPure, Category = "Ability|Helpers")
    FString GetRequirementSummary(UCharacterData *Character) const;

    // ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};