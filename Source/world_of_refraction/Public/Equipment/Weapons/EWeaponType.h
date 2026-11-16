// EWeaponType.h
// Categories of weapons

#pragma once

#include "CoreMinimal.h"
#include "EWeaponType.generated.h"

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
    Sword UMETA(DisplayName = "Sword"),
    Greatsword UMETA(DisplayName = "Greatsword"),
    Spear UMETA(DisplayName = "Spear"),
    Staff UMETA(DisplayName = "Staff"),
    Dagger UMETA(DisplayName = "Dagger"),
    Axe UMETA(DisplayName = "Axe"),
    Hammer UMETA(DisplayName = "Hammer"),
    Bow UMETA(DisplayName = "Bow"),
    Gauntlets UMETA(DisplayName = "Gauntlets"),
    Scythe UMETA(DisplayName = "Scythe"),
    Gun UMETA(DisplayName = "Gun"),
    // B-515 (D-223): a piece held only in the off hand of a pairable weapon (DualOnly handling, no attack). Appended
    // last so the saved values of the other types don't move.
    Shield UMETA(DisplayName = "Shield")
};