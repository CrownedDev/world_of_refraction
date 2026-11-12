// BalanceSimDebug.cpp

#include "Debug/Combat/BalanceSimDebug.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/IConsoleManager.h"
#include "Balance/BalanceArchetypes.h"
#include "Balance/BalanceSimReport.h"
#include "Balance/BalanceSimRunner.h"
#include "WoRLog.h"

namespace
{
	constexpr int32 BALANCE_RUN_DEFAULT_SEED = 29300;
	/** Argument positions of wor.Balance.Run. */
	constexpr int32 ARG_PRESET = 0;
	constexpr int32 ARG_FIGHTS = 1;
	constexpr int32 ARG_SEED = 2;

	FAutoConsoleCommand GBalanceRunCmd(
		TEXT("wor.Balance.Run"),
		TEXT("DEBUG: wor.Balance.Run <quick|nightly|full> [fights] [seed]: play the preset, write Saved/Balance/<preset>_<stamp>.{csv,json,md}, log the report (B-293)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString> &Args)
		{
			if (!Args.IsValidIndex(ARG_PRESET))
			{
				UE_LOG(LogWoR, Warning, TEXT("[BalanceSim] usage: wor.Balance.Run <quick|nightly|full> [fights] [seed]"));
				return;
			}
			const FName Preset(*Args[ARG_PRESET]);
			const int32 Fights = Args.IsValidIndex(ARG_FIGHTS) ? FCString::Atoi(*Args[ARG_FIGHTS]) : BalanceArchetypes::PresetFights(Preset);
			const int32 Seed = Args.IsValidIndex(ARG_SEED) ? FCString::Atoi(*Args[ARG_SEED]) : BALANCE_RUN_DEFAULT_SEED;
			BalanceSimDebug::Run(Preset, Fights, Seed);
		}));
}

namespace BalanceSimDebug
{
	FString Run(FName Preset, int32 Fights, int32 BaseSeed)
	{
		const TArray<FBalanceMatchup> Matchups = BalanceArchetypes::Preset(Preset);
		if (Matchups.IsEmpty() || Fights <= 0)
		{
			UE_LOG(LogWoR, Warning, TEXT("[BalanceSim] unknown preset '%s' or no fights (presets: quick, nightly, full)"), *Preset.ToString());
			return FString();
		}
		FBalanceSimRunner Runner;
		if (!Runner.Begin())
		{
			UE_LOG(LogWoR, Error, TEXT("[BalanceSim] the runner couldn't start"));
			return FString();
		}
		UE_LOG(LogWoR, Display, TEXT("[BalanceSim] %s: %d matchup(s) x %d fight(s), seeds from %d"), *Preset.ToString(), Matchups.Num(), Fights,
			   BaseSeed);
		TArray<FBalanceFightRecord> Records;
		for (const FBalanceMatchup &Matchup : Matchups)
		{
			Records.Append(Runner.RunMatchup(Matchup, Fights, BaseSeed));
		}
		Runner.End();
		TArray<FBalanceAggregate> Aggregates;
		TArray<FBalanceTargetResult> Results;
		const TArray<FString> Paths = BalanceSimReport::WriteReport(Preset, Matchups, Records, Aggregates, Results);
		const FString Report = BalanceSimReport::GetReportString(Aggregates, Results);
		UE_LOG(LogWoR, Display, TEXT("%s"), *Report);
		for (const FString &Path : Paths)
		{
			UE_LOG(LogWoR, Display, TEXT("[BalanceSim] wrote %s"), *Path);
		}
		return Report;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
