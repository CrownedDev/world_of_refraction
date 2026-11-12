// EParryElementPriority.h
// B-393 (D-170, Q-393): a landed parry's effect is coloured by the element behind it. When the weapon guard has two
// sources (the weapon's crystal and the character's own magic: a Refractor's element, else its adapter's crystal), this
// loadout setting says which colours it; the other is used when the first has no element. Looks only.
// Resolved by ParryVisuals (Combat/Presentation/ParryVisuals.h). Saved as a byte (FSavedPartyMember): append-only,
// explicit values.

#pragma once

#include "CoreMinimal.h"
#include "EParryElementPriority.generated.h"

UENUM(BlueprintType)
enum class EParryElementPriority : uint8
{
	/** The weapon's crystal first (the default: the thing that parried colours it, Q-393). */
	Weapon = 0 UMETA(DisplayName = "Weapon"),
	/** The character's own magic first: a Refractor's element, else the adapter's crystal. */
	Magic = 1 UMETA(DisplayName = "Magic")
};
