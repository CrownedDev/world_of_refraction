// FSkillEffect.h
// Struct defining a single effect applied by abilities, spells, and weapon attacks
// Reuses existing enums: ESkillEffectType, ETargetType, ESkillTrigger

#pragma once

#include "CoreMinimal.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Combat/TargetType.h"
#include "Skills/Effects/ESkillTrigger.h"
#include "Skills/Effects/SkillTriggerUtils.h"
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

    /** Optional display name for passive-style effects shown in UI. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    FString EffectName = TEXT("");

    /** When does this effect trigger (source-side condition). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
    ESkillTrigger Condition = ESkillTrigger::Always;

    /** Auto-set by PostSerialize — true when Condition uses a threshold value.
     *  Drives EditCondition gating for ConditionThreshold. Do not edit manually. */
    UPROPERTY()
    bool bConditionUsesThreshold = false;

    /** HP or Energy % threshold for the source trigger (0..100). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger",
              meta = (EditCondition = "bConditionUsesThreshold",
                      EditConditionHides, ClampMin = "0.0", ClampMax = "100.0"))
    float ConditionThreshold = 30.0f;

    /** Secondary source-side condition, combined with Condition via AND/OR. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger",
              meta = (EditCondition = "Condition != ESkillTrigger::None && Condition != ESkillTrigger::Always",
                      EditConditionHides))
    ESkillTrigger SecondaryCondition = ESkillTrigger::None;

    /** Auto-set by PostSerialize — true when SecondaryCondition uses a threshold. */
    UPROPERTY()
    bool bSecondaryConditionUsesThreshold = false;

    /** HP or Energy % threshold for the secondary source trigger (0..100). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger",
              meta = (EditCondition = "bSecondaryConditionUsesThreshold",
                      EditConditionHides, ClampMin = "0.0", ClampMax = "100.0"))
    float SecondaryThreshold = 30.0f;

    /** AND = both source conditions must hold. OR = either is enough. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger",
              meta = (EditCondition = "SecondaryCondition != ESkillTrigger::None",
                      EditConditionHides))
    bool bRequireBothConditions = true;

    /** Condition that must hold on the TARGET for the effect to apply. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
    ESkillTrigger TargetCondition = ESkillTrigger::None;

    /** Auto-set by PostSerialize — true when TargetCondition uses a threshold. */
    UPROPERTY()
    bool bTargetConditionUsesThreshold = false;

    /** HP or Energy % threshold for the target condition (0..100). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger",
              meta = (EditCondition = "bTargetConditionUsesThreshold",
                      EditConditionHides, ClampMin = "0.0", ClampMax = "100.0"))
    float TargetThreshold = 100.0f;

    // ==================== DRAIN ====================

    /**
     * For drain effects (HealthRestore/EnergyRestore with OnHit):
     * Percentage of damage dealt that converts to restore (0.3 = 30%)
     * Only used when Condition is OnHit and EffectType is a restore type
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drain",
              meta = (ClampMin = "0.0", ClampMax = "1.0",
                      EditCondition = "Condition == ESkillTrigger::OnHit"))
    float DrainPercent = 0.0f;

    // ==================== CONSTRUCTORS ====================

    FSkillEffect()
    {
    }

    FSkillEffect(ESkillEffectType InType, float InMagnitude, int32 InValue, int32 InDuration,
                 ETargetType InTarget, ESkillTrigger InCondition)
        : EffectType(InType), Magnitude(InMagnitude), Value(InValue), Duration(InDuration), Target(InTarget), Condition(InCondition), DrainPercent(0.0f)
    {
    }

    // ==================== HELPERS ====================

    /** Is this effect valid (has a type set)? */
    bool IsValid() const
    {
        return EffectType != ESkillEffectType::None;
    }

    /** Is this a buff effect? Delegates to the shared single-source classifier. */
    bool IsBuff() const
    {
        return SkillEffectClassification::IsBuff(EffectType, Magnitude);
    }

    /** Is this a debuff effect? Delegates to the shared single-source classifier. */
    bool IsDebuff() const
    {
        return SkillEffectClassification::IsDebuff(EffectType, Magnitude);
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
        return IsRestore() && DrainPercent > 0.0f && Condition == ESkillTrigger::OnHit;
    }

    /** Is this effect instant (Duration == 0)? */
    bool IsInstant() const
    {
        return Duration == 0;
    }

    /** Is this effect conditional (not Always, or has a secondary/target-side check)? */
    bool IsConditional() const
    {
        return Condition != ESkillTrigger::Always
            || SecondaryCondition != ESkillTrigger::None
            || TargetCondition != ESkillTrigger::None;
    }

    /** Does this effect carry a target-side condition? */
    bool HasTargetCondition() const
    {
        return TargetCondition != ESkillTrigger::None;
    }

    /** Does this effect carry a secondary source-side condition? */
    bool HasSecondaryCondition() const
    {
        return SecondaryCondition != ESkillTrigger::None;
    }

    /** Permanent / unconditional: no source-side or target-side trigger gating. */
    bool IsAlwaysActive() const
    {
        return Condition == ESkillTrigger::Always
            && TargetCondition == ESkillTrigger::None;
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

        // Optional named-passive prefix.
        if (!EffectName.IsEmpty())
        {
            Desc = EffectName + TEXT(": ");
        }

        // Effect type and magnitude
        const UEnum *StatusEnum = StaticEnum<ESkillEffectType>();
        FString TypeName = StatusEnum ? StatusEnum->GetDisplayNameTextByValue(static_cast<int64>(EffectType)).ToString() : TEXT("Unknown");

        if (IsDrain())
        {
            Desc += FString::Printf(TEXT("%.0f%% of damage as %s"),
                                    DrainPercent * 100.0f,
                                    *TypeName);
        }
        else if (Value != 0)
        {
            Desc += FString::Printf(TEXT("%s %d"), *TypeName, Value);
        }
        else if (Magnitude != 0.0f)
        {
            if (IsBuff() || IsDebuff())
            {
                Desc += FString::Printf(TEXT("%s %.0f%%"), *TypeName, Magnitude * 100.0f);
            }
            else
            {
                Desc += FString::Printf(TEXT("%s %.0f"), *TypeName, Magnitude);
            }
        }
        else
        {
            Desc += TypeName;
        }

        // Duration
        if (Duration > 0)
        {
            Desc += FString::Printf(TEXT(" for %d turn%s"),
                                    Duration,
                                    Duration > 1 ? TEXT("s") : TEXT(""));
        }

        // Target
        const UEnum *TargetEnum = StaticEnum<ETargetType>();
        FString TargetName = TargetEnum ? TargetEnum->GetDisplayNameTextByValue(static_cast<int64>(Target)).ToString() : TEXT("Unknown");
        Desc += FString::Printf(TEXT(" → %s"), *TargetName);

        // Condition (source-side)
        const UEnum *TriggerEnum = StaticEnum<ESkillTrigger>();
        if (Condition != ESkillTrigger::Always)
        {
            FString ConditionName = TriggerEnum ? TriggerEnum->GetDisplayNameTextByValue(static_cast<int64>(Condition)).ToString() : TEXT("Unknown");
            if (SkillTriggerUtils::IsThresholdTrigger(Condition))
            {
                Desc += FString::Printf(TEXT(" (%s %.0f%%)"), *ConditionName, ConditionThreshold);
            }
            else
            {
                Desc += FString::Printf(TEXT(" (%s)"), *ConditionName);
            }
        }

        // Secondary source-side condition, joined by AND / OR
        if (HasSecondaryCondition())
        {
            FString SecondaryName = TriggerEnum ? TriggerEnum->GetDisplayNameTextByValue(static_cast<int64>(SecondaryCondition)).ToString() : TEXT("Unknown");
            const TCHAR *Joiner = bRequireBothConditions ? TEXT("AND") : TEXT("OR");
            if (SkillTriggerUtils::IsThresholdTrigger(SecondaryCondition))
            {
                Desc += FString::Printf(TEXT(" %s (%s %.0f%%)"), Joiner, *SecondaryName, SecondaryThreshold);
            }
            else
            {
                Desc += FString::Printf(TEXT(" %s (%s)"), Joiner, *SecondaryName);
            }
        }

        // Target-side condition
        if (HasTargetCondition())
        {
            FString TargetCondName = TriggerEnum ? TriggerEnum->GetDisplayNameTextByValue(static_cast<int64>(TargetCondition)).ToString() : TEXT("Unknown");
            if (SkillTriggerUtils::IsThresholdTrigger(TargetCondition))
            {
                Desc += FString::Printf(TEXT(" [target: %s %.0f%%]"), *TargetCondName, TargetThreshold);
            }
            else
            {
                Desc += FString::Printf(TEXT(" [target: %s]"), *TargetCondName);
            }
        }

        return Desc;
    }

    // ==================== SERIALIZATION ====================

    /** Syncs the b*UsesThreshold flags with the current Condition /
     *  SecondaryCondition / TargetCondition on every save AND load. The owning
     *  UObject's PostEditChangeChainProperty runs the same sync on each
     *  in-editor property change, so threshold-field gating stays live without
     *  needing a save/reload. See USkillDataBase / UEquipmentDataBase / UEvolutionItemData
     *  implementations. */
    void PostSerialize(const FArchive &Ar)
    {
        bConditionUsesThreshold = SkillTriggerUtils::IsThresholdTrigger(Condition);
        bSecondaryConditionUsesThreshold = SkillTriggerUtils::IsThresholdTrigger(SecondaryCondition);
        bTargetConditionUsesThreshold = SkillTriggerUtils::IsThresholdTrigger(TargetCondition);
    }
};

template <>
struct TStructOpsTypeTraits<FSkillEffect> : public TStructOpsTypeTraitsBase2<FSkillEffect>
{
    enum
    {
        WithPostSerialize = true,
    };
};
