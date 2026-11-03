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

	/** B-293b: the parity check's own matchup: a party whose every hit builds status (Bleed from Slash, Burn from the
	 *  Fire spell) against same-tier enemies, so the GAS and enum paths both apply break effects. Not in any preset,
	 *  so the balance numbers (B-293c) don't move. */
	FBalanceMatchup ParityStatusMatchup();

	/** Every preset name, for the command and the spec. */
	const TArray<FName> &PresetNames();
}
