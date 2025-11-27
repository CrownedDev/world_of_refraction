// ESlotType.h
// Slot type enums for character loadout configuration

#pragma once

#include "CoreMinimal.h"
#include "ESlotType.generated.h"

/** Primary slot type (Caster only - Weapon or Ring) */
UENUM(BlueprintType)
enum class EPrimaryWeaponSlotType : uint8
{
    Weapon UMETA(DisplayName = "Weapon"),
    Ring UMETA(DisplayName = "Ring")
};

/** Secondary slot type (Generic only - None, Weapon, or Ring) */
UENUM(BlueprintType)
enum class ESecondaryWeaponSlotType : uint8
{
    None UMETA(DisplayName = "None"),
    Weapon UMETA(DisplayName = "Weapon"),
    Ring UMETA(DisplayName = "Ring")
};