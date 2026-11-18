// GiveTestWeaponsDebug.h
// Testing aid for the grip and shield sockets (D-242): a small set of weapons, owned, to equip from the loadout screen.
// wor.Debug.GiveTestWeapons
//   gives the played character COUNT_EACH of each weapon in TEST_WEAPON_PATHS through AddWeapon, as a vendor buy or a
//   pickup adds one: home gear, so it is refused while a run is open (there it would be run-only). No duplicate
//   prompt opens. To test another weapon, add its path to the list.

#pragma once

#include "CoreMinimal.h"

class UInventoryComponent;
class UWeaponData;
class UWorld;

namespace WoRGiveTestWeapons
{
	constexpr int32 COUNT_EACH = 2;

	// TODO: guns and a bow join when their assets exist.
	inline const TCHAR *const TEST_WEAPON_PATHS[] = {
		TEXT("/Game/Data/Pool/Generic/Weapons/Sword/DA_Weapon_Sword.DA_Weapon_Sword"),
		TEXT("/Game/Data/Pool/Generic/Weapons/Sword/DA_Weapon_Shield.DA_Weapon_Shield"),
		TEXT("/Game/Data/Pool/Generic/Weapons/Axe/DA_Weapon_Axe_Hand.DA_Weapon_Axe_Hand"),
		TEXT("/Game/Data/Pool/Generic/Weapons/Axe/DA_Weapon_Axe_Bearded.DA_Weapon_Axe_Bearded"),
	};

	/** Adds Count of each weapon to Inventory with no duplicate notice; one line per weapon in OutLines (a null one,
	 *  or one the inventory has no room for, says so). Returns how many pieces were added. */
	WORLD_OF_REFRACTION_API int32 GiveWeapons(UInventoryComponent &Inventory, const TArray<UWeaponData *> &Weapons, int32 Count, TArray<FString> &OutLines);

	/** Runs wor.Debug.GiveTestWeapons on the played character; returns the total line, or why nothing was given. */
	WORLD_OF_REFRACTION_API FString GiveTestWeapons(UWorld *World);
}
