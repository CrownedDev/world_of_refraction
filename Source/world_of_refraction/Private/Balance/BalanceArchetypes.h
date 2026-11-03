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

	/** "quick" (3 matchups, runs in the normal suite) or "nightly" (the full grid). Empty for an unknown name. */
	TArray<FBalanceMatchup> Preset(FName Name);
}
