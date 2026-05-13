// FSkillEffect.h
// Struct defining a single effect applied by abilities, spells, and weapon attacks
// Reuses existing enums: ESkillEffectType, ETargetType, EPassiveTrigger

#pragma once

#include "CoreMinimal.h"
#include "ESkillEffectType.h"
#include "TargetType.h"
#include "EPassiveTrigger.h"
#include "FSkillEffect.generated.h"

/**
 * FSkillEffect
 * Defines a single effect that an ability, spell, or weapon attack can apply.
 * Skills can have multiple effects (max 5).
 *
 * Examples:
 * - Power Strike: RawDamageBuff to Self OnHit
 * - Drain Strike: EnergyRestore to Self OnHit with DrainPercent
 * - War Cry: RawDamageBuff to AllAllies Always
 * - Intimidate: RawDamageDebuff to SingleEnemy Always
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSkillEffect
{
    GENERATED_BODY()

    // ==================== EFFECT TYPE ====================

    /** What effect to apply (buff, debuff, restore, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    ESkillEffectType EffectType = ESkillEffectType::None;

    // ==================== MAGNITUDE ====================

    /**
     * Effect strength:
     * - For buffs/debuffs: Percentage as decimal (0.2 = 20%)
     * - For restore/drain: Flat value OR use DrainPercent
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect",
              meta = (ClampMin = "0.0"))
    float Magnitude = 0.0f;

    /**
     * Flat value, used for absolute amounts (e.g. 30 = 30 HP per turn for DOT).
     * Distinct from Magnitude, which is decimal percent (0.2 = 20%).
     * Authors: use Value for flat amounts (DOT damage, heal HP), use Magnitude
     * for percentages (buff/debuff %). Don't set both on the same effect.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect", meta = (ClampMin = "0"))
    int32 Value = 0;

    /** Duration in turns (0 = instant effect like heal/damage) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect",
              meta = (ClampMin = "0"))
    int32 Duration = 0;

    // ==================== TARGETING ====================

    /** Who receives this effect */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    ETargetType Target = ETargetType::SingleEnemy;

    // ==================== CONDITION ====================

    /** When does this effect trigger */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
    EPassiveTrigger Condition = EPassiveTrigger::Always;

    // ==================== DRAIN ====================

    /**
     * For drain effects (HealthRestore/EnergyRestore with OnHit):
     * Percentage of damage dealt that converts to restore (0.3 = 30%)
     * Only used when Condition is OnHit and EffectType is a restore type
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drain",
              meta = (ClampMin = "0.0", ClampMax = "1.0",
                      EditCondition = "Condition == EPassiveTrigger::OnHit"))
    float DrainPercent = 0.0f;

    // ==================== CONSTRUCTORS ====================

    FSkillEffect()
    {
    }

    FSkillEffect(ESkillEffectType InType, float InMagnitude, int32 InValue, int32 InDuration,
                 ETargetType InTarget, EPassiveTrigger InCondition)
        : EffectType(InType)
        , Magnitude(InMagnitude)
        , Value(InValue)
        , Duration(InDuration)
        , Target(InTarget)
        , Condition(InCondition)
        , DrainPercent(0.0f)
    {
    }

    // ==================== HELPERS ====================

    /** Is this effect valid (has a type set)? */
    bool IsValid() const
    {
        return EffectType != ESkillEffectType::None;
    }

    /** Is this a buff effect? */
    bool IsBuff() const
    {
        switch (EffectType)
        {
        case ESkillEffectType::MindBuff:
        case ESkillEffectType::BodyBuff:
        case ESkillEffectType::SpiritBuff:
        case ESkillEffectType::SpellCostBuff:
        case ESkillEffectType::EffectDamageBuff:
        case ESkillEffectType::CritChanceBuff:
        case ESkillEffectType::DefenseBuff:
        case ESkillEffectType::AttackSpeedBuff:
        case ESkillEffectType::RawDamageBuff:
        case ESkillEffectType::MaxEnergyBuff:
        case ESkillEffectType::ResistanceBuff:
        case ESkillEffectType::SpellSizeBuff:
        case ESkillEffectType::DamageBuff:
        case ESkillEffectType::SpeedBuff:
            return true;
        default:
            return false;
        }
    }

    /** Is this a debuff effect? */
    bool IsDebuff() const
    {
        switch (EffectType)
        {
        case ESkillEffectType::MindDebuff:
        case ESkillEffectType::BodyDebuff:
        case ESkillEffectType::SpiritDebuff:
        case ESkillEffectType::SpellCostDebuff:
        case ESkillEffectType::EffectDamageDebuff:
        case ESkillEffectType::CritChanceDebuff:
        case ESkillEffectType::DefenseDebuff:
        case ESkillEffectType::AttackSpeedDebuff:
        case ESkillEffectType::RawDamageDebuff:
        case ESkillEffectType::MaxEnergyDebuff:
        case ESkillEffectType::ResistanceDebuff:
        case ESkillEffectType::SpellSizeDebuff:
        case ESkillEffectType::DamageDebuff:
        case ESkillEffectType::SpeedDebuff:
            return true;
        default:
            return false;
        }
    }

    /** Is this a restore effect? */
    bool IsRestore() const
    {
        return EffectType == ESkillEffectType::HealthRestore ||
               EffectType == ESkillEffectType::EnergyRestore;
    }

    /** Is this a drain effect (restore that scales with damage dealt)? */
    bool IsDrain() const
    {
        return IsRestore() && DrainPercent > 0.0f && Condition == EPassiveTrigger::OnHit;
    }

    /** Is this effect instant (Duration == 0)? */
    bool IsInstant() const
    {
        return Duration == 0;
    }

    /** Is this effect conditional (not Always)? */
    bool IsConditional() const
    {
        return Condition != EPassiveTrigger::Always;
    }

    /** Does this effect target self? */
    bool TargetsSelf() const
    {
        return Target == ETargetType::Self;
    }

    /** Does this effect target allies? */
    bool TargetsAllies() const
    {
        return Target == ETargetType::SingleAlly ||
               Target == ETargetType::AllAllies;
    }

    /** Does this effect target enemies? */
    bool TargetsEnemies() const
    {
        return Target == ETargetType::SingleEnemy ||
               Target == ETargetType::AllEnemies;
    }

    // ==================== DEBUG ====================

    /** Get a description string for debug/UI */
    FString GetDescription() const
    {
        if (!IsValid())
        {
            return TEXT("No Effect");
        }

        FString Desc;

        // Effect type and magnitude
        const UEnum* StatusEnum = StaticEnum<ESkillEffectType>();
        FString TypeName = StatusEnum ? StatusEnum->GetDisplayNameTextByValue(static_cast<int64>(EffectType)).ToString() : TEXT("Unknown");

        if (IsDrain())
        {
            Desc = FString::Printf(TEXT("%.0f%% of damage as %s"),
                                   DrainPercent * 100.0f,
                                   *TypeName);
        }
        else if (Value != 0)
        {
            // Flat amount (DOT damage, heal HP, etc.)
            Desc = FString::Printf(TEXT("%s %d"),
                                   *TypeName,
                                   Value);
        }
        else if (Magnitude != 0.0f)
        {
            if (IsBuff() || IsDebuff())
            {
                Desc = FString::Printf(TEXT("%s %.0f%%"),
                                       *TypeName,
                                       Magnitude * 100.0f);
            }
            else
            {
                Desc = FString::Printf(TEXT("%s %.0f"),
                                       *TypeName,
                                       Magnitude);
            }
        }
        else
        {
            Desc = TypeName;
        }

        // Duration
        if (Duration > 0)
        {
            Desc += FString::Printf(TEXT(" for %d turn%s"),
                                    Duration,
                                    Duration > 1 ? TEXT("s") : TEXT(""));
        }

        // Target
        const UEnum* TargetEnum = StaticEnum<ETargetType>();
        FString TargetName = TargetEnum ? TargetEnum->GetDisplayNameTextByValue(static_cast<int64>(Target)).ToString() : TEXT("Unknown");
        Desc += FString::Printf(TEXT(" → %s"), *TargetName);

        // Condition
        if (IsConditional())
        {
            const UEnum* TriggerEnum = StaticEnum<EPassiveTrigger>();
            FString ConditionName = TriggerEnum ? TriggerEnum->GetDisplayNameTextByValue(static_cast<int64>(Condition)).ToString() : TEXT("Unknown");
            Desc += FString::Printf(TEXT(" (%s)"), *ConditionName);
        }

        return Desc;
    }
};
