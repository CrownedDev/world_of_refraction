// EPassiveEffectType.h
// What a passive effect does when triggered

#pragma once

#include "CoreMinimal.h"
#include "EPassiveEffectType.generated.h"

UENUM(BlueprintType)
enum class EPassiveEffectType : uint8
{
    None UMETA(DisplayName = "None"),

    // Stat Modifiers (percentage-based)
    ModifyDamageDealt UMETA(DisplayName = "Modify Damage Dealt %"),
    ModifyDamageTaken UMETA(DisplayName = "Modify Damage Taken %"),
    ModifyHealing UMETA(DisplayName = "Modify Healing %"),
    ModifyCritChance UMETA(DisplayName = "Modify Crit Chance %"),
    ModifyCritDamage UMETA(DisplayName = "Modify Crit Damage %"),
    ModifyEnergyCost UMETA(DisplayName = "Modify Energy Cost %"),
    ModifyTurnSpeed UMETA(DisplayName = "Modify Turn Speed %"),
    ModifyStatusResist UMETA(DisplayName = "Modify Status Resist %"),

    // Resource Changes (flat values)
    RestoreHP UMETA(DisplayName = "Restore HP (Flat)"),
    RestoreEnergy UMETA(DisplayName = "Restore Energy (Flat)"),
    RestoreHPPercent UMETA(DisplayName = "Restore HP (% of Max)"),
    RestoreEnergyPercent UMETA(DisplayName = "Restore Energy (% of Max)"),
    DrainHP UMETA(DisplayName = "Drain HP (Flat)"),
    DrainEnergy UMETA(DisplayName = "Drain Energy (Flat)"),

    // Defensive
    DamageReflect UMETA(DisplayName = "Reflect Damage %"),
    Lifesteal UMETA(DisplayName = "Lifesteal %"),
    Shield UMETA(DisplayName = "Grant Shield (Flat)"),

    // Immunity
    GrantBurnImmunity UMETA(DisplayName = "Immune to Burn"),
    GrantFreezeImmunity UMETA(DisplayName = "Immune to Freeze"),
    GrantStunImmunity UMETA(DisplayName = "Immune to Stun"),
    GrantSilenceImmunity UMETA(DisplayName = "Immune to Silence"),
    GrantAllStatusImmunity UMETA(DisplayName = "Immune to All Status"),

    // Status Application
    ApplyBurnToTarget UMETA(DisplayName = "Apply Burn to Target"),
    ApplyChillToTarget UMETA(DisplayName = "Apply Chill to Target"),
    ApplyStunToTarget UMETA(DisplayName = "Apply Stun to Target"),
    CleanseSelf UMETA(DisplayName = "Cleanse Self"),
    CleanseAllies UMETA(DisplayName = "Cleanse All Allies"),

    // Special Mechanics
    ExtraAction UMETA(DisplayName = "Grant Extra Action"),
    GuaranteedCrit UMETA(DisplayName = "Guarantee Critical Hit"),
    IgnoreDefense UMETA(DisplayName = "Ignore Defense"),
    DoubleHit UMETA(DisplayName = "Attack Hits Twice (50% each)"),
    Revive UMETA(DisplayName = "Revive on Death (Once)")
};