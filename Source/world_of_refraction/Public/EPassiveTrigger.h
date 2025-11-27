// EPassiveTrigger.h
// When a passive effect activates

#pragma once

#include "CoreMinimal.h"
#include "EPassiveTrigger.generated.h"

UENUM(BlueprintType)
enum class EPassiveTrigger : uint8
{
    None UMETA(DisplayName = "None (Disabled)"),
    Always UMETA(DisplayName = "Always Active"),
    OnCrit UMETA(DisplayName = "On Critical Hit"),
    OnHit UMETA(DisplayName = "On Dealing Damage"),
    OnTakeDamage UMETA(DisplayName = "On Receiving Damage"),
    OnHPBelowThreshold UMETA(DisplayName = "When HP Below %"),
    OnHPAboveThreshold UMETA(DisplayName = "When HP Above %"),
    OnEnergyBelowThreshold UMETA(DisplayName = "When Energy Below %"),
    OnEnergyAboveThreshold UMETA(DisplayName = "When Energy Above %"),
    OnBattleStart UMETA(DisplayName = "On Battle Start"),
    OnTurnStart UMETA(DisplayName = "On Turn Start"),
    OnTurnEnd UMETA(DisplayName = "On Turn End"),
    OnKill UMETA(DisplayName = "On Kill"),
    OnStatusApplied UMETA(DisplayName = "On Applying Status"),
    OnStatusReceived UMETA(DisplayName = "On Receiving Status"),
    OnSpellCast UMETA(DisplayName = "After Casting Spell"),
    OnAbilityUsed UMETA(DisplayName = "After Using Ability"),
    OnDodge UMETA(DisplayName = "On Dodge"),
    OnBlock UMETA(DisplayName = "On Block")
};