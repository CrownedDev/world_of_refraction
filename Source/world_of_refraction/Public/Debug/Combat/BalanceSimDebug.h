// BalanceSimDebug.h
// B-293: run the balance simulator from the console and inspect its report.
//   wor.Balance.Run <preset> [fights] [seed] [gas]: plays the preset (quick / nightly / full) with fights per matchup
//   (default: the preset's), seeds from seed (default 29300), on the enum effect path or GAS (gas = 1), writes
//   Saved/Balance/<preset>_<stamp>.{csv,json,md} and logs the report. Editor and test builds only.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BalanceSimDebug
{
	/** What wor.Balance.Run does; returns the report text (empty when the preset is unknown or the runner won't start). */
	FString Run(FName Preset, int32 Fights, int32 BaseSeed, bool bUseGAS);
}

#endif // WITH_DEV_AUTOMATION_TESTS
