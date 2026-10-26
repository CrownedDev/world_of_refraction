// NativeUIDebug.h
// Debug utilities for the native (C++-owned) UI layer added by the BP-logic
// overhaul: UCombatHUD, UDefensePromptWidget, AExplorationPlayerController.
//
// Console: wor.UIState — prints all three to Output Log + screen.
// World of Refraction - Debug

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace NativeUIDebug
{
    /** Combat HUD (per orchestrator) + exploration-controller interaction state. */
    WORLD_OF_REFRACTION_API FString GetNativeUIStateString(UWorld *World);

    /** Log GetNativeUIStateString to LogTemp and the screen. */
    WORLD_OF_REFRACTION_API void PrintNativeUIState(UWorld *World);
}
