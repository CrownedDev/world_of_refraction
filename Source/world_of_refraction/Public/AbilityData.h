// AbilityData.h
// Ability Data Asset - Universal skills usable by all characters
// Can be infused with character's innate element for status effects

#pragma once

#include "CoreMinimal.h"
#include "CastableSkillDataBase.h"
#include "NiagaraSystem.h"
#include "EWeaponType.h"
#include "CombatConstants.h"
#include "MovementData.h"
#include "EAbilityExecutionType.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "AbilityData.generated.h"

// Forward declaration
class UCharacterData;

/**
 * Ability Data Asset - Universal skills usable by all characters
 * Can be infused with character's innate element for status effects.
 *
 * Inherits the shared skill shape (Name, HitCount, Effects, …) from USkillDataBase
 * and the cast shape (Tier, TargetType, BaseDamage, BaseEnergyCost, Requirements,
 * DeliveryType, ProjectileSpeed) from UCastableSkillDataBase.
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UAbilityData : public UCastableSkillDataBase
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EWeaponType RequiredWeaponType = EWeaponType::Sword;

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

    /** Projectile VFX (Ranged only). Ability-specific; spells use SpellVFX. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals",
              meta = (EditCondition = "ExecutionType == EAbilityExecutionType::Ranged", EditConditionHides))
    UNiagaraSystem *ProjectileVFX = nullptr;

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

    // ==================== EDITOR ====================

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;

    /** Hide DeliveryType/ProjectileSpeed in editor when not in Ranged mode. */
    virtual bool CanEditChange(const FProperty *InProperty) const override;
#endif
};
