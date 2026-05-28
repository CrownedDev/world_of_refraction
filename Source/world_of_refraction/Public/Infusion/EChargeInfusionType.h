// EChargeInfusionType.h
// Determines what type of action is being charged for the infusion system

#pragma once

#include "CoreMinimal.h"
#include "EChargeInfusionType.generated.h"

/**
 * Type of charge infusion being performed
 *
 * Determines how charge levels (0, 1, 2) affect the action:
 *
 * Spell:
 *   - L0: Base size, base status, base damage
 *   - L1: 1.5x size, +50% status buildup, base damage
 *   - L2: 2.0x size, base status, +30% damage
 *   - Element source: Always from SpellData.Element
 *
 * Ability:
 *   - L0: Base damage, no status
 *   - L1: Base damage, status buildup from selected source
 *   - L2: +30% damage, no status
 *   - Element source: Player-selected via EInfusionSourceOption
 *
 * HP Cost (both types):
 *   - L0: 0%
 *   - L1: 5% current HP (on charge start)
 *   - L2: 10% current HP (on charge start)
 */
UENUM(BlueprintType)
enum class EChargeInfusionType : uint8
{
    /** No charge - immediate action, no HP cost */
    None UMETA(DisplayName = "None"),

    /** Spell charge - size scales with level, L1=status boost, L2=damage boost */
    Spell UMETA(DisplayName = "Spell Infusion"),

    /** Ability charge - L1=status from source, L2=damage boost */
    Ability UMETA(DisplayName = "Ability Infusion")
};

/**
 * Helper functions for charge infusion type
 */
namespace ChargeInfusionTypeHelpers
{
    /** Is this an active charge type? */
    inline bool IsCharging(EChargeInfusionType Type)
    {
        return Type != EChargeInfusionType::None;
    }

    /** Get display name */
    inline FString GetTypeName(EChargeInfusionType Type)
    {
        switch (Type)
        {
        case EChargeInfusionType::None:
            return TEXT("None");
        case EChargeInfusionType::Spell:
            return TEXT("Spell");
        case EChargeInfusionType::Ability:
            return TEXT("Ability");
        default:
            return TEXT("Unknown");
        }
    }

    /** Does this type use size scaling? */
    inline bool UsesSizeScaling(EChargeInfusionType Type)
    {
        return Type == EChargeInfusionType::Spell;
    }

    /** Does this type use player-selected source? */
    inline bool UsesSelectedSource(EChargeInfusionType Type)
    {
        return Type == EChargeInfusionType::Ability;
    }
}