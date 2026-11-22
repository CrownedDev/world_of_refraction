// AgentTestKitDebug.h
// B-573 (D-251): test setup for agent play. Everything is found by its kind (the asset registry), so the caller needs
// no asset path, and everything goes in through the real API. Hub only, host only. Not in a shipping build.
// wor.Debug.GiveTestKit [apparel] [adapters] [evolution] [skill] [member] [weapons]
//   no argument: all of them.
//   apparel    the played character owns apparel pieces up to APPAREL_PIECES (copies, to fill a grid past one screen).
//   adapters   its adapter room is filled (AddRing until refused).
//   evolution  it owns EVOLUTIONS evolution items, or as many as it has room for.
//   skill      it learns the SKILLS abilities with the longest asset names it doesn't know (to show a long name).
//   member     a second party member: a recruitable character standing in the level, else another CharacterData in
//              the played character's body.
//   weapons    wor.Debug.GiveTestWeapons, when the character owns fewer than its set.

#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

class UInventoryComponent;
class URingData;
class UWorld;

namespace WoRAgentTestKit
{
	constexpr int32 APPAREL_PIECES = 60;
	constexpr int32 EVOLUTIONS = 2;
	constexpr int32 SKILLS = 3;
	constexpr int32 PARTY_MEMBERS = 2;

	/** Adds the adapters in turn until the inventory refuses every one of them (its room is full). Returns how many. */
	WORLD_OF_REFRACTION_API int32 GiveRings(UInventoryComponent &Inventory, const TArray<URingData *> &Rings);

	/** Runs wor.Debug.GiveTestKit on the played character; Parts as the command's arguments (empty: all). Returns one
	 *  line per part, or why nothing was given. */
	WORLD_OF_REFRACTION_API FString GiveTestKit(UWorld *World, const TArray<FString> &Parts);
}

#endif // !UE_BUILD_SHIPPING
