// RewardNoticeSpec.cpp
// B-465b (UIScreens.md #25, #28): a win's popups. The notices say what they gave (a win's run Prisms, banked levels,
// a memory by its title, a secret quest marked "only you see this"); a batch shows the win first; the queue hands its
// notices over once and keeps at most MAX_WAITING. The popups widget, headless: up to MAX_SHOWN at once, the rest
// wait; once their time is up they go and the waiting ones take their place. Outside a run the trial HUD hides.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "UI/Hub/HubHUDModel.h"
#include "UI/Trial/RewardNotices.h"
#include "UI/Trial/RewardPopupsWidget.h"
#include "WoRTestWorld.h"

namespace
{
	constexpr int32 REWARD_TEST_PRISMS = 180;
	constexpr int32 REWARD_TEST_LEVELS = 2;
	/** A little past a popup's time. */
	constexpr double REWARD_TEST_PAST_SHOWN = WoRRewardNotice::SHOWN_SECONDS + 0.1;
}

BEGIN_DEFINE_SPEC(FRewardNoticeSpec, "WoR.UI.RewardNotice", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	URewardNoticeSubsystem *Notices = nullptr;
END_DEFINE_SPEC(FRewardNoticeSpec)

void FRewardNoticeSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Notices = TestWorld->GetSubsystem<URewardNoticeSubsystem>();
		TestNotNull(TEXT("Notice queue"), Notices);
	});

	AfterEach([this]()
	{
		Notices = nullptr;
		TestWorld.Reset();
	});

	It("words each kind: the win's Prisms, banked levels, a memory's title, a secret quest only you see", [this]()
	{
		const FRewardNotice Win = WoRRewardNotice::MakeWin(REWARD_TEST_PRISMS);
		TestTrue(TEXT("the win names its Prisms"), Win.Body.ToString().Contains(FText::AsNumber(REWARD_TEST_PRISMS).ToString()));
		TestTrue(TEXT("no Prisms line outside a run"), WoRRewardNotice::MakeWin(0).Body.IsEmpty());
		TestTrue(TEXT("levels counted"), WoRRewardNotice::MakeLevels(REWARD_TEST_LEVELS).Body.ToString().Contains(TEXT("+2")));
		const FText Title = FText::FromString(TEXT("The Smith's Promise"));
		TestTrue(TEXT("a memory by its title"), WoRRewardNotice::MakeMemory(Title).Body.EqualTo(Title));
		const FRewardNotice Secret = WoRRewardNotice::MakeQuest(FText::FromString(TEXT("Secret quest found.")), /*bSecret=*/true);
		TestTrue(TEXT("a secret quest: only you see it"), Secret.bOnlyYou && Secret.Kind == ERewardNoticeKind::SecretQuest);
		TestFalse(TEXT("a visible quest: not marked"), WoRRewardNotice::MakeQuest(FText::FromString(TEXT("New quest")), false).bOnlyYou);
		TestFalse(TEXT("each kind its colour"), WoRRewardNotice::GetColor(ERewardNoticeKind::Memory).Equals(WoRRewardNotice::GetColor(ERewardNoticeKind::SecretQuest)));
	});

	It("hands its notices over once, the win first, and keeps at most MAX_WAITING", [this]()
	{
		if (!Notices)
		{
			return;
		}
		Notices->Post(WoRRewardNotice::MakeLevels(REWARD_TEST_LEVELS));
		Notices->Post(WoRRewardNotice::MakeWin(REWARD_TEST_PRISMS));
		const TArray<FRewardNotice> Taken = Notices->TakeWaiting();
		if (!TestEqual(TEXT("both"), Taken.Num(), 2))
		{
			return;
		}
		TestTrue(TEXT("the win first"), Taken[0].Kind == ERewardNoticeKind::Win && Taken[1].Kind == ERewardNoticeKind::Level);
		TestTrue(TEXT("then empty"), Notices->GetWaiting().IsEmpty());

		for (int32 Index = 0; Index <= WoRRewardNotice::MAX_WAITING; ++Index)
		{
			Notices->Post(WoRRewardNotice::MakeLevels(Index + 1));
		}
		TestEqual(TEXT("capped"), Notices->GetWaiting().Num(), WoRRewardNotice::MAX_WAITING);
		TestTrue(TEXT("the oldest went"), Notices->GetWaiting()[0].Body.ToString().Contains(TEXT("+2")));
	});

	It("shows up to MAX_SHOWN, then the waiting ones once the first have gone (the widget, headless)", [this]()
	{
		if (!Notices)
		{
			return;
		}
		URewardPopupsWidget *Popups = CreateWidget<URewardPopupsWidget>(TestWorld->GetWorld(), URewardPopupsWidget::StaticClass());
		if (!TestNotNull(TEXT("popups"), Popups))
		{
			return;
		}
		const int32 Posted = WoRRewardNotice::MAX_SHOWN + 1;
		for (int32 Index = 0; Index < Posted; ++Index)
		{
			Notices->Post(WoRRewardNotice::MakeMemory(FText::AsNumber(Index)));
		}
		Popups->TakeNotices();
		TestEqual(TEXT("MAX_SHOWN on screen"), Popups->GetShown().Num(), WoRRewardNotice::MAX_SHOWN);
		TestEqual(TEXT("one waits"), Popups->GetWaitingCount(), 1);
		TestTrue(TEXT("the queue is empty"), Notices->GetWaiting().IsEmpty());

		Popups->UpdatePopups(REWARD_TEST_PAST_SHOWN);
		TestEqual(TEXT("the first went; the waiting one shows"), Popups->GetShown().Num(), 1);
		TestEqual(TEXT("nothing waits"), Popups->GetWaitingCount(), 0);
		Popups->UpdatePopups(REWARD_TEST_PAST_SHOWN * 2.0);
		TestTrue(TEXT("and then none"), Popups->GetShown().IsEmpty());
	});

	It("hides the trial HUD outside a run (B-465b, #28)", [this]()
	{
		TestFalse(TEXT("not in a run"), WoRHubHUD::BuildTrialStatus(TestWorld->GetWorld()).bInRun);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
