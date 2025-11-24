// PassiveEffect.h
// Struct defining a single passive effect with dual trigger support

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("Passive effect description...");

    // ==================== PRIMARY TRIGGER ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Primary")
    EPassiveTrigger PrimaryTrigger = EPassiveTrigger::Always;

    // Threshold for HP/Energy triggers (percentage 0-100)
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

    // true = AND (both required), false = OR (either triggers)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Secondary",
              meta = (EditCondition = "SecondaryTrigger != EPassiveTrigger::None"))
    bool bRequireBothTriggers = true;

    // ==================== EFFECT ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
    EPassiveEffectType EffectType = EPassiveEffectType::ModifyDamageDealt;

    // Value: percentage for modifiers, flat for resources
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
    float EffectValue = 10.0f;

    // Duration: 0 = permanent/instant, >0 = turns
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0"))
    int32 Duration = 0;

    // ==================== UTILITY ====================

    bool HasSecondaryTrigger() const
    {
        return SecondaryTrigger != EPassiveTrigger::None;
    }

    bool IsAlwaysActive() const
    {
        return PrimaryTrigger == EPassiveTrigger::Always && !HasSecondaryTrigger();
    }

    FString GetTriggerDescription() const
    {
        if (!HasSecondaryTrigger())
        {
            return GetTriggerName(PrimaryTrigger);
        }

        FString Connector = bRequireBothTriggers ? TEXT(" AND ") : TEXT(" OR ");
        return GetTriggerName(PrimaryTrigger) + Connector + GetTriggerName(SecondaryTrigger);
    }

    static FString GetTriggerName(EPassiveTrigger Trigger)
    {
        switch (Trigger)
        {
        case EPassiveTrigger::None:
            return TEXT("None");
        case EPassiveTrigger::Always:
            return TEXT("Always");
        case EPassiveTrigger::OnCrit:
            return TEXT("On Crit");
        case EPassiveTrigger::OnHit:
            return TEXT("On Hit");
        case EPassiveTrigger::OnTakeDamage:
            return TEXT("On Take Damage");
        case EPassiveTrigger::OnHPBelowThreshold:
            return TEXT("HP Below Threshold");
        case EPassiveTrigger::OnHPAboveThreshold:
            return TEXT("HP Above Threshold");
        case EPassiveTrigger::OnEnergyBelowThreshold:
            return TEXT("Energy Below Threshold");
        case EPassiveTrigger::OnEnergyAboveThreshold:
            return TEXT("Energy Above Threshold");
        case EPassiveTrigger::OnBattleStart:
            return TEXT("Battle Start");
        case EPassiveTrigger::OnTurnStart:
            return TEXT("Turn Start");
        case EPassiveTrigger::OnTurnEnd:
            return TEXT("Turn End");
        case EPassiveTrigger::OnKill:
            return TEXT("On Kill");
        case EPassiveTrigger::OnStatusApplied:
            return TEXT("On Status Applied");
        case EPassiveTrigger::OnStatusReceived:
            return TEXT("On Status Received");
        case EPassiveTrigger::OnUltimateUsed:
            return TEXT("After Ultimate");
        case EPassiveTrigger::OnSpellCast:
            return TEXT("After Spell");
        case EPassiveTrigger::OnAbilityUsed:
            return TEXT("After Ability");
        case EPassiveTrigger::OnDodge:
            return TEXT("On Dodge");
        case EPassiveTrigger::OnBlock:
            return TEXT("On Block");
        default:
            return TEXT("Unknown");
        }
    }
};