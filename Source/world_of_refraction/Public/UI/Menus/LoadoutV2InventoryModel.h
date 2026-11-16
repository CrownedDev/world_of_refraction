// LoadoutV2InventoryModel.h
// B-531 (D-230): the loadout screen's Inventory tab, first in the tab row: what the character owns and is not wearing,
// grouped Weapons, Adapters, Evolutions, Crystals (each group under a header line). Read from the real store (the
// member's inventory, its evolutions and its crystal pouch); a piece in the active loadout is worn and not listed.
// D-230 "Stacking": pieces that are exactly the same show as one line with a count ("Adapter Ring x5"): the same piece
// asset and tier, both sockets empty, no skill picks, no rolled bonus. Display only: each piece stays its own item.
// One grouping helper (GetStackKey + GroupStacks) serves this tab and the Add equipment window's lists.
// Debug: GetInventoryPageString (logged when the tab opens).

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FEvolutionInventoryEntry.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Inventory/ItemTier.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "UI/WoRIcon.h"

struct FCombatLoadout;
struct FCrystalId;

enum class ELoadoutV2InventoryRowKind : uint8
{
	/** A group's name ("Weapons"); nothing to pick. */
	Header,
	Weapon,
	Adapter,
	Evolution,
	/** A crystal or an augment stone in the pouch (a stack: Count is how many). */
	Crystal
};

/** One line of the Inventory tab. A stacked line holds its first piece (the one choosing it takes). */
struct FLoadoutV2InventoryRow
{
	ELoadoutV2InventoryRowKind Kind = ELoadoutV2InventoryRowKind::Header;
	FText Name;
	/** How many the line stands for (a stack of exactly-same pieces, or a pouch stack); 1 for a piece of its own. */
	int32 Count = 1;
	/** The line's type icon (a weapon's type, an adapter's frame with its crystal, an element badge). */
	FWoRIcon Icon;
	bool bHasTier = false;
	EItemTier Tier = EItemTier::F_Tier;
	FWeaponInventoryEntry Weapon;
	FRingInventoryEntry Adapter;
	FEvolutionInventoryEntry Evolution;
	/** A crystal or stone line's piece, as a socket would hold it (for the details box). */
	FRuntimeAttachedItem Crystal;

	bool IsPiece() const { return Kind != ELoadoutV2InventoryRowKind::Header; }
};

struct FLoadoutV2InventoryPage
{
	TArray<FLoadoutV2InventoryRow> Rows;
};

/** D-230 "Stacking": a run of lines that show as one: the first line's index and how many. */
struct FLoadoutV2Stack
{
	int32 First = INDEX_NONE;
	int32 Count = 0;
};

namespace WoRLoadoutV2
{
	/** D-230: what exactly-same pieces share; empty for a piece that stands alone (built, socketed, picked or rolled). */
	WORLD_OF_REFRACTION_API FString GetStackKey(const FWeaponInventoryEntry &Weapon);
	WORLD_OF_REFRACTION_API FString GetStackKey(const FRingInventoryEntry &Adapter);

	/** D-230: lines with the same non-empty key, as one stack each, in first-seen order; an empty key stands alone. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2Stack> GroupStacks(const TArray<FString> &Keys);

	/** D-230: a stacked line's name: "Adapter Ring x5"; the name alone for one. */
	WORLD_OF_REFRACTION_API FText GetStackedName(const FText &Name, int32 Count);

	/** B-531 (D-230): the Inventory tab: the owned pieces Worn doesn't hold, the evolutions not in use, the pouch's
	 *  stacks; grouped, each group under its header, an empty group left out. */
	WORLD_OF_REFRACTION_API FLoadoutV2InventoryPage BuildInventoryPage(const FCombatLoadout &Worn, const TArray<FWeaponInventoryEntry> &Weapons,
																	   const TArray<FRingInventoryEntry> &Adapters, const TArray<FEvolutionInventoryEntry> &Evolutions,
																	   const TSet<FGuid> &EvolutionsInUse, const TMap<FCrystalId, int32> &Pouch);

	/** Debug: the tab as shown, one line per row ("Adapters / Adapter Ring x5 / ..."). */
	WORLD_OF_REFRACTION_API FString GetInventoryPageString(const FLoadoutV2InventoryPage &Page);
}
