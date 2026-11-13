// InventoryScreenModel.h
// B-441 (D-181, D-177, D-195, UIScreens #4, mockups screen_mockups/menus/inventory/): what the inventory screen
// shows, with no widget in it, so the specs read it headless. UInventoryScreenWidget draws it.
//
// Three tabs over one member: Gear (its weapons and adapters), Pouch (its carried stacks and its slots) and Chest
// (the shared home chest: stored gear and stacks). One verb menu on a tile: Give and Store, and Take only on the
// Chest tab (D-195). A verb that can't run is listed with the reason (WoRInventoryVerbs::GetRefusal), never hidden.
// The rules and the moves are WoRInventoryVerbs' (Inventory/InventoryVerbs.h).
// Debug: wor.UI.PrintInventory logs GetInventoryScreenString (every party member's three tabs).

#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryVerbs.h"
#include "Skills/Definitions/ESpellElement.h"
#include "UI/Menus/LoadoutScreenModel.h"

enum class EInventoryScreenTab : uint8
{
	Gear,
	Pouch,
	Chest
};

/** One tile of the grid: what the verbs act on (Row) and how it reads. */
struct FInventoryScreenTile
{
	WoRInventoryVerbs::FInventoryRow Row;
	FText Name;
	FText Tier;
	/** "Weapon · Sword", "Crystal · Fire", "Stone". */
	FText KindLine;
	/** The letter on the tile until there are icons: W, A, C, S. */
	FText Mark;
	ESpellElement Element = ESpellElement::None;
	/** Gear a loadout of this member wears (the tile's "E"). */
	bool bWorn = false;
	TArray<FLoadoutScreenDetail> Details;
	/** What the thing does, or a rule worth saying; may be empty. */
	FText Note;
};

struct FInventoryScreenPage
{
	TArray<FInventoryScreenTile> Tiles;
	/** "Pouch  7 of 10 slots", "Home chest"; empty on the Gear tab. */
	FText Heading;
	/** Pouch tab: the slots the pouch has, and the ones the Jeweler still sells (drawn locked). */
	int32 Slots = 0;
	int32 LockedSlots = 0;
	/** Why the tab is empty; empty when it has tiles. */
	FText EmptyNote;
};

/** One line of the verb menu: greyed, with Refusal under it, when it can't run. */
struct FInventoryScreenVerb
{
	EInventoryVerb Verb = EInventoryVerb::Give;
	FText Name;
	FText Refusal;
};

/** One line of "Give to": a party member, how much room it has, and why it can't receive (empty when it can). */
struct FInventoryScreenReceiver
{
	FName Key;
	FText Name;
	/** "4 slots free", "full"; empty for gear. */
	FText Note;
	FText Refusal;
};

namespace WoRInventoryScreen
{
	WORLD_OF_REFRACTION_API const TArray<EInventoryScreenTab> &GetTabs();
	WORLD_OF_REFRACTION_API FText GetTabName(EInventoryScreenTab Tab);

	WORLD_OF_REFRACTION_API FInventoryScreenPage BuildPage(UWorld *World, FName Member, EInventoryScreenTab Tab);

	/** Give and Store; Take too on the Chest tab, and nowhere else (D-195). Give is refused only when no other
	 *  member can receive the tile (the reason is the first member's). */
	WORLD_OF_REFRACTION_API TArray<FInventoryScreenVerb> BuildVerbs(UWorld *World, FName Member, EInventoryScreenTab Tab, const FInventoryScreenTile &Tile);

	/** The other party members, for Give. */
	WORLD_OF_REFRACTION_API TArray<FInventoryScreenReceiver> BuildReceivers(UWorld *World, FName Member, const FInventoryScreenTile &Tile);

	/** How many of a stack Verb moves: all of it with the whole-stack toggle on, else one; never more than fits the
	 *  pouch it goes to (Take: Member's; Give: Receiver's), so a big stack moves what fits. Gear: one. */
	WORLD_OF_REFRACTION_API int32 GetMoveCount(const UWorld *World, EInventoryVerb Verb, FName Member, FName Receiver, const FInventoryScreenTile &Tile,
											   bool bWholeStack);

	WORLD_OF_REFRACTION_API FString GetInventoryScreenString(UWorld *World);
}
