// BalanceSimSpec.cpp
// B-292 (docs/Design/BalanceSimulator.md §5 clusters 5-7): the runner plays seeded all-AI fights to the end and records
// turns, rounds, hits-to-kill and the win rate; a seed replays exactly; the quick preset runs with no stall.
// Targets and tolerances are B-293.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Balance/BalanceArchetypes.h"
#include "Balance/BalanceSimRunner.h"

namespace
{
	constexpr int32 SIM_SPEC_FIGHTS = 100;
	constexpr int32 SIM_SPEC_QUICK_FIGHTS = 3;
	constexpr int32 SIM_SPEC_BASE_SEED = 29200;
	constexpr int32 SIM_SPEC_REPLAY_SEED = 29299;
	const FName SIM_SPEC_QUICK(TEXT("quick"));
}

BEGIN_DEFINE_SPEC(FBalanceSimSpec, "WoR.Balance.Sim.Runner",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FBalanceSimRunner> Runner;
	TArray<FBalanceMatchup> Quick;
END_DEFINE_SPEC(FBalanceSimSpec)

void FBalanceSimSpec::Define()
{
	BeforeEach([this]()
	{
		Runner = MakeUnique<FBalanceSimRunner>();
		TestTrue(TEXT("runner started"), Runner->Begin());
		Quick = BalanceArchetypes::Preset(SIM_SPEC_QUICK);
	});

	AfterEach([this]()
	{
		Runner.Reset(); // End() restores the console variables and log levels
	});

	It("has an archetype for every party member and enemy of every preset", [this]()
	{
		for (const FName Preset : {FName(TEXT("quick")), FName(TEXT("nightly"))})
		{
			const TArray<FBalanceMatchup> Matchups = BalanceArchetypes::Preset(Preset);
			TestTrue(Preset.ToString() + TEXT(" has matchups"), Matchups.Num() > 0);
			for (const FBalanceMatchup &Matchup : Matchups)
			{
				TestNotNull(Matchup.Id.ToString() + TEXT(" enemy"), BalanceArchetypes::Find(Matchup.Enemy));
				for (const FName Member : Matchup.Party)
				{
					TestNotNull(Matchup.Id.ToString() + TEXT(" ") + Member.ToString(), BalanceArchetypes::Find(Member));
				}
			}
		}
	});

	It("runs 100 seeded fights and records turns, rounds, hits-to-kill and the win rate", [this]()
	{
		if (!TestTrue(TEXT("quick preset"), Quick.Num() > 0)) return;
		const TArray<FBalanceFightRecord> Records = Runner->RunMatchup(Quick[0], SIM_SPEC_FIGHTS, SIM_SPEC_BASE_SEED);
		const FBalanceAggregate Aggregate = FBalanceAggregate::From(Records);
		AddInfo(Aggregate.ToString());
		TestEqual(TEXT("fights run"), Aggregate.Fights, SIM_SPEC_FIGHTS);
		TestEqual(TEXT("no fight stalled"), Aggregate.Stalled, 0);
		for (const FBalanceFightRecord &Record : Records)
		{
			if (Record.Outcome == EBalanceFightOutcome::Stalled || Record.Turns <= 0 || Record.Rounds <= 0 || Record.HitsToKill.Num() == 0)
			{
				AddError(TEXT("incomplete record: ") + Record.ToString());
				break;
			}
		}
		TestTrue(TEXT("win rate is a share"), Aggregate.PartyWinRate >= 0.0f && Aggregate.PartyWinRate <= 1.0f);
		TestTrue(TEXT("hits to kill counted"), Aggregate.MeanHitsToKill > 0.0f);
	});

	It("replays a seed exactly", [this]()
	{
		if (!TestTrue(TEXT("quick preset"), Quick.Num() > 0)) return;
		const FBalanceFightRecord First = Runner->RunFight(Quick[0], SIM_SPEC_REPLAY_SEED);
		const FBalanceFightRecord Second = Runner->RunFight(Quick[0], SIM_SPEC_REPLAY_SEED);
		if (!TestTrue(TEXT("same seed, same fight"), First == Second))
		{
			AddInfo(TEXT("first:  ") + First.ToString());
			AddInfo(TEXT("second: ") + Second.ToString());
		}
		TestNotEqual(TEXT("it finished"), First.Outcome, EBalanceFightOutcome::Stalled);
	});

	It("runs the quick preset with no stall", [this]()
	{
		for (const FBalanceMatchup &Matchup : Quick)
		{
			for (const FBalanceFightRecord &Record : Runner->RunMatchup(Matchup, SIM_SPEC_QUICK_FIGHTS, SIM_SPEC_BASE_SEED))
			{
				TestNotEqual(Record.ToString(), Record.Outcome, EBalanceFightOutcome::Stalled);
			}
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
