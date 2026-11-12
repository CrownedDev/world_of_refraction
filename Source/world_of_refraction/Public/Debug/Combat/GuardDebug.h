// GuardDebug.h
// B-392 (D-170): inspect and set the two guards without a fight. Console: wor.Guard.Print, and the placeholder until the
// loadout screen has it, wor.Guard.Set <Weapon|Unarmed> [name part]. B-393: wor.Guard.Print also shows what a landed
// parry plays (ParryVisuals); placeholder wor.Guard.SetParryPriority <Weapon|Magic> [name part].

#pragma once

#include "CoreMinimal.h"

class ULoadoutComponent;
class UWorld;

namespace WoRGuardDebug
{
	/** "Hero_1: guard Unarmed | weapon Sword | montage AM_Block_Body | record yes" and, on a second line, the landed
	 *  parry's inputs and pick (a normal Guard): the resolved guard, the weapon in hand, the montage a Guard plays,
	 *  whether the choice lives on a party record (saved), and the spark / parry effect / element colour (B-393). */
	WORLD_OF_REFRACTION_API FString GetGuardString(const ULoadoutComponent *Loadout);

	/** Logs GetGuardString for every loadout in World and shows it on screen. */
	WORLD_OF_REFRACTION_API void PrintGuardState(UWorld *World);
}
