// CrystalType.h
// Defines the 11 crystal types for the Items System

#pragma once

#include "CoreMinimal.h"
#include "CrystalType.generated.h"

/**
 * Enum representing the 11 crystal types in World of Refraction
 * Each crystal has a unique effect and color theme
 * Raw crystals = consumable items
 * Refined crystals = slot into weapons/rings
 */
UENUM(BlueprintType)
enum class ECrystalType : uint8
{
    None = 0 UMETA(Hidden),
    Garnet UMETA(DisplayName = "Garnet (Fire - Damage)"),
    Sapphire UMETA(DisplayName = "Sapphire (Water - Healing)"),
    Citrine UMETA(DisplayName = "Citrine (Lightning - Energy)"),
    Emerald UMETA(DisplayName = "Emerald (Wind - Speed)"),
    Amber UMETA(DisplayName = "Amber (Earth - Defense)"),
    Opal UMETA(DisplayName = "Opal (Light - Crit/Info)"),
    Onyx UMETA(DisplayName = "Onyx (Darkness - Silence)"),
    Amethyst UMETA(DisplayName = "Amethyst (Void - Gambling)"),
    Iolite UMETA(DisplayName = "Iolite (Reality - Cleanse)"),
    Quartz UMETA(DisplayName = "Quartz (None - Transform)"),
    Whetstone UMETA(DisplayName = "Whetstone")
};