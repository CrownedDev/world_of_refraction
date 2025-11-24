// EvolutionData.h
// Evolution data asset - character transformations with stat changes, passives, and exclusive spells

#pragma once

#include "EStatModifierMode.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RefractionElement.h"
#include "EEvolutionType.h"
#include "PassiveEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "EvolutionData.generated.h"

// Forward declarations
class UCharacterData;
class UUltimateData;
class USpellData;

/**
 * Evolution Data Asset - Character transformations
 * Provides stat modifications, passive effects, exclusive spells, and ultimate overrides
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UEvolutionData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString EvolutionName = TEXT("Unnamed Evolution");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ERefractionElement Element = ERefractionElement::Fire;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EEvolutionType EvolutionType = EEvolutionType::Balanced;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("Evolution description...");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Lore = TEXT(""); // Flavor text

    // ==================== STAT MODIFIER MODE ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    EStatModifierMode StatModifierMode = EStatModifierMode::Pillar;

    // ==================== PILLAR MODIFIERS (affects all sub-stats) ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Pillar",
              meta = (ClampMin = "-50", ClampMax = "50", EditCondition = "StatModifierMode == EStatModifierMode::Pillar", EditConditionHides))
    float MindModifierPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Pillar",
              meta = (ClampMin = "-50", ClampMax = "50", EditCondition = "StatModifierMode == EStatModifierMode::Pillar", EditConditionHides))
    float BodyModifierPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Pillar",
              meta = (ClampMin = "-50", ClampMax = "50", EditCondition = "StatModifierMode == EStatModifierMode::Pillar", EditConditionHides))
    float SpiritModifierPercent = 0.0f;

    // ==================== SUB-STAT MODIFIERS (granular control) ====================

    // Mind Sub-Stats
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|SubStats|Mind",
              meta = (ClampMin = "-50", ClampMax = "50", EditCondition = "StatModifierMode == EStatModifierMode::SubStats", EditConditionHides))
    float CostReductionModifierPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|SubStats|Mind",
              meta = (ClampMin = "-50", ClampMax = "50", EditCondition = "StatModifierMode == EStatModifierMode::SubStats", EditConditionHides))
    float TurnSpeedModifierPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|SubStats|Mind",
              meta = (ClampMin = "-50", ClampMax = "50", EditCondition = "StatModifierMode == EStatModifierMode::SubStats", EditConditionHides))
    float CritChanceModifierPercent = 0.0f;

    // Body Sub-Stats
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|SubStats|Body",
              meta = (ClampMin = "-50", ClampMax = "50", EditCondition = "StatModifierMode == EStatModifierMode::SubStats", EditConditionHides))
    float DefenseModifierPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|SubStats|Body",
              meta = (ClampMin = "-50", ClampMax = "50", EditCondition = "StatModifierMode == EStatModifierMode::SubStats", EditConditionHides))
    float AttackSpeedModifierPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|SubStats|Body",
              meta = (ClampMin = "-50", ClampMax = "50", EditCondition = "StatModifierMode == EStatModifierMode::SubStats", EditConditionHides))
    float RawDamageModifierPercent = 0.0f;

    // Spirit Sub-Stats
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|SubStats|Spirit",
              meta = (ClampMin = "-50", ClampMax = "50", EditCondition = "StatModifierMode == EStatModifierMode::SubStats", EditConditionHides))
    float EffectDamageModifierPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|SubStats|Spirit",
              meta = (ClampMin = "-50", ClampMax = "50", EditCondition = "StatModifierMode == EStatModifierMode::SubStats", EditConditionHides))
    float ResistanceModifierPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|SubStats|Spirit",
              meta = (ClampMin = "-50", ClampMax = "50", EditCondition = "StatModifierMode == EStatModifierMode::SubStats", EditConditionHides))
    float AbilitySizeModifierPercent = 0.0f;
    // ==================== ULTIMATE OVERRIDE ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ultimate")
    bool bOverridesUltimate = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ultimate", meta = (EditCondition = "bOverridesUltimate"))
    UUltimateData *EvolutionUltimate = nullptr;

    // ==================== EXCLUSIVE SPELLS ====================

    // Spells granted by this evolution (up to 6)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells")
    TArray<USpellData *> ExclusiveSpells;

    // ==================== PASSIVE EFFECTS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passives")
    TArray<FPassiveEffect> PassiveEffects;

    // ==================== PRESENTATION ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    UTexture2D *EvolutionIcon = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    FLinearColor EvolutionColor = FLinearColor::White;

    // Visual transformation (mesh swap, material change, etc.)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    USkeletalMesh *TransformedMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    UMaterialInterface *TransformedMaterial = nullptr;

    // ==================== UTILITY FUNCTIONS ====================

    UFUNCTION(BlueprintPure, Category = "Evolution")
    FString GetDisplayName() const { return EvolutionName; }

    UFUNCTION(BlueprintPure, Category = "Evolution")
    FString GetElementName() const;

    UFUNCTION(BlueprintPure, Category = "Evolution")
    FString GetEvolutionTypeName() const;

    UFUNCTION(BlueprintPure, Category = "Evolution")
    FString GetStatModifierSummary() const;

    // ==================== REQUIREMENT FUNCTIONS ====================

    UFUNCTION(BlueprintPure, Category = "Evolution|Requirements")
    bool MeetsElementRequirement(const UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Evolution|Requirements")
    bool CanCharacterUse(const UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Evolution|Requirements")
    FString GetRequirementsString() const;

    // ==================== STAT CALCULATION ====================

    // Get modified Mind value
    UFUNCTION(BlueprintPure, Category = "Evolution|Stats")
    float CalculateModifiedMind(float BaseMind) const;

    // Get modified Body value
    UFUNCTION(BlueprintPure, Category = "Evolution|Stats")
    float CalculateModifiedBody(float BaseBody) const;

    // Get modified Spirit value
    UFUNCTION(BlueprintPure, Category = "Evolution|Stats")
    float CalculateModifiedSpirit(float BaseSpirit) const;

    // ==================== SUB-STAT CALCULATION ====================

    UFUNCTION(BlueprintPure, Category = "Evolution|Stats")
    float GetCostReductionModifier() const;

    UFUNCTION(BlueprintPure, Category = "Evolution|Stats")
    float GetTurnSpeedModifier() const;

    UFUNCTION(BlueprintPure, Category = "Evolution|Stats")
    float GetCritChanceModifier() const;

    UFUNCTION(BlueprintPure, Category = "Evolution|Stats")
    float GetDefenseModifier() const;

    UFUNCTION(BlueprintPure, Category = "Evolution|Stats")
    float GetAttackSpeedModifier() const;

    UFUNCTION(BlueprintPure, Category = "Evolution|Stats")
    float GetRawDamageModifier() const;

    UFUNCTION(BlueprintPure, Category = "Evolution|Stats")
    float GetEffectDamageModifier() const;

    UFUNCTION(BlueprintPure, Category = "Evolution|Stats")
    float GetResistanceModifier() const;

    UFUNCTION(BlueprintPure, Category = "Evolution|Stats")
    float GetAbilitySizeModifier() const;

    // ==================== PASSIVE HELPERS ====================

    UFUNCTION(BlueprintPure, Category = "Evolution|Passives")
    int32 GetPassiveCount() const { return PassiveEffects.Num(); }

    UFUNCTION(BlueprintPure, Category = "Evolution|Passives")
    TArray<FPassiveEffect> GetAlwaysActivePassives() const;

    UFUNCTION(BlueprintPure, Category = "Evolution|Passives")
    TArray<FPassiveEffect> GetTriggeredPassives() const;

    // ==================== SPELL HELPERS ====================

    UFUNCTION(BlueprintPure, Category = "Evolution|Spells")
    int32 GetExclusiveSpellCount() const { return ExclusiveSpells.Num(); }

    UFUNCTION(BlueprintPure, Category = "Evolution|Spells")
    bool HasExclusiveSpells() const { return ExclusiveSpells.Num() > 0; }

    // ==================== ULTIMATE HELPERS ====================

    UFUNCTION(BlueprintPure, Category = "Evolution|Ultimate")
    bool HasUltimateOverride() const { return bOverridesUltimate && EvolutionUltimate != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Evolution|Ultimate")
    UUltimateData *GetEvolutionUltimate() const { return bOverridesUltimate ? EvolutionUltimate : nullptr; }

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};