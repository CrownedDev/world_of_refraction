// GameInputDebug.h
// B-578: what the game viewport does with input right now, to tell "the game ignores input" from "the key did
// nothing". Not in a shipping build.
// wor.Input.PrintState   one line: whether the viewport ignores game input (a menu-only mode is on), how the mouse is
//                        captured and locked, whether the cursor shows, and whether keyboard focus is on the game.

#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

class UWorld;

namespace WoRGameInputDebug
{
	/** The line wor.Input.PrintState prints, for World's game viewport and first local player. */
	WORLD_OF_REFRACTION_API FString GetStateString(const UWorld *World);
}

#endif // !UE_BUILD_SHIPPING
