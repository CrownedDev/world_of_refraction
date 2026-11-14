// NpcTalkSpec.cpp
// B-471c (D-178; UIScreens.md #20): the talk box's page. A vendor's box has its role and Shop / Talk / Leave; V without
// a shop has no Shop. Under Talk sit V's heard lines and the next unlocked one marked NEW; hearing it moves it to the
// log and unlocks the next (a line whose memory isn't found stays out). An unnamed line reads "Topic n".
// The box, headless: Talk opens the topics on the NEW line, A hears it, Y shows the log and back, B steps back to the
// choices keeping Talk lit, and B on the choices leaves.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Merchant/MerchantData.h"
#include "Story/StoryLinesData.h"
#include "UI/Hub/NpcTalkModel.h"
#include "UI/Hub/NpcTalkWidget.h"
#include "Blueprint/UserWidget.h"

namespace
{
	const FName NPC_TALK_TEST_MEMORY_ID(TEXT("NpcTalkBarNight"));

	UStoryLinesData *NpcTalkTestStory()
	{
		UStoryLinesData *Story = NewObject<UStoryLinesData>(GetTransientPackage());
		FVLine Opening;
		Opening.LineId = TEXT("NpcTalkOpening");
		Opening.Text = FText::FromString(TEXT("Placeholder opening."));
		Opening.TopicName = FText::FromString(TEXT("Waking"));
		FVLine Second;
		Second.LineId = TEXT("NpcTalkSecond");
		Second.Text = FText::FromString(TEXT("Placeholder second."));
		FVLine Locked;
		Locked.LineId = TEXT("NpcTalkLocked");
		Locked.RequiredMemories.Add(NPC_TALK_TEST_MEMORY_ID);
		Locked.Text = FText::FromString(TEXT("Placeholder locked."));
		Story->VLines = {Opening, Second, Locked};
		return Story;
	}
}

BEGIN_DEFINE_SPEC(FNpcTalkSpec, "WoR.UI.NpcTalk", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UMemoryJournalSubsystem *Journal = nullptr;
END_DEFINE_SPEC(FNpcTalkSpec)

void FNpcTalkSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Journal = TestWorld->GetSubsystem<UMemoryJournalSubsystem>();
		TestNotNull(TEXT("Journal"), Journal);
	});

	AfterEach([this]()
	{
		Journal = nullptr;
		TestWorld.Reset();
	});

	It("gives a vendor its role and Shop / Talk / Leave; V without a shop has no Shop (D-178)", [this]()
	{
		UMerchantData *Smith = NewObject<UMerchantData>(GetTransientPackage());
		Smith->MerchantType = EMerchantType::Blacksmith;
		const FNpcTalkPage Page = WoRNpcTalk::Build(Smith, {}, Journal);
		TestFalse(TEXT("a role"), Page.Role.IsEmpty());
		TestTrue(TEXT("Shop, Talk, Leave"), Page.Choices == TArray<ENpcTalkChoice>({ENpcTalkChoice::Shop, ENpcTalkChoice::Talk, ENpcTalkChoice::Leave}));
		const FNpcTalkPage Plain = WoRNpcTalk::Build(nullptr, {}, Journal);
		TestFalse(TEXT("no Shop without a shop"), Plain.Choices.Contains(ENpcTalkChoice::Shop));
		TestTrue(TEXT("no topics without story"), Plain.Topics.IsEmpty() && !Plain.bTalkHasNew);
	});

	It("lists the next line NEW, then keeps it to hear again once heard, the log in order", [this]()
	{
		if (!Journal)
		{
			return;
		}
		const TArray<UStoryLinesData *> Story = {NpcTalkTestStory()};
		FNpcTalkPage Page = WoRNpcTalk::Build(nullptr, Story, Journal);
		if (!TestEqual(TEXT("only the first line at first"), Page.Topics.Num(), 1))
		{
			return;
		}
		TestTrue(TEXT("it is NEW"), Page.Topics[0].bNew && Page.bTalkHasNew);
		TestEqual(TEXT("by its short name"), Page.Topics[0].Name.ToString(), FString(TEXT("Waking")));
		TestTrue(TEXT("nothing in the log"), Page.Log.IsEmpty());

		WoRNpcTalk::Hear(*Journal, Page.Topics[0]);
		Page = WoRNpcTalk::Build(nullptr, Story, Journal);
		if (!TestEqual(TEXT("the heard line and the next"), Page.Topics.Num(), 2))
		{
			return;
		}
		TestFalse(TEXT("the heard one is not NEW"), Page.Topics[0].bNew);
		TestTrue(TEXT("the next is NEW"), Page.Topics[1].bNew);
		TestEqual(TEXT("an unnamed line: Topic 2"), Page.Topics[1].Name.ToString(), FString(TEXT("Topic 2")));
		TestEqual(TEXT("the log holds the heard line"), Page.Log.Num(), 1);

		WoRNpcTalk::Hear(*Journal, Page.Topics[1]);
		Page = WoRNpcTalk::Build(nullptr, Story, Journal);
		TestEqual(TEXT("the locked line stays out"), Page.Topics.Num(), 2);
		TestFalse(TEXT("nothing NEW"), Page.bTalkHasNew);
		TestEqual(TEXT("two in the log"), Page.Log.Num(), 2);
	});
	It("walks the box: Talk on the NEW line, hear it, the log and back, B back then Leave (B-471c)", [this]()
	{
		if (!Journal)
		{
			return;
		}
		UNpcTalkWidget *Box = CreateWidget<UNpcTalkWidget>(TestWorld->GetWorld(), UNpcTalkWidget::StaticClass());
		if (!TestNotNull(TEXT("box"), Box))
		{
			return;
		}
		Box->ActivateWidget();
		Box->SetStory({NpcTalkTestStory()});
		Box->SetSource(nullptr, nullptr);
		TestTrue(TEXT("the choices first"), Box->GetView() == ENpcTalkView::Choices);
		const int32 Talk = Box->GetPage().Choices.IndexOfByKey(ENpcTalkChoice::Talk);
		Box->MoveSelection(Talk - Box->GetSelected());
		Box->PressA();
		TestTrue(TEXT("Talk opens the topics"), Box->GetView() == ENpcTalkView::Topics);
		TestTrue(TEXT("on the NEW line"), Box->GetPage().Topics.IsValidIndex(Box->GetSelected()) && Box->GetPage().Topics[Box->GetSelected()].bNew);
		Box->PressA();
		TestTrue(TEXT("it is said"), Box->GetSpoken().ToString().Contains(TEXT("opening")));
		TestTrue(TEXT("and heard"), Journal->HasHeardVLine(TEXT("NpcTalkOpening")));
		const int32 Lit = Box->GetSelected();
		Box->PressLog();
		TestTrue(TEXT("Y: the log"), Box->GetView() == ENpcTalkView::Log);
		Box->PressLog();
		TestTrue(TEXT("Y again: the topics, the same line lit"), Box->GetView() == ENpcTalkView::Topics && Box->GetSelected() == Lit);
		Box->PressBack();
		TestTrue(TEXT("B: the choices, Talk lit"), Box->GetView() == ENpcTalkView::Choices && Box->GetSelected() == Talk);
		Box->PressBack();
		TestFalse(TEXT("B on the choices leaves"), Box->IsActivated());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
