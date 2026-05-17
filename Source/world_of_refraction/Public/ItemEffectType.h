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
    // StatusClear — Quartz consumable: clears a portion of the target's status
    // bar. Renamed in place from the removed Transform value (item-system-redesign);
    // see the EItemEffectType Transform->StatusClear redirect in DefaultEngine.ini.
    StatusClear UMETA(DisplayName = "Status Clear (Quartz bar clear)"),
    Repair UMETA(DisplayName = "Weapon Repair")
};