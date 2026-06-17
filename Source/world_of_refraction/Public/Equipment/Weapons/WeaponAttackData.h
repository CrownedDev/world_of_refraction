// WeaponAttackData.h
// Weapon attack data asset - defines all weapon attack properties.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/CastableSkillDataBase.h"
#include "Animation/AnimMontage.h"
#include "Combat/Damage/EPhysicalDamageType.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "WeaponAttackData.generated.h"

/**
 * Weapon Attack Data Asset
 * Defines attack properties including animation, damage distribution, and physical type.
 *
 * Inherits shared skill shape from USkillDataBase and cast shape
 * (Tier, TargetType, BaseDamage, BaseEnergyCost, Requirements) from
 * UCastableSkillDataBase. DeliveryType/ProjectileSpeed are inherited but
 * hidden in the editor via CanEditChange — attacks don't use projectile delivery.
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UWeaponAttackData : public UCastableSkillDataBase
{
    GENERATED_BODY()

public:
    // ==================== ANIMATION ====================

    /** DEPRECATED (D2, meta'd Stage 12 SC7): load-only — readers use
     *  SkillMontage; only the PostLoad mirror still reads this. Hard-delete
     *  at the post-SC8 resave bake. */
    UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (DeprecatedProperty))
    UAnimMontage *AttackMontage = nullptr;

    // BaseAnimSpeed hoisted to UCastableSkillDataBase (D7) — same name/default/
    // clamp, authored values load onto the inherited field (serialize-by-name).

    // ExecutionRange hoisted to USkillDataBase (root) + Icon hoisted to USkillDataBase
    // (step 2, attack/ability merge) — same name/type, authored values load onto the
    // inherited fields (serialize-by-name). ExecutionRange default reconciled 100→100.

    // ==================== UTILITY FUNCTIONS ====================

    UFUNCTION(BlueprintPure, Category = "Attack")
    FString GetDisplayName() const { return Name; }

    UFUNCTION(BlueprintPure, Category = "Attack")
    bool IsMultiHit() const { return HitCount > 1; }

    // CalculateAnimSpeed deleted (Stage 12 SC7) — superseded by the D7
    // BaseAnimSpeed direct read; zero C++ callers, BP binary-scan clean.

    UFUNCTION(BlueprintPure, Category = "Attack")
    FString GetAttackSummary() const;

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
