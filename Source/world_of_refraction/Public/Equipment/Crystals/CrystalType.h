// CrystalType.h
// Defines the crystal + weapon-stone types for the Items System

#pragma once

#include "CoreMinimal.h"
#include "CrystalType.generated.h"

/**
 * Enum representing the crystal and weapon-stone types in World of Refraction
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
    DamageStone UMETA(DisplayName = "Damage Stone"),
    // Appended after DamageStone (=12); weapon-stone sub-type that grants ability
    // slots. Attach-only (never a consumable). Do NOT reorder/insert above —
    // value is the serialized .uasset/SaveGame identity.
    AbilityStone UMETA(DisplayName = "Ability Stone"),
    // Appended after AbilityStone (=13); weapon-stone sub-type that buffs Defense.
    // Same append-only serialized-identity rule — do NOT reorder/insert above.
    DefenseStone UMETA(DisplayName = "Defense Stone")
};