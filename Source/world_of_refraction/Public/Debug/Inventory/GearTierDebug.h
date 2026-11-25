// GearTierDebug.h
// B-609a: wor.Debug.SetGearTier <name-part> <F|E|D|C|B|A|S> sets the instance tier of every owned weapon or adapter whose
// name contains the text, free. It writes where UEconomyService::LevelUpWeapon writes (the owned entry's Tier, then
// RefreshLoadoutsFromOwned, so every loadout slot and the party's shared pool follow), then saves.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"

class UInventoryComponent;

namespace WoRGearTier
{
	/** Sets Tier on every owned weapon and ring of Inventory whose asset name contains NamePart (any case). Returns one
	 *  line per piece changed ("Sword: F -> S"); a piece already at Tier is left alone and not listed. */
	WORLD_OF_REFRACTION_API TArray<FString> SetOwnedGearTier(UInventoryComponent &Inventory, const FString &NamePart, EItemTier Tier);
}
