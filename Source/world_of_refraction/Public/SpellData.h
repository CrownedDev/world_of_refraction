// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ESpellElement.h"
#include "EStatusType.h"
#include "SpellSchool.h"
#include "TargetType.h"
#include "ItemTier.h"
#include "AbilityData.h"
#include "WeaponData.h"
#include "WorldStatRequirements.h"
#include "CombatConstants.h"
#include "NiagaraSystem.h"
#include "ESpellDeliveryType.h"
#include "EDefenseType.h"
#include "ActionStatModifiers.h"
#include "FSkillEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "SpellData.generated.h"

// Forward declaration
class UCharacterData;
class UItemData;

/**
 * Spell Data Asset - Element-locked magical abilities
 * Supports mode toggle system (Elemental vs Raw/Construct)
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API USpellData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString SpellName = TEXT("Unnamed Spell");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ESpellElement Element = ESpellElement::Fire;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ESpellSchool School = ESpellSchool::Destruction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("Spell description...");

    //  Tier for break calculations
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EItemTier Tier = EItemTier::E_Tier;

    /** Required evolution crystal to use this spell (nullptr = no requirement, only for evolution spells) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
    UItemData *RequiredEvolutionCrystal = nullptr;
    // ==================== MODE ====================

    /** Raw mode: +10% damage, no status buildup. Elemental mode (default): applies status. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode")
    bool bIsRawMode = false;

    // ==================== STATS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    int32 Damage = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    int32 EnergyCost = 0;

    /** Status buildup per hit (Elemental mode only) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats",
              meta = (EditCondition = "!bIsRawMode", EditConditionHides))
    int32 StatusBuildup = 15;

    // ==================== INFUSION ====================

    /** If false, the orchestrator rejects this spell when an infusion source is selected. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
    bool bCanBeInfused = true;

    // ==================== COMMON STATS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    int32 HitCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    ETargetType TargetType = ETargetType::SingleEnemy;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    int32 TurnCost = 1; // Future: multi-turn casting

    // ==================== REQUIREMENTS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
    FWorldStatRequirements Requirements;

    // ==================== EFFECTS ====================

    /**
     * Effects applied by this spell (max 5).
     * Each effect can have its own target, condition, and timing.
     * Replaces the previous PrimaryEffect/SecondaryEffect flat fields —
     * spells now use the same array shape as abilities and attacks.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects",
              meta = (TitleProperty = "EffectType"))
    TArray<FSkillEffect> Effects;

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

    // ==================== DELIVERY ====================

    /** How the spell travels from caster to target */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery")
    ESpellDeliveryType DeliveryType = ESpellDeliveryType::Projectile;

    /** Travel speed in units per second (Projectile/Homing only) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery",
              meta = (EditCondition = "DeliveryType == ESpellDeliveryType::Projectile || DeliveryType == ESpellDeliveryType::Homing",
                      EditConditionHides, ClampMin = "100.0"))
    float ProjectileSpeed = 1500.f;

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

    // ==================== REQUIREMENT CHECKS ====================

    UFUNCTION(BlueprintPure, Category = "Spell")
    bool MeetsRequirements(const UCharacterData *Character) const
    {
        return Requirements.MeetsRequirements(Character);
    }

    UFUNCTION(BlueprintPure, Category = "Spell")
    int32 GetTotalDeficit(const UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Spell")
    float CalculateRequirementPenalty(const UCharacterData *Character) const;

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
     *  in via ActionMods. Pre-existing: DamageCalculator::CalculateSpellDamage
     *  calls this without modifiers — that path doesn't get per-action buffs.
     *  Documented, not changed by this commit. */
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
            FString Name = EnumPtr->GetNameStringByValue(static_cast<int64>(Element));
            Name.RemoveFromStart(TEXT("ESpellElement::"));
            return Name;
        }
        return TEXT("Unknown");
    }

    UFUNCTION(BlueprintPure, Category = "Spell")
    FString GetTierString() const { return TierHelpers::GetTierDisplayString(Tier); }

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