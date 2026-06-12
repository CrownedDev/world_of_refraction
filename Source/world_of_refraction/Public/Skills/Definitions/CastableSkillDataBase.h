// CastableSkillDataBase.h
// Intermediate base for skills that are "cast" against targets — abilities, spells,
// and weapon attacks. Adds targeting, damage, energy cost, requirements, tier, and
// delivery fields on top of USkillDataBase.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Combat/TargetType.h"
#include "Inventory/ItemTier.h"
#include "Skills/Definitions/WorldStatRequirements.h"
#include "Skills/Definitions/ESpellDeliveryType.h"
#include "Combat/Actions/EAbilityExecutionType.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "CastableSkillDataBase.generated.h"

class UCharacterData;
class UAnimMontage;

/**
 * UCastableSkillDataBase
 * Intermediate base between USkillDataBase and the concrete skill assets.
 * Holds fields that are shared by abilities, spells, and weapon attacks once
 * weapon attacks gained their own tier/requirements/base damage (Phase 4).
 *
 * DeliveryType / ProjectileSpeed are declared here so any subclass can opt
 * in via EditCondition. WeaponAttackData hides them; AbilityData shows them
 * only in Ranged mode; SpellData shows them always.
 */
UCLASS(Abstract, BlueprintType)
class WORLD_OF_REFRACTION_API UCastableSkillDataBase : public USkillDataBase
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    /** Tier for break calculations and difficulty scaling. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EItemTier Tier = EItemTier::E_Tier;

    // ==================== TARGETING ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targeting")
    ETargetType TargetType = ETargetType::SingleEnemy;

    // ==================== COMBAT ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0"))
    int32 BaseDamage = 0;

    /** Default 0 means free. Attacks are free unless designers set a cost. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0"))
    int32 BaseEnergyCost = 0;

    /** Turns before this skill fires after arming. 0 = fires this turn (no
     *  deferral). N = arms now, fires at the start of the turn N global turns
     *  ahead (the deferral mechanism is D8 Stage 8b/8c). FIFO if multiple land
     *  the same turn. Renamed/hoisted from SpellData.TurnCost (CoreRedirect);
     *  old default-1 spell assets self-migrate to 0 via delta serialization. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0"))
    int32 ActivationDelay = 0;

    // ==================== EXECUTION ====================

    /** DESCRIPTIVE tag (D3): how this skill executes, for UI / AI filtering /
     *  categorization — uniform across abilities, spells, and weapon attacks.
     *  Still read by the legacy approach path (RequiresApproach/IsMelee) until
     *  the fused-montage runner owns movement (D4); the unhook is runner-gated.
     *  Default Melee preserves existing ability assets (serialize-by-name);
     *  USpellData overrides to Ranged in its constructor. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execution")
    EAbilityExecutionType ExecutionType = EAbilityExecutionType::Melee;

    // ==================== DELIVERY ====================

    /**
     * How the skill travels from user to target.
     * Visibility is controlled by subclass-specific EditCondition meta:
     *   - AbilityData: visible when ExecutionType == Ranged
     *   - SpellData:   always visible
     *   - WeaponAttackData: hidden (handled by subclass override)
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery")
    ESpellDeliveryType DeliveryType = ESpellDeliveryType::Projectile;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery", meta = (ClampMin = "100.0"))
    float ProjectileSpeed = 1500.0f;

    // ==================== ANIMATION ====================

    /** The unified skill montage (D2) — the field the fused-montage runner
     *  plays at Stage 12. Populated via PostLoad migration from the leaf
     *  montage fields (CastAnimation / ExecutionMontage / AttackMontage) until
     *  readers switch; the leaf fields stay runtime-authoritative until then. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage *SkillMontage = nullptr;

    // ==================== REQUIREMENTS ====================

    // ShowOnlyInnerProperties: inline the struct's fields under the category
    // header — without it the panel shows "Requirements" twice (category +
    // identically-named member row). Inherited by USpellData / UAbilityData.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements",
              meta = (ShowOnlyInnerProperties))
    FWorldStatRequirements Requirements;

    // ==================== REQUIREMENT QUERIES ====================

    UFUNCTION(BlueprintPure, Category = "Skill|Requirements")
    bool MeetsRequirements(const UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Skill|Requirements")
    int32 GetTotalDeficit(const UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Skill|Requirements")
    float CalculateRequirementPenalty(const UCharacterData *Character) const;

    // ==================== DISPLAY ====================

    UFUNCTION(BlueprintPure, Category = "Skill|Display")
    FString GetTierString() const;

    // ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};
