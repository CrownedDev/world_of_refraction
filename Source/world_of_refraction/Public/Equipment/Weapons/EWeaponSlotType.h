// EWeaponSlotType.h
// Slot type enums for character loadout configuration

#pragma once

#include "CoreMinimal.h"
#include "EWeaponSlotType.generated.h"

/** Primary slot type (Generic/Caster: Weapon, Ring, or Evolution | Resonator: Weapon or Evolution | None: empty primary) */
UENUM(BlueprintType)
enum class EPrimarySlotType : uint8
{
    Weapon UMETA(DisplayName = "Weapon"),
    Ring UMETA(DisplayName = "Ring"),
    Evolution UMETA(DisplayName = "Evolution"),
    None = 3 UMETA(DisplayName = "None")
};

/** Secondary slot type (Generic only - None or Weapon) */
UENUM(BlueprintType)
enum class ESecondarySlotType : uint8
{
    None UMETA(DisplayName = "None"),
    Weapon UMETA(DisplayName = "Weapon")
};