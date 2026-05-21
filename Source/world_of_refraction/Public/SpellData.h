// SpellData.h
// Spell Data Asset - Element-locked magical abilities.

#pragma once

#include "CoreMinimal.h"
#include "CastableSkillDataBase.h"
#include "ESpellElement.h"
#include "ESkillEffectType.h"
#include "SpellSchool.h"
#include "WeaponData.h"
#include "CombatConstants.h"
#include "NiagaraSystem.h"
#include "EDefenseType.h"
#include "ActionStatModifiers.h"

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
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ESpellElement Element = ESpellElement::Fire;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ESpellSchool School = ESpellSchool::Destruction;

    /** Required evolution crystal to use this spell (nullptr = no requirement, only for evolution spells) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
    UEvolutionItemData *RequiredEvolutionCrystal = nullptr;

    // ==================== STATS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    int32 TurnCost = 1; // Future: multi-turn casting

    // ==================== VISUALS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UAnimMontage *CastAnimation = nullptr;

    /** Main spell VFX (projectile traveling, AOE expanding, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UNiagaraSystem *SpellVFX = nullptr;

    /** Impact/explosion VFX (optional - plays on hit) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UNiagaraSystem *ImpactVFX = nullptr;

    /** Muzzle/cast flash VFX (optional - plays at caster on cast) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UNiagaraSystem *MuzzleVFX = nullptr;

    // ==================== DELIVERY (spell-specific extensions) ====================

    /** Homing tracking strength: 0 = no tracking, 1 = instant turn (Homing only) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery",
              meta = (EditCondition = "DeliveryType == ESpellDeliveryType::Homing",
                      EditConditionHides, ClampMin = "0.0", ClampMax = "1.0"))
    float HomingStrength = 0.5f;

    /** Beam duration in seconds (Beam only) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery",
              meta = (EditCondition = "DeliveryType == ESpellDeliveryType::Beam",
                      EditConditionHides, ClampMin = "0.1"))
    float BeamDuration = 1.0f;

    // ==================== SIZE ====================

    /** Base VFX scale - set to match Niagara system's intended size */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size",
              meta = (ClampMin = "0.1"))
    float BaseSize = 1.0f;

    /** Hitbox as percentage of visual (0.8 = hitbox is 80% of visual, more forgiving) */
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

    // ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};
