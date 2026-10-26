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
    OnBlock UMETA(DisplayName = "On Block (legacy: On Guard)"),
    // Defense-outcome triggers (append-only — values serialized into .uassets). Fired at
    // impact (ResolveImpactDefense → OnDefenseResolved); a perfect outcome fires both its
    // perfect trigger and the base (see SkillTriggerUtils::DefenseOutcomeToTriggers).
    // Retired by Guard (D-084 / D-087, B-168): still fire, mapped onto Perfect Guard. Author On Guard /
    // On Perfect Guard instead.
    OnParry UMETA(DisplayName = "On Parry (legacy: On Perfect Guard)"),
    OnPerfectParry UMETA(DisplayName = "On Perfect Parry (legacy: On Perfect Guard)"),
    OnPerfectBlock UMETA(DisplayName = "On Perfect Block (legacy: On Perfect Guard)"),
    OnPerfectDodge UMETA(DisplayName = "On Perfect Dodge"),
    // Guard (D-084): On Guard fires on every successful Guard, On Perfect Guard also on a Perfect one.
    OnGuard UMETA(DisplayName = "On Guard"),
    OnPerfectGuard UMETA(DisplayName = "On Perfect Guard")
};
