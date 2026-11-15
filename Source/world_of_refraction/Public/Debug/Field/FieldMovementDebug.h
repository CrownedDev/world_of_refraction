// FieldMovementDebug.h
// B-214a (D-099): inspect and drive field movement without the input assets. Console: wor.Field.Print,
// wor.Field.Activate (stands in for double-tap sprint until B-214g), wor.Field.Dash, and the placeholder until the
// picker (B-214b) wor.Field.Set <Innate|Weapon|Ring> <Dash|EnhancedSprint|...>. B-214b: wor.Field.Unlocks,
// wor.Field.Unlock <Type> <Element>. B-214d: wor.Field.FlightLimits <None|Unlimited|NoCeiling|IgnoreNoFly|All>.
// B-214f: wor.Field.Attack [Hold]. B-503: wor.Field.CycleAttack (was wor.Field.SwitchSource).

#pragma once

#include "CoreMinimal.h"

class UFieldMovementComponent;
class UWorld;

namespace WoRFieldMovementDebug
{
	/** B-503: every stop of the field attack's cycle, one per line, the one in use marked:
	 *  "> 1 Weapon 'Iron Sword' Fire Slash (swing)". */
	WORLD_OF_REFRACTION_API FString GetFieldAttackCycleString(const UFieldMovementComponent &FieldMovement);

	/** Logs UFieldMovementComponent::GetFieldStateString for every field pawn in World and shows it on screen. */
	WORLD_OF_REFRACTION_API void PrintFieldState(UWorld *World);

	/** B-214b: logs UFieldSkillUnlockSubsystem::GetUnlocksString (after a scan of the store) and shows it on screen. */
	WORLD_OF_REFRACTION_API void PrintUnlocks(UWorld *World);
}
