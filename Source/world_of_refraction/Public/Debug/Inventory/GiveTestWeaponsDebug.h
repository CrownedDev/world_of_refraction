// GiveTestWeaponsDebug.h
// Testing aid for the grip and shield sockets (D-242): a small set of weapons, owned, to equip from the loadout screen.
// wor.Debug.GiveTestWeapons
//   gives the played character each weapon in TEST_WEAPONS, in its count, through AddWeapon, as a vendor buy or a
//   pickup adds one: home gear, so it is refused while a run is open (there it would be run-only). No duplicate
//   prompt opens. To test another weapon, add its path and count to the list.

#pragma once

#include "CoreMinimal.h"

class UInventoryComponent;
class UWeaponData;
class UWorld;

namespace WoRGiveTestWeapons
{
	struct FTestWeapon
	{
		const TCHAR *Path;
		int32 Count;
	};

	/** Crown's test set (2026-11-18): one of each. */
	inline const FTestWeapon TEST_WEAPONS[] = {
		{TEXT("/Game/Data/Pool/Generic/Weapons/Sword/DA_Weapon_SimpleSword.DA_Weapon_SimpleSword"), 1},
		{TEXT("/Game/Data/Pool/Generic/Weapons/Sword/DA_Weapon_ShortSword.DA_Weapon_ShortSword"), 1},
		{TEXT("/Game/Data/Pool/Generic/Weapons/Greatsword/DA_Weapon_Claymore.DA_Weapon_Claymore"), 1},
		{TEXT("/Game/Data/Pool/Generic/Weapons/Greatsword/DA_Weapon_Longsword.DA_Weapon_Longsword"), 1},
		{TEXT("/Game/Data/Pool/Generic/Weapons/Axe/DA_Weapon_KnightAxe.DA_Weapon_KnightAxe"), 1},
		{TEXT("/Game/Data/Pool/Generic/Weapons/Axe/DA_Weapon_WarAxe.DA_Weapon_WarAxe"), 1},
		{TEXT("/Game/Data/Pool/Generic/Weapons/Shield/DA_Weapon_PlainShield.DA_Weapon_PlainShield"), 1},
		{TEXT("/Game/Data/Pool/Generic/Weapons/Shield/DA_Weapon_GuardShield.DA_Weapon_GuardShield"), 1},
		{TEXT("/Game/Data/Pool/Generic/Weapons/Bow/DA_Weapon_FalconerBow.DA_Weapon_FalconerBow"), 1},
		{TEXT("/Game/Data/Pool/Generic/Weapons/Bow/DA_Weapon_MechanicalBow.DA_Weapon_MechanicalBow"), 1},
		{TEXT("/Game/Data/Pool/Generic/Weapons/Gun/DA_Weapon_HunterGun.DA_Weapon_HunterGun"), 1},
		{TEXT("/Game/Data/Pool/Generic/Weapons/Gun/DA_Weapon_HunterGun2.DA_Weapon_HunterGun2"), 1},
	};

	/** Adds Count of each weapon to Inventory with no duplicate notice; one line per weapon in OutLines (a null one,
	 *  or one the inventory has no room for, says so). Returns how many pieces were added. */
	WORLD_OF_REFRACTION_API int32 GiveWeapons(UInventoryComponent &Inventory, const TArray<UWeaponData *> &Weapons, int32 Count, TArray<FString> &OutLines);

	/** Runs wor.Debug.GiveTestWeapons on the played character; returns the total line, or why nothing was given. */
	WORLD_OF_REFRACTION_API FString GiveTestWeapons(UWorld *World);
}
