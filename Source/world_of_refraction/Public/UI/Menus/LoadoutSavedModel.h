// LoadoutSavedModel.h
// B-440e (D-111, UIScreens #2: "Y opens the five saved ones"): the loadout screen's saved loadouts, without widgets
// so the specs read it. One row per saved loadout (the active one marked, a lore one read only), then "New loadout"
// while there is room for another of the player's own (PoolConstants::MAX_CUSTOM_LOADOUTS, lore ones not counted).
// Each verb returns the refusal (empty when it went through), as WoRLoadoutSkills::ApplySkill does.

#pragma once

#include "CoreMinimal.h"

class UInventoryComponent;
class ULoadoutComponent;
class UWorld;
struct FLoadoutScreenMember;

/** One line of the saved loadouts list. */
struct WORLD_OF_REFRACTION_API FLoadoutSavedRow
{
	/** The loadout's index in the saved list; INDEX_NONE for the "New loadout" line. */
	int32 LoadoutIndex = INDEX_NONE;
	FText Name;
	/** "Active", "Lore: rename a copy", or empty. */
	FText Note;
	bool bActive = false;
	bool bLore = false;

	bool IsNewLine() const { return LoadoutIndex == INDEX_NONE; }
};

struct WORLD_OF_REFRACTION_API FLoadoutSavedList
{
	TArray<FLoadoutSavedRow> Rows;
	/** The player's own loadouts and the most allowed ("3 of 5"); lore ones are extra. */
	int32 CustomCount = 0;
	int32 MaxCustom = 0;
	/** Why the list is empty (no member, no loadout component). */
	FText Refusal;
};

namespace WoRLoadoutSaved
{
	/** Names are typed by the player; longer ones are refused. */
	constexpr int32 MAX_LOADOUT_NAME_LENGTH = 24;

	WORLD_OF_REFRACTION_API FLoadoutSavedList BuildList(const UInventoryComponent &Inventory);

	/** Makes LoadoutIndex the one the character fights with. */
	WORLD_OF_REFRACTION_API FText SetActive(ULoadoutComponent &Loadout, UInventoryComponent &Inventory, int32 LoadoutIndex);

	/** A new empty loadout named "Loadout N" (the first free number); OutIndex is where it went. */
	WORLD_OF_REFRACTION_API FText CreateNew(ULoadoutComponent &Loadout, UInventoryComponent &Inventory, int32 &OutIndex);

	/** A copy of SourceIndex named "<name> copy", the player's own even of a lore loadout (B-311). */
	WORLD_OF_REFRACTION_API FText Duplicate(ULoadoutComponent &Loadout, UInventoryComponent &Inventory, int32 SourceIndex, int32 &OutIndex);

	/** Renames LoadoutIndex to NewName (trimmed). Refused: empty, too long, or a lore loadout (read only, D-111). */
	WORLD_OF_REFRACTION_API FText Rename(ULoadoutComponent &Loadout, UInventoryComponent &Inventory, int32 LoadoutIndex, const FText &NewName);

	/** "Loadout N" with the first N no saved loadout uses. */
	WORLD_OF_REFRACTION_API FString MakeNewName(const UInventoryComponent &Inventory);

	/** The shown member's components (WoRLoadoutScreen::FindMemberLoadout); false when either is missing. */
	WORLD_OF_REFRACTION_API bool FindComponents(UWorld *World, const FLoadoutScreenMember &Member, ULoadoutComponent *&OutLoadout,
												UInventoryComponent *&OutInventory);

	/** Debug: the list, one line per row. */
	WORLD_OF_REFRACTION_API FString GetSavedListString(const UInventoryComponent &Inventory);
}
