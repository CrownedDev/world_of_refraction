// WeaponFollowsSkillDebug.h
// B-474 (D-207): inspect "Switch to the weapon of the skill you use" without the settings screen.
// wor.Settings.SwitchToSkillWeapon [0|1] prints or sets the setting (saved with the next save);
// wor.Loadout.PrintOtherWeaponSkills prints, per character, the weapon in hand and the other weapon's skills.

#pragma once

#include "CoreMinimal.h"

class ULoadoutComponent;
class UWorld;

namespace WeaponFollowsSkillDebug
{
	/** "Sword in hand; other weapon Hammer: Smash, Quake" or "no other weapon". */
	WORLD_OF_REFRACTION_API FString GetOtherWeaponSkillsString(const ULoadoutComponent &Loadout);

	/** Logs and shows on screen each character's line. */
	WORLD_OF_REFRACTION_API void PrintOtherWeaponSkills(UWorld *World);
}
