// FAbilityEffect.h
// Struct defining a single effect applied by an ability
// Reuses existing enums: EStatusType, ETargetType, EPassiveTrigger

#pragma once

#include "CoreMinimal.h"
#include "EStatusType.h"
#include "TargetType.h"
#include "EPassiveTrigger.h"
#include "FAbilityEffect.generated.h"

/**
 * FAbilityEffect
 * Defines a single effect that an ability can apply.
 * Abilities can have multiple effects (max 5).
 *
 * Examples:
 * - Power Strike: RawDamageBuff to Self OnHit
 * - Drain Strike: EnergyRestore to Self OnHit with DrainPercent
 * - War Cry: RawDamageBuff to AllAllies Always
 * - Intimidate: RawDamageDebuff to SingleEnemy Always
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FAbilityEffect
{
    GENERATED_BODY()

    // ==================== EFFECT TYPE ====================

    /** What effect to apply (buff, debuff, restore, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    EStatusType EffectType = EStatusType::None;

    // ==================== MAGNITUDE ====================

    /** 
     * Effect strength:
     * - For buffs/debuffs: Percentage as decimal (0.2 = 20%)
     * - For restore/drain: Flat value OR use DrainPercent
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect",
              meta = (ClampMin = "0.0"))
    float Magnitude = 0.0f;

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

    FAbilityEffect()
    {
    }

    FAbilityEffect(EStatusType InType, float InMagnitude, int32 InDuration,
                   ETargetType InTarget, EPassiveTrigger InCondition)
        : EffectType(InType)
        , Magnitude(InMagnitude)
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
        return EffectType != EStatusType::None;
    }

    /** Is this a buff effect? */
    bool IsBuff() const
    {
        switch (EffectType)
        {
        case EStatusType::MindBuff:
        case EStatusType::BodyBuff:
        case EStatusType::SpiritBuff:
        case EStatusType::SpellCostBuff:
        case EStatusType::EffectDamageBuff:
        case EStatusType::CritChanceBuff:
        case EStatusType::DefenseBuff:
        case EStatusType::AttackSpeedBuff:
        case EStatusType::RawDamageBuff:
        case EStatusType::MaxEnergyBuff:
        case EStatusType::ResistanceBuff:
        case EStatusType::SpellSizeBuff:
        case EStatusType::DamageBuff:
        case EStatusType::SpeedBuff:
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
        case EStatusType::MindDebuff:
        case EStatusType::BodyDebuff:
        case EStatusType::SpiritDebuff:
        case EStatusType::SpellCostDebuff:
        case EStatusType::EffectDamageDebuff:
        case EStatusType::CritChanceDebuff:
        case EStatusType::DefenseDebuff:
        case EStatusType::AttackSpeedDebuff:
        case EStatusType::RawDamageDebuff:
        case EStatusType::MaxEnergyDebuff:
        case EStatusType::ResistanceDebuff:
        case EStatusType::SpellSizeDebuff:
        case EStatusType::DamageDebuff:
        case EStatusType::SpeedDebuff:
            return true;
        default:
            return false;
        }
    }

    /** Is this a restore effect? */
    bool IsRestore() const
    {
        return EffectType == EStatusType::HealthRestore ||
               EffectType == EStatusType::EnergyRestore;
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
        const UEnum* StatusEnum = StaticEnum<EStatusType>();
        FString TypeName = StatusEnum ? StatusEnum->GetDisplayNameTextByValue(static_cast<int64>(EffectType)).ToString() : TEXT("Unknown");

        if (IsDrain())
        {
            Desc = FString::Printf(TEXT("%.0f%% of damage as %s"),
                                   DrainPercent * 100.0f,
                                   *TypeName);
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
