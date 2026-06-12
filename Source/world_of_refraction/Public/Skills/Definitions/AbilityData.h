// AbilityData.h
// Ability Data Asset - Universal skills usable by all characters
// Can be infused with character's innate element for status effects

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/CastableSkillDataBase.h"
#include "Equipment/Weapons/EWeaponType.h"
#include "Combat/CombatConstants.h"
#include "Character/MovementData.h"
#include "Combat/Actions/EAbilityExecutionType.h"

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

    /** When true, usable ONLY on weapons whose WieldMode is Dual or
     *  OffHandShield. When false, usable on both single and dual weapons
     *  of the matching RequiredWeaponType. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    bool bRequiresDualWeapon = false;

    // ==================== EXECUTION ====================
    // ExecutionType hoisted to UCastableSkillDataBase (D3 descriptive tag).

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

    /** DEPRECATED (D2, meta'd Stage 12 SC7): load-only — readers use
     *  SkillMontage; only the PostLoad mirror still reads this. Hard-delete
     *  at the post-SC8 resave bake. */
    UPROPERTY(BlueprintReadOnly, Category = "Visuals", meta = (DeprecatedProperty))
    UAnimMontage *ExecutionMontage = nullptr;

    // ==================== DAMAGE CALCULATIONS ====================

    /** Attacker-side ability damage: BaseDamage * (1 - requirement penalty).
     *  RawDamage multiplier is applied once downstream by DamageCalculator;
     *  bIsInfused no longer affects the damage value (the element-infusion
     *  damage penalty was removed per the locked cost matrix). */
    UFUNCTION(BlueprintPure, Category = "Ability|Damage")
    int32 CalculateDamage(UCharacterData *Character, bool bIsInfused) const;

    /** @deprecated Use CalculateDamage(Character, false). Kept as a Blueprint
     *  forwarder so existing BP graphs continue to resolve; remove once those
     *  graphs are repointed. */
    UFUNCTION(BlueprintPure, Category = "Ability|Damage", meta=(DeprecatedFunction, DeprecationMessage="Use CalculateDamage(Character, false) instead."))
    int32 CalculateNormalDamage(UCharacterData *Character) const;

    /** @deprecated Use CalculateDamage(Character, true). Kept as a Blueprint
     *  forwarder so existing BP graphs continue to resolve; remove once those
     *  graphs are repointed. */
    UFUNCTION(BlueprintPure, Category = "Ability|Damage", meta=(DeprecatedFunction, DeprecationMessage="Use CalculateDamage(Character, true) instead."))
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

    // ==================== MIGRATION ====================

    // Outside WITH_EDITOR — the D2 montage migration must run in all builds.
    virtual void PostLoad() override;

    // ==================== EDITOR ====================

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
    // CanEditChange hiding override removed (SC7) — the delivery fields are
    // DeprecatedProperty now, hidden everywhere.
#endif
};
