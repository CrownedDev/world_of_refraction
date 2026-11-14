// RunEndModelSpec.cpp
// B-465 (D-187, D-188, D-189; UIScreens.md #26, #27): the wipe card and the run-end page, headless. The card reads "Your
// possibilities end here", names the foe that dealt the last blow (and says nothing when none did), counts the dropped
// Prisms and the wipes, and says where the party wakes, or that the trial ends. The page titles a cleared trial
// apart, says each ending's share in words, lists the payout on one line (nothing for a quit) and reads "Return".
// The card widget, headless: it takes the card's words and its button presses once.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Trial/DefeatRules.h"
#include "UI/Trial/RunEndModel.h"
#include "UI/Trial/RunEndCardWidget.h"
#include "Blueprint/UserWidget.h"
#include "WoRTestWorld.h"

namespace
{
	constexpr int32 RUN_END_TEST_DROPPED = 120;
	constexpr int32 RUN_END_TEST_LOST = 40;
	constexpr int32 RUN_END_TEST_WIPES = 2;
	constexpr int32 RUN_END_TEST_GOLD = 1200;
	constexpr int32 RUN_END_TEST_GEAR = 30;
}

BEGIN_DEFINE_SPEC(FRunEndModelSpec, "WoR.UI.RunEnd", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FRunEndModelSpec)

void FRunEndModelSpec::Define()
{
	It("builds the wipe card: the title, the last blow by name, the drop, where you wake, the wipes (D-187, D-188)", [this]()
	{
		WoRDefeat::FWipeResult Wipe;
		Wipe.PrismsDropped = RUN_END_TEST_DROPPED;
		Wipe.PrismsLostForGood = RUN_END_TEST_LOST;
		Wipe.Wipes = RUN_END_TEST_WIPES;
		Wipe.LastBlowBy = FText::FromString(TEXT("Kael"));
		const FWipeCard Card = WoRRunEnd::BuildWipeCard(Wipe, /*bHasRestStand=*/true);
		TestEqual(TEXT("D-188's title"), Card.Title.ToString(), FString(TEXT("Your possibilities end here")));
		TestTrue(TEXT("the foe by name"), Card.LastBlowLine.ToString().Contains(TEXT("Kael")));
		TestTrue(TEXT("the drop and the earlier one, the wake, the enemy"), Card.Lines.Num() == 4);
		TestTrue(TEXT("the drop counted"), Card.Lines[0].ToString().Contains(FText::AsNumber(RUN_END_TEST_DROPPED).ToString()));
		TestEqual(TEXT("wipes used"), Card.WipesUsed, RUN_END_TEST_WIPES);
		TestEqual(TEXT("of 5"), Card.WipesMax, DefeatRules::MAX_WIPES_PER_RUN);
		TestEqual(TEXT("one button"), Card.ButtonText.ToString(), FString(TEXT("Get up")));
		TestFalse(TEXT("the run goes on"), Card.bRunEnds);

		WoRDefeat::FWipeResult Last;
		Last.Wipes = DefeatRules::MAX_WIPES_PER_RUN;
		Last.bRunEnds = true;
		const FWipeCard Ends = WoRRunEnd::BuildWipeCard(Last, /*bHasRestStand=*/false);
		TestTrue(TEXT("no last blow named when no foe dealt it"), Ends.LastBlowLine.IsEmpty());
		TestTrue(TEXT("the run ends"), Ends.bRunEnds && Ends.Lines.Num() == 1);
	});

	It("builds the run-end page: the title, the share in words, one payout line, Return (D-187, D-189)", [this]()
	{
		FRunPayout Payout;
		Payout.HubGold = RUN_END_TEST_GOLD;
		Payout.GearEssence = RUN_END_TEST_GEAR;
		const FRunEndPage Cleared = WoRRunEnd::BuildRunEnd(ERunEnding::BossBeaten, Payout);
		TestTrue(TEXT("cleared"), Cleared.bCleared);
		TestEqual(TEXT("Trial cleared"), Cleared.Title.ToString(), FString(TEXT("Trial cleared")));
		TestTrue(TEXT("both currencies on one line"),
				 Cleared.PayoutLine.ToString().Contains(FText::AsNumber(RUN_END_TEST_GOLD).ToString()) && Cleared.PayoutLine.ToString().Contains(FText::AsNumber(RUN_END_TEST_GEAR).ToString()));
		TestEqual(TEXT("Return"), Cleared.ButtonText.ToString(), FString(TEXT("Return")));

		const FRunEndPage Wiped = WoRRunEnd::BuildRunEnd(ERunEnding::OutOfWipes, Payout);
		TestFalse(TEXT("not cleared"), Wiped.bCleared);
		TestNotEqual(TEXT("its own share"), Wiped.ShareLine.ToString(), Cleared.ShareLine.ToString());
		const FRunEndPage Quit = WoRRunEnd::BuildRunEnd(ERunEnding::Quit, Payout);
		TestFalse(TEXT("a quit lists no payout"), Quit.PayoutLine.ToString().Contains(FText::AsNumber(RUN_END_TEST_GOLD).ToString()));
		TestFalse(TEXT("an empty payout says so"), WoRRunEnd::FormatPayoutLine(FRunPayout()).IsEmpty());
	});
	It("shows the card's words and presses its button once (URunEndCardWidget)", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("world"), TestWorld.Create()))
		{
			return;
		}
		URunEndCardWidget *Card = CreateWidget<URunEndCardWidget>(TestWorld.GetWorld(), URunEndCardWidget::StaticClass());
		if (!TestNotNull(TEXT("card"), Card))
		{
			return;
		}
		WoRDefeat::FWipeResult Wipe;
		Wipe.Wipes = RUN_END_TEST_WIPES;
		Card->ShowWipe(WoRRunEnd::BuildWipeCard(Wipe, /*bHasRestStand=*/false));
		TestEqual(TEXT("the wipe title"), Card->GetTitle().ToString(), FString(TEXT("Your possibilities end here")));
		TestTrue(TEXT("Get up"), Card->GetButtonText().ToString().Contains(TEXT("Get up")));
		Card->PressContinue();
		Card->PressContinue();
		TestTrue(TEXT("pressed"), Card->HasContinued());
		Card->ShowRunEnd(WoRRunEnd::BuildRunEnd(ERunEnding::OutOfWipes, FRunPayout()));
		TestFalse(TEXT("a new page waits for its own press"), Card->HasContinued());
		TestTrue(TEXT("Return"), Card->GetButtonText().ToString().Contains(TEXT("Return")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
