// PauseMenuSpec.cpp
// B-439 (D-181, D-176): the game menu's model. Its eight pages in order, which ones open, the header in the hub
// and in a run, and Quit in a run: the run's end pays nothing. The screen itself is the PIE sheet (docs/PIE/B-439.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Trial/DefeatRules.h"
#include "Trial/RunRewards.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Menus/PauseMenuModel.h"
#include "WoRTestWorld.h"

namespace
{
	constexpr int32 PAUSE_TEST_SEED = 23;
	constexpr int32 PAUSE_TEST_PAGE_COUNT = 8;
	constexpr int32 PAUSE_TEST_CHIP_COUNT = 3;
}

BEGIN_DEFINE_SPEC(FPauseMenuSpec, "WoR.UI.PauseMenu", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTrialRunSubsystem *Run = nullptr;

	void StartRun()
	{
		Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), PAUSE_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
	}
END_DEFINE_SPEC(FPauseMenuSpec)

void FPauseMenuSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
	});
	AfterEach([this]()
	{
		Run = nullptr;
		TestWorld.Reset();
	});

	It("lists the eight pages in the mockup's order, Resume first and Quit last", [this]()
	{
		const TArray<FPauseMenuPage> Pages = WoRPauseMenu::BuildPages(false, true);
		if (!TestEqual(TEXT("eight pages"), Pages.Num(), PAUSE_TEST_PAGE_COUNT))
		{
			return;
		}
		for (int32 Index = 0; Index < Pages.Num(); ++Index)
		{
			TestEqual(TEXT("in enum order"), static_cast<int32>(Pages[Index].Page), Index);
			TestFalse(TEXT("named"), Pages[Index].Label.IsEmpty());
			TestFalse(TEXT("previewed"), Pages[Index].Preview.IsEmpty());
		}
		TestTrue(TEXT("Resume opens"), Pages[static_cast<int32>(EPauseMenuPage::Resume)].CanOpen());
		TestTrue(TEXT("Inventory opens (its placeholder)"), Pages[static_cast<int32>(EPauseMenuPage::Inventory)].CanOpen());
		TestTrue(TEXT("Quit opens"), Pages[static_cast<int32>(EPauseMenuPage::Quit)].CanOpen());
		TestFalse(TEXT("Loadout says why it can't open yet"), Pages[static_cast<int32>(EPauseMenuPage::Loadout)].CanOpen());
	});

	It("refuses Quit to a guest in a run, and only then", [this]()
	{
		const int32 Quit = static_cast<int32>(EPauseMenuPage::Quit);
		TestFalse(TEXT("guest in a run"), WoRPauseMenu::BuildPages(true, false)[Quit].CanOpen());
		TestTrue(TEXT("host in a run"), WoRPauseMenu::BuildPages(true, true)[Quit].CanOpen());
		TestTrue(TEXT("guest in the hub"), WoRPauseMenu::BuildPages(false, false)[Quit].CanOpen());
	});

	It("warns that quitting a run loses everything (D-176)", [this]()
	{
		const FText InRun = WoRPauseMenu::GetQuitWarning(true);
		TestFalse(TEXT("a warning in a run"), InRun.IsEmpty());
		TestFalse(TEXT("not the hub's line"), InRun.EqualTo(WoRPauseMenu::GetQuitWarning(false)));
		TestTrue(TEXT("Quit's preview is the warning"), WoRPauseMenu::BuildPages(true, true)[static_cast<int32>(EPauseMenuPage::Quit)].Preview.EqualTo(InRun));
	});

	It("shows the hub outside a run and the run's wipes left inside one", [this]()
	{
		if (!TestNotNull(TEXT("run"), Run))
		{
			return;
		}
		const FPauseMenuHeader Hub = WoRPauseMenu::BuildHeader(TestWorld->GetWorld());
		TestFalse(TEXT("hub: not in a run"), Hub.bInRun);
		TestEqual(TEXT("hub: three chips"), Hub.Chips.Num(), PAUSE_TEST_CHIP_COUNT);

		StartRun();
		Run->RecordWipe(EEncounterRank::Normal);
		const FPauseMenuHeader InRun = WoRPauseMenu::BuildHeader(TestWorld->GetWorld());
		TestTrue(TEXT("run: in a run"), InRun.bInRun);
		if (TestEqual(TEXT("run: three chips"), InRun.Chips.Num(), PAUSE_TEST_CHIP_COUNT))
		{
			const FString WipesLeft = InRun.Chips.Last().Value.ToString();
			TestTrue(TEXT("one wipe used"), WipesLeft.StartsWith(FString::FromInt(DefeatRules::MAX_WIPES_PER_RUN - 1)));
		}
		TestFalse(TEXT("the title differs from the hub's"), InRun.Title.EqualTo(Hub.Title));
	});

	It("pays nothing for a run that was quit (D-176)", [this]()
	{
		if (!TestNotNull(TEXT("run"), Run))
		{
			return;
		}
		StartRun();
		Run->RecordChestReward();
		TestFalse(TEXT("something earned"), Run->GetRunEndPayout().IsEmpty());
		Run->AbandonRun(); // the hub travel is next tick; the world isn't ticked here
		if (TestTrue(TEXT("quit"), Run->IsAbandoned()))
		{
			TestTrue(TEXT("the end pays nothing"), Run->GetRunEndPayout().IsEmpty());
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
