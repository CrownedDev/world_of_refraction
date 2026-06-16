// SpellData.h
// Spell Data Asset - Element-locked magical abilities.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/CastableSkillDataBase.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Definitions/SpellSchool.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Combat/CombatConstants.h"
#include "NiagaraSystem.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Actions/ActionStatModifiers.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "SpellData.generated.h"

// Forward declaration
class UCharacterData;
class UEvolutionItemData;

/**
 * Spell Data Asset - Element-locked magical abilities
 * Supports mode toggle system (Elemental vs Raw/Construct).
 *
 * Inherits shared skill shape from USkillDataBase and cast shape
 * (Tier, BaseDamage, BaseEnergyCost, Requirements, DeliveryType, ProjectileSpeed)
 * from UCastableSkillDataBase.
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API USpellData : public UCastableSkillDataBase
{
    GENERATED_BODY()

public:
    /** Spells default to the Ranged descriptive tag (D3) — the field is never
     *  serialized on existing spell assets, so the ctor default applies to all. */
    USpellData() { ExecutionType = EAbilityExecutionType::Ranged; }

    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ESpellElement Element = ESpellElement::Fire;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ESpellSchool School = ESpellSchool::Destruction;

    /** Required evolution crystal to use this spell (nullptr = no requirement, only for evolution spells) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
    UEvolutionItemData *RequiredEvolutionCrystal = nullptr;

    // ==================== VISUALS ====================

    /** DEPRECATED (D2, meta'd Stage 12 SC7): load-only — readers use
     *  SkillMontage; only the PostLoad mirror still reads this. Hard-delete
     *  at the post-SC8 resave bake. */
    UPROPERTY(BlueprintReadOnly, Category = "Visuals", meta = (DeprecatedProperty))
    UAnimMontage *CastAnimation = nullptr;

    /** DEPRECATED (D6, meta'd Stage 12 SC7): load-only — became the Cast
     *  entry's Trail; readers use the entry (empty-CastArray fallback + the
     *  PostLoad migration still read this). Hard-delete post-SC8. */
    UPROPERTY(BlueprintReadOnly, Category = "Visuals", meta = (DeprecatedProperty))
    UNiagaraSystem *SpellVFX = nullptr;

    /** DEPRECATED (D5, meta'd Stage 12 SC7): load-only — readers use the
     *  VFXArray Impact-role entry (per-role fallback + PostLoad migration
     *  still read this). Hard-delete post-SC8. */
    UPROPERTY(BlueprintReadOnly, Category = "Visuals", meta = (DeprecatedProperty))
    UNiagaraSystem *ImpactVFX = nullptr;

    /** DEPRECATED (D5, meta'd Stage 12 SC7): load-only — readers use the
     *  VFXArray Muzzle-role entry (per-role fallback + PostLoad migration
     *  still read this). Hard-delete post-SC8. */
    UPROPERTY(BlueprintReadOnly, Category = "Visuals", meta = (DeprecatedProperty))
    UNiagaraSystem *MuzzleVFX = nullptr;

    // ==================== SIZE ====================

    /** DEPRECATED (D6 Stage 12 SC6): load-only — migrated to CastArray
     *  (VisualScale = BaseSize, Size = BaseSize × HitboxRatio) and all
     *  production readers re-pointed; only the PostLoad migration and the
     *  empty-CastArray loose fallback still read it. Hard-delete (with the
     *  migration block) after the post-SC8 spell-asset resave bake. */
    UPROPERTY(BlueprintReadOnly, Category = "Size", meta = (DeprecatedProperty))
    float BaseSize = 1.0f;

    /** DEPRECATED (D6 Stage 12 SC6): load-only — folded into the CastArray
     *  entry's Size (= BaseSize × HitboxRatio) and all production readers
     *  re-pointed; only the PostLoad migration and the empty-CastArray loose
     *  fallback still read it. Hard-delete (with the migration block) after
     *  the post-SC8 spell-asset resave bake. */
    UPROPERTY(BlueprintReadOnly, Category = "Size", meta = (DeprecatedProperty))
    float HitboxRatio = 0.8f;

    // ==================== DEFENSE HELPERS ====================

    UFUNCTION(BlueprintPure, Category = "Delivery|Defense")
    bool CanBeBlocked() const;

    UFUNCTION(BlueprintPure, Category = "Delivery|Defense")
    bool CanBeParried() const;

    UFUNCTION(BlueprintPure, Category = "Delivery|Defense")
    bool CanBeDodgedByMoving() const;

    UFUNCTION(BlueprintPure, Category = "Delivery|Defense")
    bool CanBeDodgedByTiming() const;

    UFUNCTION(BlueprintPure, Category = "Delivery|Defense")
    TArray<EDefenseType> GetAvailableDefenses() const;

    // ==================== DAMAGE CALCULATIONS ====================

    /** Calculate spell damage. Per-action stat modifiers (Reality, Evolution,
     *  future buffs) come in via ActionMods — populated by ActionExecutor at
     *  spell execution time. AI preview / non-execution callers can omit the
     *  parameter to use the default (no boost), preserving prior behaviour.
     *
     *  BaseDamageOverride (Stage 6 cluster 5): when >= 0, this base is used in place of the
     *  skill-level BaseDamage — the per-cast-entry SPELL damage. -1 (default) = use BaseDamage
     *  as before. Only the raw BASE is swapped; the bIsRawMode mult + requirement penalty still
     *  apply on it, and the SpellDamage/Mind/element scaling runs downstream at ApplyHit. */
    int32 CalculateDamage(UCharacterData *Character, const FActionStatModifiers &ActionMods = FActionStatModifiers(), int32 BaseDamageOverride = -1) const;

    // ==================== ENERGY CALCULATIONS ====================

    UFUNCTION(BlueprintPure, Category = "Spell|Energy")
    int32 CalculateEnergyCost(UCharacterData *Character) const;

    // ==================== STATUS BUILDUP ====================

    /** Calculate spell status buildup per-hit. Per-action stat modifiers come
     *  in via ActionMods. */
    int32 CalculateStatusBuildup(UCharacterData *Character, const FActionStatModifiers &ActionMods = FActionStatModifiers()) const;

    // ==================== HELPER FUNCTIONS ====================

    UFUNCTION(BlueprintPure, Category = "Spell|Helpers")
    bool CanCharacterCast(UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Spell|Display")
    FString GetDisplayName(UCharacterData *Caster) const;

    UFUNCTION(BlueprintPure, Category = "Spell")
    FString GetElementName() const
    {
        const UEnum *EnumPtr = StaticEnum<ESpellElement>();
        if (EnumPtr)
        {
            FString ElementName = EnumPtr->GetNameStringByValue(static_cast<int64>(Element));
            ElementName.RemoveFromStart(TEXT("ESpellElement::"));
            return ElementName;
        }
        return TEXT("Unknown");
    }

    // ==================== CONSTRUCT ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Construct")
    bool bIsConstruct = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Construct", meta = (EditCondition = "bIsConstruct"))
    UWeaponData *ConstructedWeapon = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Construct", meta = (EditCondition = "bIsConstruct"))
    bool bSealsSpells = true;

    // ==================== MIGRATION ====================

    // Outside WITH_EDITOR — the D2 montage migration must run in all builds.
    virtual void PostLoad() override;

    // ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};
