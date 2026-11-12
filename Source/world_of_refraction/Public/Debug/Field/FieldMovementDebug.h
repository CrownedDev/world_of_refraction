// FieldMovementDebug.h
// B-214a (D-099): inspect and drive field movement without the input assets. Console: wor.Field.Print,
// wor.Field.Activate (stands in for double-tap sprint until B-214g), wor.Field.Dash, and the placeholder until the
// picker (B-214b) wor.Field.Set <Innate|Weapon|Ring> <Dash|EnhancedSprint|...>.

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace WoRFieldMovementDebug
{
	/** Logs UFieldMovementComponent::GetFieldStateString for every field pawn in World and shows it on screen. */
	WORLD_OF_REFRACTION_API void PrintFieldState(UWorld *World);
}
