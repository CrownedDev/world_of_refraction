// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RefractionElement.h"
#include "SpellSchool.h"
#include "TargetType.h"
#include "AbilityEffectType.h"
#include <world_of_refraction/CombatConstants.h>

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "SpellData.generated.h"

// Forward declaration
class UCharacterData;

/**
 * Spell Data Asset - Element-locked magical abilities
 * Supports mode toggle system (Elemental vs Raw/Construct)
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API USpellData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString SpellName = TEXT("Unnamed Spell");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ERefractionElement Element = ERefractionElement::Fire;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ESpellSchool School = ESpellSchool::Destruction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("Spell description...");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    bool bIsUniversalSpell = false; // Can be cast by any element (except Generic)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (EditCondition = "bIsUniversalSpell"))
    bool bPrependElementName = false; // "Elemental" becomes "Fire Elemental"

    // ==================== MODE TOGGLE ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode")
    bool bHasModeToggle = false;

    // ELEMENTAL MODE (if toggle available)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode|Elemental", meta = (EditCondition = "bHasModeToggle"))
    int32 ElementalModeDamage = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode|Elemental", meta = (EditCondition = "bHasModeToggle"))
    int32 ElementalModeEnergyCost = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode|Elemental", meta = (EditCondition = "bHasModeToggle"))
    int32 ElementalModeStatusBuildup = 15;

    // RAW/CONSTRUCT MODE (if toggle available)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode|Raw", meta = (EditCondition = "bHasModeToggle"))
    int32 RawModeDamage = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode|Raw", meta = (EditCondition = "bHasModeToggle"))
    int32 RawModeEnergyCost = 0;

    // NO MODE TOGGLE (single configuration)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (EditCondition = "!bHasModeToggle"))
    int32 BaseDamage = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (EditCondition = "!bHasModeToggle"))
    int32 BaseEnergyCost = 0;

    // ==================== COMMON STATS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    int32 HitCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    ETargetType TargetType = ETargetType::SingleEnemy;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    int32 TurnCost = 1; // Future: multi-turn casting

    // ==================== REQUIREMENTS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements", meta = (ClampMin = "0"))
    int32 RequiredMind = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements", meta = (ClampMin = "0"))
    int32 RequiredBody = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements", meta = (ClampMin = "0"))
    int32 RequiredSpirit = 0;

    // ==================== PRIMARY EFFECTS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|Primary")
    EAbilityEffectType PrimaryEffect = EAbilityEffectType::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|Primary", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PrimaryEffectMagnitude = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|Primary", meta = (ClampMin = "0"))
    int32 PrimaryEffectDuration = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|Primary")
    int32 PrimaryEffectValue = 0;

    // ==================== SECONDARY EFFECTS (CROSS-SCHOOL) ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|Secondary")
    EAbilityEffectType SecondaryEffect = EAbilityEffectType::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|Secondary", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SecondaryEffectMagnitude = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|Secondary", meta = (ClampMin = "0"))
    int32 SecondaryEffectDuration = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|Secondary")
    int32 SecondaryEffectValue = 0;

    // ==================== VISUALS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UAnimMontage *CastAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UParticleSystem *ElementalModeEffect = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UParticleSystem *RawModeEffect = nullptr;

    // ==================== REQUIREMENT CHECKS ====================

    UFUNCTION(BlueprintPure, Category = "Spell|Requirements")
    bool MeetsRequirements(UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Spell|Requirements")
    int32 GetTotalDeficit(UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Spell|Requirements")
    float CalculateRequirementPenalty(UCharacterData *Character) const;

    // ==================== DAMAGE CALCULATIONS ====================

    UFUNCTION(BlueprintPure, Category = "Spell|Damage")
    int32 CalculateDamage(UCharacterData *Character, bool bUsingElementalMode) const;

    UFUNCTION(BlueprintPure, Category = "Spell|Damage")
    int32 CalculateElementalModeDamage(UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Spell|Damage")
    int32 CalculateRawModeDamage(UCharacterData *Character) const;

    // ==================== ENERGY CALCULATIONS ====================

    UFUNCTION(BlueprintPure, Category = "Spell|Energy")
    int32 CalculateEnergyCost(UCharacterData *Character, bool bUsingElementalMode) const;

    UFUNCTION(BlueprintPure, Category = "Spell|Energy")
    int32 CalculateElementalModeEnergyCost(UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Spell|Energy")
    int32 CalculateRawModeEnergyCost(UCharacterData *Character) const;

    // ==================== STATUS BUILDUP ====================

    UFUNCTION(BlueprintPure, Category = "Spell|Status")
    int32 CalculateStatusBuildup(UCharacterData *Character) const;

    // ==================== HELPER FUNCTIONS ====================

    UFUNCTION(BlueprintPure, Category = "Spell|Helpers")
    FString GetRequirementSummary(UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Spell|Helpers")
    bool CanCharacterCast(UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Spell|Display")
    FString GetDisplayName(UCharacterData *Caster) const;

    // ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};