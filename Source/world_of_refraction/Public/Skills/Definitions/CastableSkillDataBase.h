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
#include "Skills/Definitions/SkillVFXEntry.h"
#include "Skills/Definitions/SkillCastEntry.h"
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

    /** DEPRECATED (D6, meta'd Stage 12 SC7): load-only — delivery is
     *  per-entry on CastArray; only the migration, the empty-CastArray
     *  fallback dispatch, and the async-decision fallback still read this.
     *  (The AbilityData/WeaponAttackData CanEditChange hiding overrides were
     *  removed — DeprecatedProperty hides it everywhere.) Hard-delete at the
     *  post-SC8 resave bake. */
    UPROPERTY(BlueprintReadOnly, Category = "Delivery", meta = (DeprecatedProperty))
    ESpellDeliveryType DeliveryType = ESpellDeliveryType::Projectile;

    /** DEPRECATED (D6, meta'd Stage 12 SC7): load-only — per-entry on
     *  CastArray; only the migration + empty-CastArray fallback still read
     *  this. Hard-delete at the post-SC8 resave bake. */
    UPROPERTY(BlueprintReadOnly, Category = "Delivery", meta = (DeprecatedProperty))
    float ProjectileSpeed = 1500.0f;

    /** Cast deliveries (D6). INDEX-ORDERED: a UCombatNotify (Family=Cast,
     *  Index=N) fires entry N — array position IS identity. Each entry is one
     *  self-contained delivery (fireball-then-pillar = two entries: Projectile
     *  + AOE). Consumed by the fused-montage runner (Stage 12); populated via
     *  PostLoad migration from the loose delivery fields. All three skill
     *  types inherit it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Delivery", meta = (TitleProperty = "Label"))
    TArray<FSkillCastEntry> CastArray;

    // ==================== ANIMATION ====================

    /** The unified skill montage (D2) — the field the fused-montage runner
     *  plays at Stage 12. Populated via PostLoad migration from the leaf
     *  montage fields (CastAnimation / ExecutionMontage / AttackMontage) until
     *  readers switch; the leaf fields stay runtime-authoritative until then. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage *SkillMontage = nullptr;

    /** Optional post-cast return montage — the runner plays it after
     *  SkillMontage IFF set (null = no return leg). Plays in-place this
     *  stage; warp-to-origin movement is the deferred movement arc. All
     *  three skill types inherit it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage *ReturnMontage = nullptr;

    /** Montage play-rate scalar (D7) — uniform across all three skill types;
     *  stat scaling layers on top. 1.0 = no change (the regression guard).
     *  Hoisted from WeaponAttackData; the runner plays SkillMontage at this
     *  rate (Stage 12). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float BaseAnimSpeed = 1.0f;

    // ==================== VISUALS ====================

    /** Role-classified VFX entries (D5). INDEX-ORDERED: a UCombatNotify
     *  (Family=VFX, Index=N) selects entry N — array position IS identity.
     *  Role drives code-spawned visuals (e.g. projectile impact fires
     *  Impact-role entries). Consumed by the fused-montage runner (Stage 12);
     *  populated via PostLoad migration from the loose spell VFX fields
     *  (Stage 10B). All three skill types inherit it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|VFX", meta = (TitleProperty = "Label"))
    TArray<FSkillVFXEntry> VFXArray;

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
