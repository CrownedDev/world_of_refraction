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
    DualBlades UMETA(DisplayName = "Dual Blades"),
    Axe UMETA(DisplayName = "Axe"),
    Hammer UMETA(DisplayName = "Hammer"),
    Bow UMETA(DisplayName = "Bow"),
    Fists UMETA(DisplayName = "Gauntlets"),
    Scythe UMETA(DisplayName = "Scythe"),
    Gun UMETA(DisplayName = "Gun")
        swordandshield UMETA(DisplayName = "Sword and Shield")

};