// BalanceArchetypes.h
// B-292 (docs/Design/BalanceSimulator.md §4): the archetype table and the matchup presets. Numbers come from
// docs/analysis/BaselineNumbers.md §3-4; pillars are clamped to the cap of 7 (B-222).

#pragma once

#include "CoreMinimal.h"
#include "Balance/BalanceSimTypes.h"

namespace BalanceArchetypes
{
	/** Every player and enemy archetype. */
	const TArray<FBalanceArchetype> &All();

	/** The archetype with Id, or null. */
	const FBalanceArchetype *Find(FName Id);

	/** "quick" (3 matchups, runs in the normal suite), "nightly" (the grid at Medium and Hard) or "full" (nightly plus
	 *  the Elite rank). Empty for an unknown name. */
	TArray<FBalanceMatchup> Preset(FName Name);

	/** Fights per matchup a preset runs: quick 3, nightly 50, full 200 (0 for an unknown name). */
	int32 PresetFights(FName Name);

	/** Every preset name, for the command and the spec. */
	const TArray<FName> &PresetNames();
}
