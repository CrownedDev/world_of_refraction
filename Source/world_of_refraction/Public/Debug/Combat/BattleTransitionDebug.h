// BattleTransitionDebug.h
// B-500 (D-212): see and inspect the battle transition without a fight.
// Console: wor.Transition.Play [hold seconds] plays it on the current map (cracks from the screen centre,
// holds, then blows away in place); wor.Transition.State logs where a running one is and the values it uses.

#pragma once

#include "CoreMinimal.h"

class UBattleTransitionData;
class UBattleTransitionSubsystem;
class UWorld;

namespace WoRBattleTransitionDebug
{
	/** "Playing: 0.62s (blow-away 0.00s), 168 shards, frame captured, no picture". */
	WORLD_OF_REFRACTION_API FString GetStateString(const UBattleTransitionSubsystem *Transition);

	/** The timings and the main look values on one line. */
	WORLD_OF_REFRACTION_API FString GetDataString(const UBattleTransitionData &Data);

	/** Plays the transition in World with no fight: holds Args[0] seconds from contact (default: a join window). */
	WORLD_OF_REFRACTION_API void PlayPreview(const TArray<FString> &Args, UWorld *World);

	/** Logs GetStateString and GetDataString. */
	WORLD_OF_REFRACTION_API void PrintState(UWorld *World);
}
