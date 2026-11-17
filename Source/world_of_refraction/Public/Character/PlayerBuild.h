// PlayerBuild.h
// B-554 (D-228, one-player stage 3): the player's character data is built, not authored. A copy of the shared player
// asset (UPartySessionSubsystem::PlayerCharacter, DA_Character_Player) takes the picks' name and class and the class
// table's numbers for that class and element (UClassDefaultLoadoutData::FindNumbers): world levels, action speed, the
// innate element and Broken Darkness, the portrait. Every reader of UCharacterData then works unchanged
// (OnePlayerCharacterPlan.md section 2). The copy has a fixed name (BUILT_NAME), so the pool's key for the player stays
// the same across loads; a save stores the picks and the load builds it again.
// Shortcut until stage 4 (the picks set the gear): the built character takes the inventory of today's start character
// for that class and element (WoRNewGame::FindStartCharacter), so its innate spells and starting gear are as before.
// Debug: wor.Player.PrintBuilt.

#pragma once

#include "CoreMinimal.h"

class UCharacterData;
class UGameInstance;
class UInventoryData;
struct FClassElementNumbers;
struct FPlayerPicks;

namespace WoRPlayerBuild
{
	/** The built player's object name, in the transient package. */
	WORLD_OF_REFRACTION_API extern const TCHAR *BUILT_NAME;

	/** Template copied, named ObjectName (an older object of that name is renamed away first), with the picks and the
	 *  class numbers filled in. Numbers null: the template's levels and portrait stay. An ActionSpeed of 0 is the
	 *  normal speed (no Action Speed points: animations at their own pace). Null without a template or picks. */
	WORLD_OF_REFRACTION_API UCharacterData *BuildFrom(const FPlayerPicks &Picks, const UCharacterData *Template,
													  const FClassElementNumbers *Numbers, int32 ActionSpeed,
													  UInventoryData *Inventory, FName ObjectName);

	/** BuildFrom with the game's own assets: the player asset, the class table and today's start character's inventory
	 *  (see the shortcut above). Null, with a warning, when the picks are unset or the player asset is missing. */
	WORLD_OF_REFRACTION_API UCharacterData *Build(const FPlayerPicks &Picks, const UGameInstance *GameInstance);

	/** A character made by BuildFrom (its name starts with BUILT_NAME and it lives in the transient package). */
	WORLD_OF_REFRACTION_API bool IsBuilt(const UCharacterData *Character);

	/** Debug: the built character's name, class, element, levels, action speed and inventory, one line. */
	WORLD_OF_REFRACTION_API FString GetBuiltString(const UCharacterData *Character);
}
