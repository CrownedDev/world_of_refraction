// EvolvedResonatorState.h
// Runtime state tracking for evolved Resonators

#pragma once

#include "CoreMinimal.h"
#include "SpellElement.h"
#include "EvolvedResonatorState.generated.h"

/**
 * Tracks evolved Resonator-specific state
 * Used by CharacterDataComponent at runtime
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FEvolvedResonatorState
{
    GENERATED_BODY()

    /** Locked element from evolution */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution")
    ESpellElement LockedElement = ESpellElement::Generic;

    /** Chose second weapon (mutually exclusive with bHasSecondRing) */
    UPROPERTY(BlueprintReadWrite, Category = "Evolution")
    bool bHasSecondWeapon = false;

    /** Chose second ring (mutually exclusive with bHasSecondWeapon) */
    UPROPERTY(BlueprintReadWrite, Category = "Evolution")
    bool bHasSecondRing = false;

    /** If second ring broke, locked to primary only */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution")
    bool bSecondRingBroken = false;

    /** Can use second equipment slot? */
    bool CanUseSecondSlot() const
    {
        return !bSecondRingBroken;
    }

    /** Has chosen their bonus slot type? */
    bool HasChosenBonusType() const
    {
        return bHasSecondWeapon || bHasSecondRing;
    }
};