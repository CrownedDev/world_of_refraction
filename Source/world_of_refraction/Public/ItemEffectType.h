// ItemEffectType.h
// Defines the different types of effects items can have

#pragma once

#include "CoreMinimal.h"
#include "ItemEffectType.generated.h"

/**
 * Enum representing the different types of effects an item can produce
 * Used for categorization and effect application
 */
UENUM(BlueprintType)
enum class EItemEffectType : uint8
{
    Damage UMETA(DisplayName = "Damage (Direct HP damage)"),
    Healing UMETA(DisplayName = "Healing (HP restore)"),
    EnergyRestore UMETA(DisplayName = "Energy Restore (Energy gain)"),
    BuffDamage UMETA(DisplayName = "Buff Damage (Increase outgoing damage)"),
    BuffDefense UMETA(DisplayName = "Buff Defense (Reduce incoming damage)"),
    BuffSpeed UMETA(DisplayName = "Buff Speed (Increase turn speed)"),
    BuffCrit UMETA(DisplayName = "Buff Crit (Increase crit chance)"),
    Silence UMETA(DisplayName = "Silence (Prevent energy gain)"),
    Cleanse UMETA(DisplayName = "Cleanse (Remove debuffs)"),
    Gamble UMETA(DisplayName = "Gamble (Random effects)"),
    Transform UMETA(DisplayName = "Transform (Absorb and change)")
};