// RunEndEveryEndingSpec.cpp
// B-465c (D-187, D-189; UIScreens.md #27): every run end shows the run-end page in the hub. How the run is ending is
// read from the run (none without one or without the boss, the boss beaten, out of wipes ahead of the boss); the page
// lists the memories found, a quit's too though it pays nothing; the queue hands the page over once.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Trial/DefeatRules.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Trial/RewardNotices.h"
#include "UI/Trial/RunEndModel.h"
#include "WoRTestWorld.h"

namespace
{
	constexpr int32 RUN_END_EVERY_SEED = 2468;
	constexpr int32 RUN_END_EVERY_VARIATION = 0;
	constexpr int32 RUN_END_EVERY_GOLD = 900;
	constexpr int32 RUN_END_EVERY_MEMORIES = 2;
}

BEGIN_DEFINE_SPEC(FRunEndEveryEndingSpec, "WoR.UI.RunEndEvery", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FRunEndEveryEndingSpec)

void FRunEndEveryEndingSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("reads how the run is ending: none without the boss, the boss beaten, out of wipes first", [this]()
	{
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("Trial run"), TrialRun))
		{
			return;
		}
		ERunEnding Ending = ERunEnding::Quit;
		TestFalse(TEXT("no run, no ending"), TrialRun->GetRunEnding(Ending));
		TrialRun->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), RUN_END_EVERY_SEED, RUN_END_EVERY_VARIATION, ETrialDifficulty::Medium, {}, {}, {});
		TestFalse(TEXT("a run with no end yet: none"), TrialRun->GetRunEnding(Ending));
		TrialRun->MarkRunBossBeaten();
		TestTrue(TEXT("the boss beaten"), TrialRun->GetRunEnding(Ending) && Ending == ERunEnding::BossBeaten);
		TrialRun->RestoreRunLosses(DefeatRules::MAX_WIPES_PER_RUN, 0, NAME_None);
		TestTrue(TEXT("out of wipes"), TrialRun->GetRunEnding(Ending) && Ending == ERunEnding::OutOfWipes);
	});

	It("lists the memories found on the page, a quit's too, though it pays nothing (D-187, D-176)", [this]()
	{
		FRunPayout Payout;
		Payout.HubGold = RUN_END_EVERY_GOLD;
		const FString Quit = WoRRunEnd::BuildRunEnd(ERunEnding::Quit, Payout, RUN_END_EVERY_MEMORIES).PayoutLine.ToString();
		TestTrue(TEXT("the memories"), Quit.Contains(TEXT("2 memories")));
		TestFalse(TEXT("no payout"), Quit.Contains(FText::AsNumber(RUN_END_EVERY_GOLD).ToString()));
		const FString Cleared = WoRRunEnd::BuildRunEnd(ERunEnding::BossBeaten, Payout, 1).PayoutLine.ToString();
		TestTrue(TEXT("the payout and one memory"), Cleared.Contains(FText::AsNumber(RUN_END_EVERY_GOLD).ToString()) && Cleared.Contains(TEXT("1 memory")));
	});

	It("hands the page over once", [this]()
	{
		URewardNoticeSubsystem *Notices = TestWorld->GetSubsystem<URewardNoticeSubsystem>();
		if (!TestNotNull(TEXT("Notices"), Notices))
		{
			return;
		}
		TestFalse(TEXT("none at first"), Notices->HasRunEnd());
		Notices->SetRunEnd(WoRRunEnd::BuildRunEnd(ERunEnding::Extracted, FRunPayout()));
		const TOptional<FRunEndPage> Page = Notices->TakeRunEnd();
		TestTrue(TEXT("the extracted page"), Page.IsSet() && Page->Ending == ERunEnding::Extracted);
		TestFalse(TEXT("then none"), Notices->HasRunEnd() || Notices->TakeRunEnd().IsSet());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
