// InventoryVerbs.h
// B-339 (D-131): the one inventory screen's verbs over a member's store and the shared home chest, as plain functions
// so the placeholder screen (wor.UI.Inventory, UInventoryWidget) and specs route through the same code:
//   Give      a weapon / adapter (WoRGive::TryGiveGear) or a pouch stack (TryGiveCarried) to another member
//   Store     a weapon / adapter into the chest (TryPutInChest) or a pouch stack into storage (MoveToStorage)
//   Take      a chest weapon / adapter (TryTakeFromChest) or a stored stack into the pouch (MoveToCarried)
//   Dismantle a weapon / adapter (UEconomyService::DismantleWeapon / DismantleRing) or a pouch stack (DismantleCrystal)
//   Convert   the weaker copy of a duplicate to Gold (UEconomyService::ResolveDuplicate, D-115)
// GetRefusal gives the player-facing reason a verb can't run on a row (empty = it can); GetVerbsFor lists the verbs
// with no reason, and Run refuses with the reason. Dismantle and Convert act through the member's spawned actor (the
// economy needs it). Verb names: WoRGameNames::GetInventoryVerbName (D-062). Debug: GetRowsString.

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

	/** The verb as the player reads it ("Give", "Store", ...): WoRGameNames::GetInventoryVerbName. B-366b: Convert names
	 *  the currency of the place WorldContext is in (UEconomyService::GetPlaceCurrency). */
	WORLD_OF_REFRACTION_API FText GetVerbName(const UObject *WorldContext, EInventoryVerb Verb);

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

	/** Why Verb can't run on Row for Member (Receiver for Give), for the player; empty when it can. Covers the
	 *  rules the old screens and the economy refuse on: chest rows only Take; worn gear isn't stored; no give to
	 *  yourself, of a loadout's primary (D-116) or of gear to a run-only member (Q-321); no dismantle or convert of
	 *  a home piece during a run (Q-314b) or of a loadout's primary; Convert only for a duplicate's weaker copy. */
	WORLD_OF_REFRACTION_API FText GetRefusal(UWorld *World, EInventoryVerb Verb, FName Member, FName Receiver, const FInventoryRow &Row);

	/** The verbs with no refusal for Row. */
	WORLD_OF_REFRACTION_API TArray<EInventoryVerb> GetVerbsFor(UWorld *World, FName Member, FName Receiver, const FInventoryRow &Row);

	/** What the player confirms before Verb runs (D-016's dismantle warning for equipped gear; the Gold a convert
	 *  gives); empty when it runs at once. */
	WORLD_OF_REFRACTION_API FText GetConfirmText(UWorld *World, EInventoryVerb Verb, FName Member, const FInventoryRow &Row);

	/** Runs Verb on Row for Member (Receiver for Give; Count for stacks, clamped to the row's count). Empty on success;
	 *  else the reason, for the player. */
	WORLD_OF_REFRACTION_API FText Run(UWorld *World, EInventoryVerb Verb, FName Member, FName Receiver, const FInventoryRow &Row,
									  int32 Count);

	/** Debug: each row with its verbs (Receiver for Give), one line each. */
	WORLD_OF_REFRACTION_API FString GetRowsString(UWorld *World, FName Member, FName Receiver);
}
