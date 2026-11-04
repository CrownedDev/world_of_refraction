// GearGive.h
// B-323 (D-117): giving between your own characters. A member gives an owned weapon or adapter, or a stack from
// its pouch, to another member: one instance moves (the pool's owner changes, UPoolSubsystem::GiveGear /
// GiveCarried). Worn gear comes off the giver's loadouts as it leaves; a loadout's primary is refused (D-116);
// the receiver's duplicate choice fires when it already owns one (D-115, UInventoryComponent::OnDuplicateGearReceived);
// a full pouch refuses. Evolutions are not given yet (Q-323).
// B-316b (Q-316, Q-316b): a learned spell or ability is given the same way (one copy moves) or requested from a
// teammate (an AI member, or one of the requesting player's own, says yes at once).
// B-361 (D-152, D-153): a run-only recruit trades skills like any member and carries the host's pouch; skills never
// change hands between two players' members (gear still does, for good).
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

	/** Gives Count of FromKey's carried Id to ToKey. Empty on success; else the reason, for the player. A run-only
	 *  recruit's pouch is the host's (B-361): a give between the two is refused as the same pouch. */
	WORLD_OF_REFRACTION_API FText TryGiveCarried(UWorld *World, FName FromKey, FName ToKey, const FCrystalId &Id, int32 Count);

	// ---- The shared chest (B-324, Q-321): gear the chest owns; any member (not a run-only recruit) takes from it ----

	/** Puts the member's unworn weapon or adapter in the chest. Empty on success; else the reason. Gear in any of
	 *  the spawned member's loadouts is refused (a worn piece stays worn; take it off first). */
	WORLD_OF_REFRACTION_API FText TryPutInChest(UWorld *World, FName FromKey, const FGuid &GearID);

	/** Takes a piece from the chest: it becomes ToKey's (its duplicate choice fires if it owns a copy, D-115).
	 *  Empty on success; else the reason. */
	WORLD_OF_REFRACTION_API FText TryTakeFromChest(UWorld *World, FName ToKey, const FGuid &GearID);

	// ---- Skills (B-316b, Q-316, Q-316b): each character's learned spells and abilities (UPoolSubsystem::FindSkills) ----
	/** Gives FromKey's learned spell or ability (Skill) to ToKey: one copy moves, the giver loses it (D-111). Empty on
	 *  success; else the reason. A skill in the giver's loadouts leaves them at their next check (the loadout rules clear
	 *  a skill the character doesn't know). Refused between two different players' members (D-153); a run-only
	 *  recruit or an AI member nobody claims is the host's (D-152). */
	WORLD_OF_REFRACTION_API FText TryGiveSkill(UWorld *World, FName FromKey, FName ToKey, const UObject *Skill);
	/** RequesterKey asks HolderKey for its skill: the same player's member (or an AI one) says yes at once
	 *  (TryGiveSkill); another player's member is refused (D-153). */
	WORLD_OF_REFRACTION_API FText TryRequestSkill(UWorld *World, FName RequesterKey, FName HolderKey, const UObject *Skill);
	/** FromKey's learned skills a give could move: its spells, then its abilities. Empty when its skills aren't kept. */
	WORLD_OF_REFRACTION_API TArray<const UObject *> GetGiveableSkills(const UWorld *World, FName FromKey);

	/** Debug: what FromKey could give (its weapons, adapters, pouch stacks and skills), one line each. */
	WORLD_OF_REFRACTION_API FString GetGiveableString(const UWorld *World, FName FromKey);
}
