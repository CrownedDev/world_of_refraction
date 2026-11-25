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
	/** Tiles in a row: B-598c (mockup menus/loadout/v5), the left column's five. */
	constexpr int32 COLUMNS = 5;
	/** OBSOLETE (B-598c: one grid width now; delete after Crown's PIE pass): D-249's whole-width row. */
	constexpr int32 COLUMNS_WIDE = 14;

	/** The filter tabs after All: each weapon type present, once, in the enum's order. */
	WORLD_OF_REFRACTION_API TArray<EWeaponType> GetTypesPresent(TConstArrayView<EWeaponType> Types);

	/** The tile the keys go to from Index in a grid of Count tiles, Columns to a row: left and right step one tile and
	 *  run on into the row before or after; up and down keep the column, and down from the row above a short last row
	 *  lands on its last tile. A move off the grid stays put. INDEX_NONE for an empty grid. */
	WORLD_OF_REFRACTION_API int32 Move(int32 Index, int32 Count, int32 Columns, int32 DeltaX, int32 DeltaY);

	/** D-248 (Crown): the line under a piece's name: its type with the tier in brackets, "Sword (F)". No tier badge. */
	WORLD_OF_REFRACTION_API FText GetTypeTierLine(const FText &TypeName, EItemTier Tier);

	// ---- D-248 step 2: the pop-up card beside the focused tile ----

	/** Where tile Index's top-left corner is in the grid's view: its place in the rows (RowPitch is a tile with the
	 *  padding around it, Padding that padding on one side), moved up by how far the grid is scrolled. */
	WORLD_OF_REFRACTION_API FVector2D GetTilePosition(int32 Index, int32 Columns, float RowPitch, float Padding, float ScrollOffset);

	/** The scroll offset that shows Row whole in a view ViewHeight tall: Offset when it already does, else the least
	 *  move (up to the row's top, or down until its bottom is at the view's). */
	WORLD_OF_REFRACTION_API float ScrollToRow(float Offset, int32 Row, float RowPitch, float ViewHeight);

	/** Where the card goes, and which way it turned. */
	struct FCardPlace
	{
		/** The card's top-left corner in the view. */
		FVector2D Position = FVector2D::ZeroVector;
		/** On the tile's left (no room on its right). */
		bool bLeft = false;
		/** Its bottom on the tile's bottom (no room below the tile's top). */
		bool bUp = false;
	};

	/** The card beside the tile at TilePosition, Gap away: on its right with their tops level; on its left when the
	 *  right has no room; its bottom level with the tile's when it would run past the view's bottom. Then kept inside
	 *  the view (a card bigger than the view starts at the view's corner). */
	WORLD_OF_REFRACTION_API FCardPlace PlaceCard(const FVector2D &TilePosition, float TileSize, const FVector2D &CardSize, const FVector2D &ViewSize, float Gap);

	/** Debug: the grid as rows of tiles, the selected one in brackets: "[Sword] Sword x2 Bow (locked)". */
	WORLD_OF_REFRACTION_API FString GetGridString(const TArray<FLoadoutV2GridTile> &Tiles, int32 Columns, int32 Selected);
}
