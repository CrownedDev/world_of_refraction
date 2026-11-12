// EGuardStyle.h
// B-392 (D-170): the two guards. A character with a weapon in hand parries with it (the weapon guard, that weapon
// type's own) or puts it away and parries unarmed (the body's guard, the character's own). Looks only: the timing
// window is the same in both (D-170). Resolved by ULoadoutComponent::GetGuardStyle / GetGuardMontage.
// Saved as a byte (FSavedPartyMember): append-only, explicit values.

#pragma once

#include "CoreMinimal.h"
#include "EGuardStyle.generated.h"

UENUM(BlueprintType)
enum class EGuardStyle : uint8
{
	/** Parry with the weapon in hand (the default once a weapon is equipped). */
	Weapon = 0 UMETA(DisplayName = "Weapon"),
	/** Put the weapon away and parry with the body (always, with no weapon in hand). */
	Unarmed = 1 UMETA(DisplayName = "Unarmed")
};
