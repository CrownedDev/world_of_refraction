// ESkillTrigger.h
// Trigger condition that activates a skill / passive effect.

#pragma once

#include "CoreMinimal.h"
#include "ESkillTrigger.generated.h"

UENUM(BlueprintType)
enum class ESkillTrigger : uint8
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
    OnBlock UMETA(DisplayName = "On Block"),
    // Defense-outcome triggers (append-only — values serialized into .uassets). Fired at
    // impact (ResolveImpactDefense → OnDefenseResolved); a perfect outcome fires both its
    // perfect trigger and the base (see SkillTriggerUtils::DefenseOutcomeToTriggers).
    OnParry UMETA(DisplayName = "On Parry"),
    OnPerfectParry UMETA(DisplayName = "On Perfect Parry"),
    OnPerfectBlock UMETA(DisplayName = "On Perfect Block"),
    OnPerfectDodge UMETA(DisplayName = "On Perfect Dodge")
};
