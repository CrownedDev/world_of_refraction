// BalanceTargetsSpec.cpp
// B-293 (docs/Design/BalanceSimulator.md §3): the balance targets and the GAS parity check.
// - WoR.Balance.Sim.Targets runs a preset (quick by default; -WoRBalance=nightly or full on the command line, see
//   Scripts/ue/test.sh), writes Saved/Balance/<preset>_<stamp>.{csv,json,md} and checks every target: on quick only
//   stalls are errors (3 fights can't carry a band); on nightly / full the error targets fail the test and the warn
//   targets are reported.
// - WoR.Balance.Sim.GASParity plays the same seed on the enum effect path and on GAS and expects the same record.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Balance/BalanceArchetypes.h"
#include "Balance/BalanceSimReport.h"
#include "Balance/BalanceSimRunner.h"

namespace
{
	const FName BALANCE_PRESET_QUICK(TEXT("quick"));
	const TCHAR *BALANCE_PRESET_SWITCH = TEXT("WoRBalance=");
	constexpr int32 BALANCE_TARGETS_BASE_SEED = 29300;
	constexpr int32 BALANCE_PARITY_BASE_SEED = 29350;
	/** Seeds per matchup the parity check replays on both paths. */
	constexpr int32 BALANCE_PARITY_SEEDS = 2;
	/** A run writes a .csv, a .json and a .md. */
	constexpr int32 BALANCE_REPORT_FILES = 3;

	/** The preset named on the command line, else quick. */
	FName BalancePresetFromCommandLine()
	{
		FString Named;
		if (FParse::Value(FCommandLine::Get(), BALANCE_PRESET_SWITCH, Named) && BalanceArchetypes::PresetNames().Contains(FName(*Named)))
		{
			return FName(*Named);
		}
		return BALANCE_PRESET_QUICK;
	}
}

BEGIN_DEFINE_SPEC(FBalanceTargetsSpec, "WoR.Balance.Sim.Targets",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FBalanceSimRunner> Runner;
END_DEFINE_SPEC(FBalanceTargetsSpec)

void FBalanceTargetsSpec::Define()
{
	BeforeEach([this]()
	{
		Runner = MakeUnique<FBalanceSimRunner>();
		TestTrue(TEXT("runner started"), Runner->Begin());
	});

	AfterEach([this]()
	{
		Runner.Reset();
	});

	It("runs the preset, writes the report and checks every target", [this]()
	{
		const FName Preset = BalancePresetFromCommandLine();
		const TArray<FBalanceMatchup> Matchups = BalanceArchetypes::Preset(Preset);
		const int32 Fights = BalanceArchetypes::PresetFights(Preset);
		if (!TestTrue(Preset.ToString() + TEXT(" has matchups"), Matchups.Num() > 0 && Fights > 0)) return;
		AddInfo(FString::Printf(TEXT("preset %s: %d matchup(s) x %d fight(s)"), *Preset.ToString(), Matchups.Num(), Fights));

		TArray<FBalanceFightRecord> Records;
		for (const FBalanceMatchup &Matchup : Matchups)
		{
			Records.Append(Runner->RunMatchup(Matchup, Fights, BALANCE_TARGETS_BASE_SEED));
		}
		// B-293: the Resonator and Caster archetypes carry a spell; the AI must actually cast it.
		int32 SpellCasts = 0;
		for (const FBalanceFightRecord &Record : Records)
		{
			SpellCasts += Record.SpellCasts;
		}
		AddInfo(FString::Printf(TEXT("spells cast: %d"), SpellCasts));
		TestTrue(TEXT("the spell archetypes cast their spell"), SpellCasts > 0);

		TArray<FBalanceAggregate> Aggregates;
		TArray<FBalanceTargetResult> Results;
		const TArray<FString> Paths = BalanceSimReport::WriteReport(Preset, Matchups, Records, Aggregates, Results);
		TestEqual(TEXT("three report files written"), Paths.Num(), BALANCE_REPORT_FILES);
		for (const FString &Path : Paths)
		{
			AddInfo(TEXT("wrote ") + Path);
		}
		AddInfo(BalanceSimReport::GetReportString(Aggregates, Results));

		const bool bBandsAreErrors = Preset != BALANCE_PRESET_QUICK;
		for (const FBalanceTargetResult &Result : Results)
		{
			if (Result.Passed())
			{
				continue;
			}
			const bool bStall = Result.Target.Contains(TEXT("stalled"));
			if (bStall || (Result.bError && bBandsAreErrors))
			{
				AddError(Result.ToString());
			}
			else
			{
				AddWarning(Result.ToString());
			}
		}
	});
}

BEGIN_DEFINE_SPEC(FBalanceGASParitySpec, "WoR.Balance.Sim.GASParity",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FBalanceSimRunner> Runner;
END_DEFINE_SPEC(FBalanceGASParitySpec)

void FBalanceGASParitySpec::Define()
{
	BeforeEach([this]()
	{
		Runner = MakeUnique<FBalanceSimRunner>();
		TestTrue(TEXT("runner started"), Runner->Begin());
	});

	AfterEach([this]()
	{
		Runner.Reset(); // End() restores wor.Effects.UseGAS with the other console variables
	});

	It("plays the same seed on the enum path and on GAS and gets the same record", [this]()
	{
		const FName Preset = BalancePresetFromCommandLine();
		const TArray<FBalanceMatchup> Matchups = BalanceArchetypes::Preset(Preset);
		if (!TestTrue(Preset.ToString() + TEXT(" has matchups"), Matchups.Num() > 0)) return;
		int32 Differences = 0;
		for (const FBalanceMatchup &Matchup : Matchups)
		{
			for (int32 Index = 0; Index < BALANCE_PARITY_SEEDS; ++Index)
			{
				const int32 Seed = BALANCE_PARITY_BASE_SEED + Index;
				Runner->SetUseGAS(false);
				const FBalanceFightRecord Enum = Runner->RunFight(Matchup, Seed);
				Runner->SetUseGAS(true);
				const FBalanceFightRecord GAS = Runner->RunFight(Matchup, Seed);
				TestTrue(TEXT("the GAS record says so"), GAS.bGAS);
				TestNotEqual(*Enum.ToString(), Enum.Outcome, EBalanceFightOutcome::Stalled);
				if (!(Enum == GAS))
				{
					++Differences;
					AddError(FString::Printf(TEXT("GAS parity broke on %s seed %d:\n  enum: %s\n  gas:  %s"), *Matchup.Id.ToString(), Seed,
											 *Enum.ToString(), *GAS.ToString()));
				}
			}
		}
		TestEqual(TEXT("differences"), Differences, 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
