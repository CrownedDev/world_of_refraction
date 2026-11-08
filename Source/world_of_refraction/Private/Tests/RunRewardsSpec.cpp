// RunRewardsSpec.cpp
// B-073 (D-006, D-048, D-094, D-159, Q-073): a win pays run Prisms at once and adds to the run's tally; the tally
// (hub Gold, Gear / Skill Essence) is paid only at the run's end, scaled by difficulty, the boss bonus and the
// multiplier; nothing outside a run.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Currency/CurrencyComponent.h"
#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "Trial/RunRewards.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRTestWorld.h"

namespace
{
	constexpr int32 RUN_REWARDS_TEST_SEED = 7;
	constexpr float RUN_REWARDS_TEST_PASSIVE = 1.1f;
}

BEGIN_DEFINE_SPEC(FRunRewardsSpec, "WoR.Trial.RunRewards", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTrialRunSubsystem *Run = nullptr;
	UCurrencyComponent *Wallet = nullptr;
END_DEFINE_SPEC(FRunRewardsSpec)

void FRunRewardsSpec::Define()
{
	It("pays more for harder fights and harder trials", [this]()
	{
		TestTrue(TEXT("a boss pays more than a normal fight"),
				 RunRewards::WinRunPrisms(EEncounterRank::Boss, ETrialDifficulty::Medium) > RunRewards::WinRunPrisms(EEncounterRank::Normal, ETrialDifficulty::Medium));
		TestTrue(TEXT("Hard pays more than Easy"),
				 RunRewards::WinRunPrisms(EEncounterRank::Normal, ETrialDifficulty::Hard) > RunRewards::WinRunPrisms(EEncounterRank::Normal, ETrialDifficulty::Easy));
		TestTrue(TEXT("a mini-boss adds more essence than a normal fight"),
				 RunRewards::FightTally(EEncounterRank::MiniBoss).GearEssence > RunRewards::FightTally(EEncounterRank::Normal).GearEssence);
		TestTrue(TEXT("a chest adds more than a normal fight"), RunRewards::ChestTally().HubGold > RunRewards::FightTally(EEncounterRank::Normal).HubGold);
	});

	It("scales the tally at the end by difficulty, the boss bonus and the multiplier", [this]()
	{
		const FRunPayout Tally = RunRewards::FightTally(EEncounterRank::Boss);
		const FRunPayout Plain = RunRewards::ScalePayout(Tally, ETrialDifficulty::Medium, false, RunRewards::BASE_PAYOUT_MULTIPLIER);
		TestEqual(TEXT("Medium, no boss, x1: the tally"), Plain.HubGold, Tally.HubGold);
		TestTrue(TEXT("the boss beaten pays more"),
				 RunRewards::ScalePayout(Tally, ETrialDifficulty::Medium, true, RunRewards::BASE_PAYOUT_MULTIPLIER).HubGold > Plain.HubGold);
		TestTrue(TEXT("a passive's multiplier pays more"),
				 RunRewards::ScalePayout(Tally, ETrialDifficulty::Medium, false, RUN_REWARDS_TEST_PASSIVE).HubGold > Plain.HubGold);
		TestTrue(TEXT("Easy pays less"), RunRewards::ScalePayout(Tally, ETrialDifficulty::Easy, false, RunRewards::BASE_PAYOUT_MULTIPLIER).HubGold < Plain.HubGold);
	});

	Describe("in a run", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("world"), TestWorld->Create());
			UWorld *World = TestWorld->GetWorld();
			AWoRGameState *GameState = World ? World->SpawnActor<AWoRGameState>() : nullptr;
			AWoRPlayerState *Player = World ? World->SpawnActor<AWoRPlayerState>() : nullptr;
			if (GameState && Player)
			{
				World->SetGameState(GameState);
				GameState->AddPlayerState(Player);
				Wallet = Player->GetWallet();
			}
			Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		});
		AfterEach([this]()
		{
			Run = nullptr;
			Wallet = nullptr;
			TestWorld.Reset();
		});

		It("pays run Prisms for a win at once and banks nothing until the run ends", [this]()
		{
			if (!TestTrue(TEXT("setup"), Run && Wallet))
			{
				return;
			}
			Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), RUN_REWARDS_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
			const int32 HubBefore = Wallet->GetBalance(ECurrencyType::Prisms);
			Run->RecordFightReward(TestWorld->GetWorld(), EEncounterRank::Normal);
			TestEqual(TEXT("run Prisms at once"), Wallet->GetBalance(ECurrencyType::Gold),
					  RunRewards::WinRunPrisms(EEncounterRank::Normal, ETrialDifficulty::Medium));
			TestEqual(TEXT("no hub Gold mid-run"), Wallet->GetBalance(ECurrencyType::Prisms), HubBefore);
			TestFalse(TEXT("the tally grew"), Run->GetRunTally().IsEmpty());

			// The run's end (ExitTrial pays this, win or lose).
			const FRunPayout Payout = Run->GetRunEndPayout();
			TestEqual(TEXT("paid to the one player"), RunRewards::PayRunEndToPlayers(TestWorld->GetWorld(), Payout), 1);
			TestEqual(TEXT("hub Gold at the end"), Wallet->GetBalance(ECurrencyType::Prisms), HubBefore + Payout.HubGold);
			TestEqual(TEXT("Gear Essence at the end"), Wallet->GetBalance(ECurrencyType::GearEssence), Payout.GearEssence);
		});

		It("pays nothing for a fight outside a run", [this]()
		{
			if (!TestTrue(TEXT("setup"), Run && Wallet))
			{
				return;
			}
			Run->RecordFightReward(TestWorld->GetWorld(), EEncounterRank::Boss);
			TestEqual(TEXT("no run Prisms"), Wallet->GetBalance(ECurrencyType::Gold), 0);
			TestTrue(TEXT("no tally"), Run->GetRunTally().IsEmpty());
		});
	});
}

#endif
