// EInfusionSourceOption.h
// Player-selectable infusion source - determines element AND weapon stat trade-off

#pragma once

#include "CoreMinimal.h"
#include "EInfusionSourceOption.generated.h"

/**
 * Player-selectable infusion source
 *
 * Core Trade-off:
 * - None = Physical damage + Weapon stats apply
 * - Any other = Elemental damage + Weapon stats ignored
 *
 * Availability by class:
 * - Caster: None, Innate, WeaponCrystal*, Evolution**
 * - Resonator: None, ActiveRing, WeaponCrystal*, Evolution**
 * - Generic: None, , WeaponCrystal*, Evolution**
 *
 * * If non-Ilodite crystal equipped
 * ** If evolved
 * *** If SecondarySlotType == Ring
 */
UENUM(BlueprintType)
enum class EInfusionSourceOption : uint8
{
    /** No infusion - Physical damage, weapon stats apply */
    None UMETA(DisplayName = "None (Physical)"),

    /** Caster's innate element - L2 costs HP */
    Innate UMETA(DisplayName = "Innate Element"),

    /** Resonator's currently equipped ring - L2 risks ring break */
    ActiveRing UMETA(DisplayName = "Active Ring"),

    /** Gaster/Generics's current primary ring - L2 risks ring break */
    PrimaryRing UMETA(DisplayName = "Primary Ring"),

    /** Weapon's slotted crystal element - L2 risks crystal break */
    WeaponCrystal UMETA(DisplayName = "Weapon Crystal"),

    /** Evolution element - L2 costs HP + self-status buildup */
    Evolution UMETA(DisplayName = "Evolution")
};

/**
 * Helper functions for infusion source options
 */
namespace InfusionSourceOptionHelpers
{
    /** Does this source provide an element? (false = physical only) */
    inline bool HasElement(EInfusionSourceOption Option)
    {
        return Option != EInfusionSourceOption::None;
    }

    /** Do weapon stats apply with this source? */
    inline bool WeaponStatsApply(EInfusionSourceOption Option)
    {
        return Option == EInfusionSourceOption::None;
    }

    /** Get display name for UI */
    inline FString GetDisplayName(EInfusionSourceOption Option)
    {
        switch (Option)
        {
        case EInfusionSourceOption::None:
            return TEXT("Physical");
        case EInfusionSourceOption::Innate:
            return TEXT("Innate");
        case EInfusionSourceOption::ActiveRing:
            return TEXT("Ring");
        case EInfusionSourceOption::WeaponCrystal:
            return TEXT("Crystal");
        case EInfusionSourceOption::Evolution:
            return TEXT("Evolution");
        default:
            return TEXT("Unknown");
        }
    }
}