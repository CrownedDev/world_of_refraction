// LoadoutV2GridModel.h
// D-248 step 1 (mockup menus/loadout/v4/1b_equipment_grid_tooltip.png): the picture grid the Equipment tab shows while a
// weapon slot is being filled, headless and testable. A tile is a piece's picture (one per weapon type for now), a tick
// when it is worn, a count for a stack, a lock when it can't go in the slot. Filter tabs: All, then one per type present.
// Drawn by ULoadoutV2ScreenWidget; what may go in a slot is still WoRLoadoutV2's (the rules don't change).

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Weapons/EWeaponType.h"
#include "Inventory/ItemTier.h"
#include "UI/WoRIcon.h"

/** One tile of the grid. */
struct FLoadoutV2GridTile
{
	/** The picture: the weapon type's icon; its Name is the type in a word (shown when the table has no picture). */
	FWoRIcon Icon;
	/** A stack's size: "x2" on the tile above one. */
	int32 Count = 1;
	/** In the slot, or in another entry of this loadout: the tick. */
	bool bWorn = false;
	/** It can't go in the focused slot (the reason is in the details): greyed, with the lock. */
	bool bLocked = false;
};

namespace WoRLoadoutV2Grid
{
	/** Tiles in a row. TODO: 10 once the details move to the pop-up card (step 2) and the grid takes the box's width. */
	constexpr int32 COLUMNS = 7;

	/** The filter tabs after All: each weapon type present, once, in the enum's order. */
	WORLD_OF_REFRACTION_API TArray<EWeaponType> GetTypesPresent(TConstArrayView<EWeaponType> Types);

	/** The tile the keys go to from Index in a grid of Count tiles, Columns to a row: left and right step one tile and
	 *  run on into the row before or after; up and down keep the column, and down from the row above a short last row
	 *  lands on its last tile. A move off the grid stays put. INDEX_NONE for an empty grid. */
	WORLD_OF_REFRACTION_API int32 Move(int32 Index, int32 Count, int32 Columns, int32 DeltaX, int32 DeltaY);

	/** D-248 (Crown): the line under a piece's name: its type with the tier in brackets, "Sword (F)". No tier badge. */
	WORLD_OF_REFRACTION_API FText GetTypeTierLine(const FText &TypeName, EItemTier Tier);

	/** Debug: the grid as rows of tiles, the selected one in brackets: "[Sword] Sword x2 Bow (locked)". */
	WORLD_OF_REFRACTION_API FString GetGridString(const TArray<FLoadoutV2GridTile> &Tiles, int32 Columns, int32 Selected);
}
