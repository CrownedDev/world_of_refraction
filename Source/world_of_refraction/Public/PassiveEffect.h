// PassiveEffect.h
// Struct defining a single passive effect with dual trigger support and auto-description

#pragma once

#include "CoreMinimal.h"
#include "EPassiveTrigger.h"
#include "EPassiveEffectType.h"
#include "PassiveEffect.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FPassiveEffect
{
    GENERATED_BODY()

    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString PassiveName = TEXT("Unnamed Passive");

    // Auto-generated description (read-only, updates on save)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString GeneratedDescription = TEXT("");

    // ==================== PRIMARY TRIGGER ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Primary")
    EPassiveTrigger PrimaryTrigger = EPassiveTrigger::Always;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Primary",
              meta = (ClampMin = "0", ClampMax = "100",
                      EditCondition = "PrimaryTrigger == EPassiveTrigger::OnHPBelowThreshold || PrimaryTrigger == EPassiveTrigger::OnHPAboveThreshold || PrimaryTrigger == EPassiveTrigger::OnEnergyBelowThreshold || PrimaryTrigger == EPassiveTrigger::OnEnergyAboveThreshold"))
    float PrimaryThreshold = 30.0f;

    // ==================== SECONDARY TRIGGER ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Secondary")
    EPassiveTrigger SecondaryTrigger = EPassiveTrigger::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Secondary",
              meta = (ClampMin = "0", ClampMax = "100",
                      EditCondition = "SecondaryTrigger == EPassiveTrigger::OnHPBelowThreshold || SecondaryTrigger == EPassiveTrigger::OnHPAboveThreshold || SecondaryTrigger == EPassiveTrigger::OnEnergyBelowThreshold || SecondaryTrigger == EPassiveTrigger::OnEnergyAboveThreshold"))
    float SecondaryThreshold = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Secondary",
              meta = (EditCondition = "SecondaryTrigger != EPassiveTrigger::None"))
    bool bRequireBothTriggers = true;

    // ==================== EFFECT ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
    EPassiveEffectType EffectType = EPassiveEffectType::ModifyDamageDealt;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
    float EffectValue = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0"))
    int32 Duration = 0;

    // ==================== HELPER FUNCTIONS ====================

    bool HasSecondaryTrigger() const
    {
        return SecondaryTrigger != EPassiveTrigger::None;
    }

    bool IsAlwaysActive() const
    {
        return PrimaryTrigger == EPassiveTrigger::Always && !HasSecondaryTrigger();
    }

    // Refresh the generated description (call from parent's PostEditChangeProperty)
    void RefreshDescription()
    {
        GeneratedDescription = BuildDescription();
    }

    // Get description (uses cached if available, otherwise builds)
    FString GetDescription() const
    {
        if (!GeneratedDescription.IsEmpty())
        {
            return GeneratedDescription;
        }
        return BuildDescription();
    }

    // ==================== DESCRIPTION BUILDING ====================

    FString BuildDescription() const
    {
        FString TriggerStr = GetTriggerDescription();
        FString EffectStr = GetEffectDescription();

        if (Duration > 0)
        {
            return FString::Printf(TEXT("%s: %s for %d turn%s"),
                                   *TriggerStr, *EffectStr, Duration, Duration > 1 ? TEXT("s") : TEXT(""));
        }
        return FString::Printf(TEXT("%s: %s"), *TriggerStr, *EffectStr);
    }

    FString GetTriggerDescription() const
    {
        FString PrimaryStr = GetTriggerName(PrimaryTrigger, PrimaryThreshold);

        if (!HasSecondaryTrigger())
        {
            return PrimaryStr;
        }

        FString SecondaryStr = GetTriggerName(SecondaryTrigger, SecondaryThreshold);
        FString Connector = bRequireBothTriggers ? TEXT(" AND ") : TEXT(" OR ");

        return PrimaryStr + Connector + SecondaryStr;
    }

    FString GetEffectDescription() const
    {
        FString ValueStr = (EffectValue >= 0)
                               ? FString::Printf(TEXT("+%.0f"), EffectValue)
                               : FString::Printf(TEXT("%.0f"), EffectValue);

        FString AbsValueStr = FString::Printf(TEXT("%.0f"), FMath::Abs(EffectValue));

        switch (EffectType)
        {
        // Stat Modifiers
        case EPassiveEffectType::ModifyDamageDealt:
            return FString::Printf(TEXT("%s%% Damage Dealt"), *ValueStr);
        case EPassiveEffectType::ModifyDamageTaken:
            return FString::Printf(TEXT("%s%% Damage Taken"), *ValueStr);
        case EPassiveEffectType::ModifyHealing:
            return FString::Printf(TEXT("%s%% Healing"), *ValueStr);
        case EPassiveEffectType::ModifyCritChance:
            return FString::Printf(TEXT("%s%% Crit Chance"), *ValueStr);
        case EPassiveEffectType::ModifyCritDamage:
            return FString::Printf(TEXT("%s%% Crit Damage"), *ValueStr);
        case EPassiveEffectType::ModifyEnergyCost:
            return FString::Printf(TEXT("%s%% Energy Cost"), *ValueStr);
        case EPassiveEffectType::ModifyTurnSpeed:
            return FString::Printf(TEXT("%s%% Turn Speed"), *ValueStr);
        case EPassiveEffectType::ModifyStatusResist:
            return FString::Printf(TEXT("%s%% Status Resist"), *ValueStr);

        // Resource Changes
        case EPassiveEffectType::RestoreHP:
            return FString::Printf(TEXT("Restore %s HP"), *AbsValueStr);
        case EPassiveEffectType::RestoreEnergy:
            return FString::Printf(TEXT("Restore %s Energy"), *AbsValueStr);
        case EPassiveEffectType::RestoreHPPercent:
            return FString::Printf(TEXT("Restore %s%% HP"), *AbsValueStr);
        case EPassiveEffectType::RestoreEnergyPercent:
            return FString::Printf(TEXT("Restore %s%% Energy"), *AbsValueStr);
        case EPassiveEffectType::DrainHP:
            return FString::Printf(TEXT("Lose %s HP"), *AbsValueStr);
        case EPassiveEffectType::DrainEnergy:
            return FString::Printf(TEXT("Lose %s Energy"), *AbsValueStr);

        // Defensive
        case EPassiveEffectType::DamageReflect:
            return FString::Printf(TEXT("Reflect %s%% Damage"), *AbsValueStr);
        case EPassiveEffectType::Lifesteal:
            return FString::Printf(TEXT("%s%% Lifesteal"), *AbsValueStr);
        case EPassiveEffectType::Shield:
            return FString::Printf(TEXT("Gain %s Shield"), *AbsValueStr);

        // Immunity
        case EPassiveEffectType::GrantBurnImmunity:
            return TEXT("Immune to Burn");
        case EPassiveEffectType::GrantFreezeImmunity:
            return TEXT("Immune to Freeze");
        case EPassiveEffectType::GrantStunImmunity:
            return TEXT("Immune to Stun");
        case EPassiveEffectType::GrantSilenceImmunity:
            return TEXT("Immune to Silence");
        case EPassiveEffectType::GrantAllStatusImmunity:
            return TEXT("Immune to All Status");

        // Status Application
        case EPassiveEffectType::ApplyBurnToTarget:
            return FString::Printf(TEXT("Apply %s Burn"), *AbsValueStr);
        case EPassiveEffectType::ApplyChillToTarget:
            return FString::Printf(TEXT("Apply %s Chill"), *AbsValueStr);
        case EPassiveEffectType::ApplyStunToTarget:
            return FString::Printf(TEXT("Apply %s Stun"), *AbsValueStr);
        case EPassiveEffectType::CleanseSelf:
            return TEXT("Cleanse Self");
        case EPassiveEffectType::CleanseAllies:
            return TEXT("Cleanse All Allies");

        // Special Mechanics
        case EPassiveEffectType::ExtraAction:
            return TEXT("Gain Extra Action");
        case EPassiveEffectType::GuaranteedCrit:
            return TEXT("Guaranteed Critical Hit");
        case EPassiveEffectType::IgnoreDefense:
            return TEXT("Ignore Defense");
        case EPassiveEffectType::DoubleHit:
            return TEXT("Attack Hits Twice (50% each)");
        case EPassiveEffectType::Revive:
            return FString::Printf(TEXT("Revive at %s%% HP"), *AbsValueStr);

        case EPassiveEffectType::None:
        default:
            return TEXT("No Effect");
        }
    }

    static FString GetTriggerName(EPassiveTrigger Trigger, float Threshold = 0.0f)
    {
        switch (Trigger)
        {
        case EPassiveTrigger::None:
            return TEXT("Never");
        case EPassiveTrigger::Always:
            return TEXT("Always Active");
        case EPassiveTrigger::OnCrit:
            return TEXT("On Critical Hit");
        case EPassiveTrigger::OnHit:
            return TEXT("On Dealing Damage");
        case EPassiveTrigger::OnTakeDamage:
            return TEXT("On Receiving Damage");
        case EPassiveTrigger::OnHPBelowThreshold:
            return FString::Printf(TEXT("When HP Below %.0f%%"), Threshold);
        case EPassiveTrigger::OnHPAboveThreshold:
            return FString::Printf(TEXT("When HP Above %.0f%%"), Threshold);
        case EPassiveTrigger::OnEnergyBelowThreshold:
            return FString::Printf(TEXT("When Energy Below %.0f%%"), Threshold);
        case EPassiveTrigger::OnEnergyAboveThreshold:
            return FString::Printf(TEXT("When Energy Above %.0f%%"), Threshold);
        case EPassiveTrigger::OnBattleStart:
            return TEXT("On Battle Start");
        case EPassiveTrigger::OnTurnStart:
            return TEXT("On Turn Start");
        case EPassiveTrigger::OnTurnEnd:
            return TEXT("On Turn End");
        case EPassiveTrigger::OnKill:
            return TEXT("On Kill");
        case EPassiveTrigger::OnStatusApplied:
            return TEXT("On Applying Status");
        case EPassiveTrigger::OnStatusReceived:
            return TEXT("On Receiving Status");
        case EPassiveTrigger::OnUltimateUsed:
            return TEXT("After Using Ultimate");
        case EPassiveTrigger::OnSpellCast:
            return TEXT("After Casting Spell");
        case EPassiveTrigger::OnAbilityUsed:
            return TEXT("After Using Ability");
        case EPassiveTrigger::OnDodge:
            return TEXT("On Dodge");
        case EPassiveTrigger::OnBlock:
            return TEXT("On Block");
        default:
            return TEXT("Unknown Trigger");
        }
    }
};