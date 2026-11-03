// InventoryVerbs.h
// B-339 (D-131): the one inventory screen's verbs over a member's store and the shared home chest, as plain functions
// so the placeholder screen (wor.UI.Inventory, UInventoryWidget) and specs route through the same code:
//   Give      a weapon / adapter (WoRGive::TryGiveGear) or a pouch stack (TryGiveCarried) to another member
//   Store     a weapon / adapter into the chest (TryPutInChest) or a pouch stack into storage (MoveToStorage)
//   Take      a chest weapon / adapter (TryTakeFromChest) or a stored stack into the pouch (MoveToCarried)
//   Dismantle a weapon / adapter (UEconomyService::DismantleWeapon / DismantleRing) or a pouch stack (DismantleCrystal)
//   Convert   the weaker copy of a duplicate to Gold (UEconomyService::ResolveDuplicate, D-115)
// Dismantle and Convert act through the member's spawned actor (the economy needs it); an unspawned member refuses.
// Debug: GetRowsString.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"

class UWorld;

enum class EInventoryVerb : uint8
{
	Give,
	Store,
	Take,
	Dismantle,
	Convert
};

namespace WoRInventoryVerbs
{
	/** Every verb, in the screen's button order. */
	WORLD_OF_REFRACTION_API const TArray<EInventoryVerb> &GetAllVerbs();

	/** The verb as the player reads it ("Give", "Store", ...): the shared button-verb names (D-062). */
	WORLD_OF_REFRACTION_API FText GetVerbName(EInventoryVerb Verb);

	/** One thing the screen lists: a weapon / adapter (GearID) or an item stack (Item, Count); the member's own
	 *  (bInChest false: its gear and pouch) or the shared chest's (bInChest true: chest gear and stored stacks). */
	struct WORLD_OF_REFRACTION_API FInventoryRow
	{
		FGuid GearID;
		FCrystalId Item;
		int32 Count = 0;
		bool bInChest = false;
		FText Label;

		bool IsGear() const { return GearID.IsValid(); }
	};

	/** Member's gear and pouch stacks, then the chest's gear and the stored stacks. */
	WORLD_OF_REFRACTION_API TArray<FInventoryRow> GatherRows(const UWorld *World, FName Member);

	/** The verbs that apply to Row: the member's own gear Give / Store / Dismantle (+ Convert when it is the weaker
	 *  copy of a duplicate); its pouch stacks Give / Store / Dismantle; chest gear and stored stacks Take. */
	WORLD_OF_REFRACTION_API TArray<EInventoryVerb> GetVerbsFor(UWorld *World, FName Member, const FInventoryRow &Row);

	/** Runs Verb on Row for Member (Receiver for Give; Count for stacks, clamped to the row's count). Empty on success;
	 *  else the reason, for the player. */
	WORLD_OF_REFRACTION_API FText Run(UWorld *World, EInventoryVerb Verb, FName Member, FName Receiver, const FInventoryRow &Row,
									  int32 Count);

	/** Debug: each row with its verbs, one line each. */
	WORLD_OF_REFRACTION_API FString GetRowsString(UWorld *World, FName Member);
}
