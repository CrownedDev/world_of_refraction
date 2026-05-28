// WeaponAttackData.h
// Weapon attack data asset - defines all weapon attack properties.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/CastableSkillDataBase.h"
#include "Animation/AnimMontage.h"
#include "Engine/Texture2D.h"
#include "Combat/Damage/EPhysicalDamageType.h"
#include "Character/MovementData.h"

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
    // ==================== COMBAT (Attack-specific) ====================

    /** Damage distribution per hit (percentages, should sum to ~100 for balance). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (EditCondition = "HitCount == 2"))
    float FirstHitPercent = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (EditCondition = "HitCount == 2"))
    float SecondHitPercent = 50.0f;

    // ==================== SIZE ====================

    /** Hitbox / defense-window size scalar for this attack. 0 = unauthored
     *  (logged as a warning at execution time; the executor uses the raw 0 — no
     *  fallback). Mirrors USpellData::BaseSize convention; designers should set
     *  it on every attack a player can defend against. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "0.0"))
    float BaseSize = 0.0f;

    // ==================== ANIMATION ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage *AttackMontage = nullptr;

    /** Animation playback speed (1.0 = normal, affected by Animation Speed stat) */
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

    // ==================== EDITOR ====================

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;

    /** Hide DeliveryType/ProjectileSpeed in the editor — attacks don't use projectile delivery. */
    virtual bool CanEditChange(const FProperty *InProperty) const override;
#endif
};
