// GearGive.h
// B-323 (D-117): giving between your own characters. A member gives an owned weapon or adapter, or a stack from
// its pouch, to another member: one instance moves (the pool's owner changes, UPoolSubsystem::GiveGear /
// GiveCarried). Worn gear comes off the giver's loadouts as it leaves; a loadout's primary is refused (D-116);
// the receiver's duplicate choice fires when it already owns one (D-115, UInventoryComponent::OnDuplicateGearReceived);
// a full pouch refuses. Evolutions are not given yet (Q-323).
// Placeholder screen: wor.UI.Give (UGiveWidget). Debug: WoRGive::GetGiveableString.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"

class UInventoryComponent;
class UPoolSubsystem;
class UWorld;

namespace WoRGive
{
	/** The spawned member whose character key is Key, or null (not in this level). */
	WORLD_OF_REFRACTION_API UInventoryComponent *FindMemberInventory(UWorld *World, FName FromKey);

	/** Gives the giver's weapon or adapter GearID to ToKey. Empty on success; else the reason, for the player.
	 *  When the giver isn't spawned the D-116 primary check can't run: the give goes through and the giver's next
	 *  draw refills its primary (RefillActivePrimary, with a warning). */
	WORLD_OF_REFRACTION_API FText TryGiveGear(UWorld *World, FName FromKey, FName ToKey, const FGuid &GearID);

	/** Gives Count of FromKey's carried Id to ToKey. Empty on success; else the reason, for the player. */
	WORLD_OF_REFRACTION_API FText TryGiveCarried(UWorld *World, FName FromKey, FName ToKey, const FCrystalId &Id, int32 Count);

	// ---- The shared chest (B-324, Q-321): gear nobody owns for now; any member takes from it ----

	/** Puts the member's unworn weapon or adapter in the chest. Empty on success; else the reason. Gear in any of
	 *  the spawned member's loadouts is refused (a worn piece stays worn; take it off first). */
	WORLD_OF_REFRACTION_API FText TryPutInChest(UWorld *World, FName FromKey, const FGuid &GearID);

	/** Takes a piece from the chest: it becomes ToKey's (its duplicate choice fires if it owns a copy, D-115).
	 *  Empty on success; else the reason. */
	WORLD_OF_REFRACTION_API FText TryTakeFromChest(UWorld *World, FName ToKey, const FGuid &GearID);

	/** Debug: what FromKey could give (its weapons, adapters and pouch stacks), one line each. */
	WORLD_OF_REFRACTION_API FString GetGiveableString(const UWorld *World, FName FromKey);
}
