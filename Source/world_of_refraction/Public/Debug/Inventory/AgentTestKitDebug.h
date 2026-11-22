// AgentTestKitDebug.h
// B-573 (D-251): test setup for agent play. Everything is found by its kind (the asset registry), so the caller needs
// no asset path, and everything goes in through the real API. Hub only, host only. Not in a shipping build.
// wor.Debug.GiveTestKit [apparel] [adapters] [evolution] [skill] [member] [weapons] [potions]
//   no argument: all of them.
//   apparel    the played character owns apparel pieces up to APPAREL_PIECES (copies, to fill a grid past one screen).
//   adapters   its adapter room is filled (AddRing until refused).
//   evolution  it owns EVOLUTIONS evolution items, or as many as it has room for.
//   skill      it learns the SKILLS abilities with the longest asset names it doesn't know (to show a long name).
//   member     a second party member: a recruitable character standing in the level, else another CharacterData in
//              the played character's body.
//   weapons    wor.Debug.GiveTestWeapons, when the character owns fewer than its set.
//   potions    it carries POTIONS_OF_EACH Health Potions and Energy Potions (B-579, D-253; no merchant stocks them yet).

#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

class UInventoryComponent;
class UPoolSubsystem;
class URingData;
class UWorld;

namespace WoRAgentTestKit
{
	constexpr int32 APPAREL_PIECES = 60;
	constexpr int32 EVOLUTIONS = 2;
	constexpr int32 SKILLS = 3;
	constexpr int32 PARTY_MEMBERS = 2;
	constexpr int32 ADAPTERS = 20; // the kit's own count: the game has no adapter limit (D-250)
	constexpr int32 POTIONS_OF_EACH = 5; // B-579: Health and Energy Potions the played character is brought up to

	/** B-579: brings CharacterKey's pouch up to POTIONS_OF_EACH of each potion (the "potions" part). One line. */
	WORLD_OF_REFRACTION_API FString GivePotions(UPoolSubsystem &Pool, FName CharacterKey);

	/** Adds the adapters in turn until the inventory owns ADAPTERS of them or refuses every one. Returns how many
	 *  were added. */
	WORLD_OF_REFRACTION_API int32 GiveRings(UInventoryComponent &Inventory, const TArray<URingData *> &Rings);

	/** Runs wor.Debug.GiveTestKit on the played character; Parts as the command's arguments (empty: all). Returns one
	 *  line per part, or why nothing was given. */
	WORLD_OF_REFRACTION_API FString GiveTestKit(UWorld *World, const TArray<FString> &Parts);
}

#endif // !UE_BUILD_SHIPPING
