// GiveAllWeaponsDebug.h
// Testing aid for the grip and shield sockets (D-242): every weapon in the project, owned, to equip from the loadout screen.
// wor.Debug.GiveAllWeapons [count]
//   gives the played character count (default 2) of every UWeaponData asset (asset registry) through AddWeapon, as a
//   vendor buy or a pickup adds one: home gear, so it is refused while a run is open (there it would be run-only).
//   The weapon inventory's room (MAX_WEAPON_INVENTORY_SLOTS) is stepped over and no duplicate prompt opens.

#pragma once

#include "CoreMinimal.h"

class UInventoryComponent;
class UWeaponData;
class UWorld;

namespace WoRGiveAllWeapons
{
	constexpr int32 DEFAULT_COUNT = 2;
	constexpr int32 MAX_COUNT = 5;

	/** Adds Count of each weapon to Inventory, past its room and with no duplicate notice; one line per weapon in
	 *  OutLines (a null or refused one says so). Returns how many pieces were added. */
	WORLD_OF_REFRACTION_API int32 GiveWeapons(UInventoryComponent &Inventory, const TArray<UWeaponData *> &Weapons, int32 Count, TArray<FString> &OutLines);

	/** Runs wor.Debug.GiveAllWeapons on the played character; returns the total line, or why nothing was given. */
	WORLD_OF_REFRACTION_API FString GiveAllWeapons(UWorld *World, int32 Count);
}
