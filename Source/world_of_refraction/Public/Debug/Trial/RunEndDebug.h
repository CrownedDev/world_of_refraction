// RunEndDebug.h
// B-510b: test every run ending without playing it out, through the real end path (FinishRun -> UPoolSubsystem::CloseRun).
// wor.Trial.EndRun <win|extract|wipes|quit>
//   win      the main boss counts as beaten (MarkRunBossBeaten), then the run ends as ExitTrial ends it: Kept.
//   extract  leaving at a rest spot (LeaveRunEarly): Kept.
//   wipes    wipes are counted (RecordWipe) until the run is out of them, then it ends as the battle's loss ends it: Lost.
//   quit     the game menu's Quit (AbandonRun): Quit.
// wor.Trial.GrantFoundWeapon [asset path]
//   the played character finds a weapon (AddWeapon, as trial loot does): a new piece, marked run-only. Run only.

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace WoRRunEndDebug
{
	/** Ends the open run the way How names (win, extract, wipes, quit); returns what was done or why not. The run
	 *  ends next tick, as in play. */
	WORLD_OF_REFRACTION_API FString EndRun(UWorld *World, const FString &How);

	/** The played character finds the weapon at AssetPath (empty: a plain sword); returns what was given or why not. */
	WORLD_OF_REFRACTION_API FString GrantFoundWeapon(UWorld *World, const FString &AssetPath);
}
