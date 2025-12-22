// EWeaponSlotType.h
// Slot type enums for character loadout configuration

#pragma once

#include "CoreMinimal.h"
#include "EWeaponSlotType.generated.h"

/** Primary slot type (Generic/Caster: Weapon, Ring, or Evolution | Resonator: Weapon or Evolution) */
UENUM(BlueprintType)
enum class EPrimarySlotType : uint8
{
    Weapon UMETA(DisplayName = "Weapon"),
    Ring UMETA(DisplayName = "Ring"),
    Evolution UMETA(DisplayName = "Evolution")
};

/** Secondary slot type (Generic only - None or Weapon) */
UENUM(BlueprintType)
enum class ESecondarySlotType : uint8
{
    None UMETA(DisplayName = "None"),
    Weapon UMETA(DisplayName = "Weapon")
};