// AbilityData.h
// Ability Data Asset - Universal skills usable by all characters
// Can be infused with character's innate element for status effects

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TargetType.h"
#include "EStatusType.h"
#include "WorldStatRequirements.h"
#include "NiagaraSystem.h"
#include "EWeaponType.h"
#include "CombatConstants.h"
#include "MovementData.h"
#include "EAbilityExecutionType.h"
#include "FAbilityEffect.h"
#include "ESpellDeliveryType.h"
#include "ItemTier.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "AbilityData.generated.h"

// Forward declaration
class UCharacterData;

/** Maximum number of effects an ability can have */
constexpr int32 MAX_ABILITY_EFFECTS = 5;

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EWeaponType RequiredWeaponType = EWeaponType::Sword;

    /** Tier for break calculations (mirrors USpellData::Tier) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EItemTier Tier = EItemTier::E_Tier;

    // ==================== EXECUTION ====================

    /** How this ability executes (determines movement, animation style, delivery) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execution")
    EAbilityExecutionType ExecutionType = EAbilityExecutionType::Melee;

    // --- Melee Only ---

    /** How the user approaches the target (Melee only, nullptr = use character default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execution|Melee",
              meta = (EditCondition = "ExecutionType == EAbilityExecutionType::Melee", EditConditionHides))
    UMovementData *ApproachData = nullptr;

    /** Distance from target to stop and execute ability (Melee only) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execution|Melee",
              meta = (EditCondition = "ExecutionType == EAbilityExecutionType::Melee", EditConditionHides, ClampMin = "0.0"))
    float ExecutionRange = 150.0f;

    // --- Ranged Only ---

    /** How the projectile travels (Ranged only) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execution|Ranged",
              meta = (EditCondition = "ExecutionType == EAbilityExecutionType::Ranged", EditConditionHides))
    ESpellDeliveryType DeliveryType = ESpellDeliveryType::Projectile;

    /** Projectile travel speed in units/second (Ranged only) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execution|Ranged",
              meta = (EditCondition = "ExecutionType == EAbilityExecutionType::Ranged", EditConditionHides, ClampMin = "100.0"))
    float ProjectileSpeed = 1500.0f;

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
    FWorldStatRequirements Requirements;

    // ==================== STATUS ====================

    /** Raw mode: folds StatusBuildup into BaseDamage at the orchestrator boundary; status bar doesn't move. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
    bool bIsRawMode = false;

    /** Per-hit status buildup amount. Read by the orchestrator when wiring ability buildup
     *  through OpenDefenseWindowsForTargets. Defaults to 0 — existing assets continue to
     *  not build status until designers populate this value. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status",
              meta = (EditCondition = "!bIsRawMode", EditConditionHides, ClampMin = "0"))
    int32 StatusBuildup = 0;

    // ==================== INFUSION ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
    bool bCanBeInfused = true;

    // ==================== EFFECTS ====================

    /**
     * Effects applied by this ability (max 5)
     * Each effect can have its own target, condition, and timing
     * Examples: Self buff on hit, team buff always, debuff on target
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects",
              meta = (TitleProperty = "EffectType"))
    TArray<FAbilityEffect> Effects;

    // ==================== VISUALS ====================

    /** Animation to play during ability execution */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UAnimMontage *ExecutionMontage = nullptr;

    /** VFX for normal (non-infused) ability */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UNiagaraSystem *NormalVFX = nullptr;

    /** VFX for infused ability */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UNiagaraSystem *InfusedVFX = nullptr;

    /** Projectile VFX (Ranged only) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals",
              meta = (EditCondition = "ExecutionType == EAbilityExecutionType::Ranged", EditConditionHides))
    UNiagaraSystem *ProjectileVFX = nullptr;

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

    // ==================== EFFECT HELPERS ====================

    /** Get all effects that trigger on a specific condition */
    UFUNCTION(BlueprintPure, Category = "Ability|Effects")
    TArray<FAbilityEffect> GetEffectsForCondition(EPassiveTrigger Condition) const;

    /** Check if ability has any drain effects */
    UFUNCTION(BlueprintPure, Category = "Ability|Effects")
    bool HasDrainEffect() const;

    /** Check if ability has any buff effects */
    UFUNCTION(BlueprintPure, Category = "Ability|Effects")
    bool HasBuffEffects() const;

    /** Check if ability has any debuff effects */
    UFUNCTION(BlueprintPure, Category = "Ability|Effects")
    bool HasDebuffEffects() const;

    // ==================== EXECUTION HELPERS ====================

    /** Is this a ranged ability? */
    UFUNCTION(BlueprintPure, Category = "Ability|Execution")
    bool IsRanged() const { return ExecutionType == EAbilityExecutionType::Ranged; }

    /** Is this a melee ability? */
    UFUNCTION(BlueprintPure, Category = "Ability|Execution")
    bool IsMelee() const { return ExecutionType == EAbilityExecutionType::Melee; }

    /** Is this a support ability (no damage, has buff effects)? */
    UFUNCTION(BlueprintPure, Category = "Ability|Execution")
    bool IsSupportAbility() const { return BaseDamage == 0 && HasBuffEffects(); }

    /** Does this ability require approaching the target? */
    UFUNCTION(BlueprintPure, Category = "Ability|Execution")
    bool RequiresApproach() const { return AbilityExecutionTypeHelper::RequiresApproach(ExecutionType); }

    // ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};
