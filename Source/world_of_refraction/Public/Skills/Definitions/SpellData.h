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

    /** DEPRECATED (D2): authored value mirrors to SkillMontage via PostLoad;
     *  readers switch + DeprecatedProperty meta added at Stage 12. Still
     *  runtime-authoritative — keep authoring here until then. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UAnimMontage *CastAnimation = nullptr;

    /** Main spell VFX (projectile traveling, AOE expanding, etc.)
     *  DEPRECATED (D6): migrates to CastArray (the entry's Trail) via
     *  PostLoad; readers switch + DeprecatedProperty meta at Stage 12. Still
     *  runtime-authoritative — keep authoring here until then. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UNiagaraSystem *SpellVFX = nullptr;

    /** Impact/explosion VFX (optional - plays on hit)
     *  DEPRECATED (D5): mirrors to VFXArray (Role=Impact) via PostLoad;
     *  readers switch + DeprecatedProperty meta at Stage 12. Still
     *  runtime-authoritative — keep authoring here until then. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UNiagaraSystem *ImpactVFX = nullptr;

    /** Muzzle/cast flash VFX (optional - plays at caster on cast)
     *  DEPRECATED (D5): mirrors to VFXArray (Role=Muzzle) via PostLoad;
     *  readers switch + DeprecatedProperty meta at Stage 12. Still
     *  runtime-authoritative — keep authoring here until then. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UNiagaraSystem *MuzzleVFX = nullptr;

    // ==================== DELIVERY (spell-specific extensions) ====================

    /** Homing tracking strength: 0 = no tracking, 1 = instant turn (Homing only)
     *  DEPRECATED (D6): migrates to CastArray via PostLoad; readers switch +
     *  DeprecatedProperty meta at Stage 12. Still runtime-authoritative. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery",
              meta = (EditCondition = "DeliveryType == ESpellDeliveryType::Homing",
                      EditConditionHides, ClampMin = "0.0", ClampMax = "1.0"))
    float HomingStrength = 0.5f;

    /** Beam duration in seconds (Beam only)
     *  DEPRECATED (D6): migrates to CastArray via PostLoad; readers switch +
     *  DeprecatedProperty meta at Stage 12. Still runtime-authoritative. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery",
              meta = (EditCondition = "DeliveryType == ESpellDeliveryType::Beam",
                      EditConditionHides, ClampMin = "0.1"))
    float BeamDuration = 1.0f;

    /** Seconds between damage ticks while the beam is active (Beam only).
     *  DEPRECATED (D6): migrates to CastArray via PostLoad; readers switch +
     *  DeprecatedProperty meta at Stage 12. Still runtime-authoritative.
     *  TickCount = max(1, RoundToInt(BeamDuration / BeamTickInterval)).
     *  BaseDamage is the total dealt across all ticks; per-tick = BaseDamage /
     *  TickCount with the integer remainder distributed across the first
     *  `BaseDamage % TickCount` ticks so the total stays exact. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery",
              meta = (EditCondition = "DeliveryType == ESpellDeliveryType::Beam",
                      EditConditionHides, ClampMin = "0.01"))
    float BeamTickInterval = 0.5f;

    // ==================== SIZE ====================

    /** Base VFX scale - set to match Niagara system's intended size
     *  DEPRECATED (D6): migrates to CastArray via PostLoad (TrailScale =
     *  BaseSize, Size = BaseSize × HitboxRatio); readers switch (incl. the
     *  defense-window sizing) + DeprecatedProperty meta at Stage 12. Still
     *  runtime-authoritative. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size",
              meta = (ClampMin = "0.1"))
    float BaseSize = 1.0f;

    /** Hitbox as percentage of visual (0.8 = hitbox is 80% of visual, more forgiving)
     *  DEPRECATED (D6): folded into the CastArray entry's Size (= BaseSize ×
     *  HitboxRatio) via PostLoad; readers switch + DeprecatedProperty meta at
     *  Stage 12. Still runtime-authoritative. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size",
              meta = (ClampMin = "0.5", ClampMax = "1.2"))
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
     *  parameter to use the default (no boost), preserving prior behaviour. */
    int32 CalculateDamage(UCharacterData *Character, const FActionStatModifiers &ActionMods = FActionStatModifiers()) const;

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
