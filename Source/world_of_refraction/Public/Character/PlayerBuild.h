// PlayerBuild.h
// B-554 (D-228, one-player stage 3): the player's character data is built, not authored. A copy of the shared player
// asset (UPartySessionSubsystem::PlayerCharacter, DA_Character_Player) takes the picks' name and class and the class
// table's numbers for that class and element (UClassDefaultLoadoutData::FindNumbers): world levels, action speed, the
// innate element and Broken Darkness, the portrait. Every reader of UCharacterData then works unchanged
// (OnePlayerCharacterPlan.md section 2). The copy has a fixed name (BUILT_NAME), so the pool's key for the player stays
// the same across loads; a save stores the picks and the load builds it again.
// B-556 (stage 4): its inventory is made from the picks and the class table (MakeInventory), not an old Lord's.
// Debug: wor.Player.PrintBuilt.

#pragma once

#include "CoreMinimal.h"

class UCharacterData;
class UGameInstance;
class UInventoryData;
class UClassDefaultLoadoutData;
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

	/** BuildFrom with the game's own assets: the player asset, the class table and an inventory made from the picks
	 *  (MakeInventory). Null, with a warning, when the picks are unset or the player asset is missing. */
	WORLD_OF_REFRACTION_API UCharacterData *Build(const FPlayerPicks &Picks, const UGameInstance *GameInstance);

	/** B-556 (D-228, stage 4): the built player's own inventory: the picked pieces as its starting gear (no base kit),
	 *  a Resonator's picked crystals in its stock, and one authored loadout with the class numbers' innate spells and
	 *  Broken Darkness pools (Numbers null: none). Transient. */
	WORLD_OF_REFRACTION_API UInventoryData *MakeInventory(const FPlayerPicks &Picks, const UClassDefaultLoadoutData &Table,
														  const FClassElementNumbers *Numbers);

	/** A character made by BuildFrom (its name starts with BUILT_NAME and it lives in the transient package). */
	WORLD_OF_REFRACTION_API bool IsBuilt(const UCharacterData *Character);

	/** Debug: the built character's name, class, element, levels, action speed and inventory, one line. */
	WORLD_OF_REFRACTION_API FString GetBuiltString(const UCharacterData *Character);
}
