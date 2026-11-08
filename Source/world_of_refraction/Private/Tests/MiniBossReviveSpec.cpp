// MiniBossReviveSpec.cpp
// B-187b part 1 (RunStructure §4, Q-187b): a beaten mini-boss's place is a rest spot whose menu can revive it, one
// tier stronger and with one tier better drops per revival, up to MiniBossRevival::MAX_REVIVALS; other beaten
// encounters can't be revived.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRestSpot.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 REVIVE_TEST_SEED = 11;
	const FName REVIVE_TEST_MINI_BOSS(TEXT("MiniBossTeam"));
	const FName REVIVE_TEST_ELITE(TEXT("EliteTeam"));
}

BEGIN_DEFINE_SPEC(FMiniBossReviveSpec, "WoR.Trial.MiniBossRevive", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTrialRunSubsystem *Run = nullptr;
END_DEFINE_SPEC(FMiniBossReviveSpec)

void FMiniBossReviveSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (Run)
		{
			Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), REVIVE_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
		}
	});

	AfterEach([this]()
	{
		Run = nullptr;
		TestWorld.Reset();
	});

	It("a beaten mini-boss can be revived up to the cap; an elite can't", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		Run->MarkEncounterBeaten(REVIVE_TEST_MINI_BOSS, EEncounterRank::MiniBoss);
		Run->MarkEncounterBeaten(REVIVE_TEST_ELITE, EEncounterRank::Elite);
		TestTrue(TEXT("a mini-boss's rest spot"), Run->IsBeatenMiniBoss(REVIVE_TEST_MINI_BOSS));
		TestFalse(TEXT("not an elite's"), Run->IsBeatenMiniBoss(REVIVE_TEST_ELITE));
		TestFalse(TEXT("an elite can't be revived"), Run->CanReviveEncounter(REVIVE_TEST_ELITE));
		for (int32 Revival = 1; Revival <= MiniBossRevival::MAX_REVIVALS; ++Revival)
		{
			TestTrue(FString::Printf(TEXT("revival %d"), Revival), Run->ReviveEncounter(REVIVE_TEST_MINI_BOSS));
			TestFalse(TEXT("back on the map"), Run->IsEncounterBeaten(REVIVE_TEST_MINI_BOSS));
			TestFalse(TEXT("not beaten: nothing to revive"), Run->CanReviveEncounter(REVIVE_TEST_MINI_BOSS));
			Run->MarkEncounterBeaten(REVIVE_TEST_MINI_BOSS, EEncounterRank::MiniBoss); // beaten again
			TestEqual(TEXT("counted"), Run->GetEncounterRevivals(REVIVE_TEST_MINI_BOSS), Revival);
		}
		TestFalse(TEXT("at the cap"), Run->CanReviveEncounter(REVIVE_TEST_MINI_BOSS));
		TestFalse(TEXT("refused"), Run->ReviveEncounter(REVIVE_TEST_MINI_BOSS));
		TestTrue(TEXT("debug lists it"), Run->GetRevivalsString().Contains(REVIVE_TEST_MINI_BOSS.ToString()));
	});

	It("the rest menu offers it only at that mini-boss's rest spot", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		Run->MarkEncounterBeaten(REVIVE_TEST_MINI_BOSS, EEncounterRank::MiniBoss);
		WoRRestSpot::Rest(TestWorld->GetWorld(), TEXT("SomeOtherSpot"));
		TestFalse(TEXT("elsewhere: no revive"), WoRRestSpot::CanReviveHere(TestWorld->GetWorld()));
		WoRRestSpot::Rest(TestWorld->GetWorld(), REVIVE_TEST_MINI_BOSS);
		TestTrue(TEXT("at its spot"), WoRRestSpot::CanReviveHere(TestWorld->GetWorld()));
	});

	It("a revival steps the fight's tier, at most S", [this]()
	{
		TestEqual(TEXT("one step"), TrialGate::StepTier(EItemTier::C_Tier, MiniBossRevival::TIER_STEPS_PER_REVIVAL), EItemTier::B_Tier);
		TestEqual(TEXT("no steps"), TrialGate::StepTier(EItemTier::C_Tier, 0), EItemTier::C_Tier);
		TestEqual(TEXT("capped at S"), TrialGate::StepTier(EItemTier::A_Tier, MiniBossRevival::MAX_REVIVALS), EItemTier::S_Tier);
		if (!TestNotNull(TEXT("Run"), Run)) return;
		Run->SetPendingTierSteps(2);
		TestEqual(TEXT("pending steps"), Run->GetPendingTierSteps(), 2);
		Run->SetPendingTierSteps(-1);
		TestEqual(TEXT("never negative"), Run->GetPendingTierSteps(), 0);
	});

	It("a new run forgets the mini-bosses and revivals", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		Run->MarkEncounterBeaten(REVIVE_TEST_MINI_BOSS, EEncounterRank::MiniBoss);
		Run->ReviveEncounter(REVIVE_TEST_MINI_BOSS);
		Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), REVIVE_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
		TestEqual(TEXT("no revivals"), Run->GetEncounterRevivals(REVIVE_TEST_MINI_BOSS), 0);
		TestFalse(TEXT("no mini-boss spot"), Run->IsBeatenMiniBoss(REVIVE_TEST_MINI_BOSS));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
