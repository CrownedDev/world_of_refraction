// WeaponSetRulesDebug.h
// B-477: inspect a Generic's weapon sets without PIE paths: wor.Loadout.PrintWeaponSets prints, per character, each
// set's hands, whether the pair is allowed (and why not), its skill-slot count (D-211) and its stance.

#pragma once

#include "CoreMinimal.h"

struct FWeaponLoadoutEntry;
class UWorld;

namespace WeaponSetRulesDebug
{
	/** "Sword (B) + Sword (C): pair OK, 4 tier slots, 6 skill slots, stance DualSwords". */
	WORLD_OF_REFRACTION_API FString GetWeaponSetString(const FWeaponLoadoutEntry &Set);

	/** Logs and shows on screen each character's two sets (the active loadout). */
	WORLD_OF_REFRACTION_API void PrintWeaponSets(UWorld *World);
}
