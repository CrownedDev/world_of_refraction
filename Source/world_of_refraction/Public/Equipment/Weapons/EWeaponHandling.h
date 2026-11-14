// EWeaponHandling.h
// B-477 (D-209, Crown 2026-11-14): how a weapon may be held in a Generic's weapon set.

#pragma once

#include "CoreMinimal.h"
#include "EWeaponHandling.generated.h"

/**
 * How a weapon can be held (D-209).
 *
 * SingleOnly   — held alone, never paired. The default: an asset stays single until the editor step (B-478) sets it.
 * DualOnly     — only as a same-type pair (two of these or of SingleOrDual); can't be equipped alone.
 * SingleOrDual — either.
 *
 * Only two weapons of the same type pair (two swords, two daggers); the pair stands in the main hand's (the
 * first-chosen weapon's) DualWieldStance, so the order matters.
 */
UENUM(BlueprintType)
enum class EWeaponHandling : uint8
{
    SingleOnly   UMETA(DisplayName = "Single only"),
    DualOnly     UMETA(DisplayName = "Dual only"),
    SingleOrDual UMETA(DisplayName = "Single or dual")
};
