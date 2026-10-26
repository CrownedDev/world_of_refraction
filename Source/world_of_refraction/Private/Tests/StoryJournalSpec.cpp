// StoryJournalSpec.cpp
// B-194 (HubAndStory §4, Q-194): collected memories unlock V's lines and conversations, the hub
// journal plays them, and lore read on a trial map is kept for rereading.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Story/StoryLinesData.h"
#include "UI/Hub/MemoryJournalWidget.h"

namespace
{
	const FName STORY_TEST_MEMORY_ID(TEXT("BarNight"));

	UStoryLinesData *StoryTestData()
	{
		UStoryLinesData *Story = NewObject<UStoryLinesData>(GetTransientPackage());
		FVLine Opening;
		Opening.LineId = TEXT("Opening");
		Opening.Text = FText::FromString(TEXT("V: You're awake."));
		FVLine AfterBar;
		AfterBar.LineId = TEXT("AfterBar");
		AfterBar.RequiredMemories.Add(STORY_TEST_MEMORY_ID);
		AfterBar.Text = FText::FromString(TEXT("V: The bar. You remember."));
		Story->VLines = {Opening, AfterBar};

		FStoryConversation Talk;
		Talk.ConversationId = TEXT("BarTalk");
		Talk.Title = FText::FromString(TEXT("About that night"));
		Talk.RequiredMemories.Add(STORY_TEST_MEMORY_ID);
		Talk.Lines = {FText::FromString(TEXT("A: Line one.")), FText::FromString(TEXT("B: Line two."))};
		Story->Conversations.Add(Talk);
		return Story;
	}

	UMemoryData *StoryTestMemory()
	{
		UMemoryData *Memory = NewObject<UMemoryData>(GetTransientPackage());
		Memory->MemoryId = STORY_TEST_MEMORY_ID;
		Memory->Title = FText::FromString(TEXT("The bar night"));
		Memory->Text = FText::FromString(TEXT("Placeholder scene."));
		return Memory;
	}
}

BEGIN_DEFINE_SPEC(FStoryJournalSpec, "WoR.Story.Journal",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UMemoryJournalSubsystem *Journal = nullptr;
	UMemoryJournalWidget *Widget = nullptr;
END_DEFINE_SPEC(FStoryJournalSpec)

void FStoryJournalSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Journal = TestWorld->GetSubsystem<UMemoryJournalSubsystem>();
		TestNotNull(TEXT("Journal"), Journal);
		Widget = CreateWidget<UMemoryJournalWidget>(TestWorld->GetWorld(), UMemoryJournalWidget::StaticClass());
		TestNotNull(TEXT("Journal widget"), Widget);
		if (Widget)
		{
			Widget->SetStory({StoryTestData()});
		}
	});

	AfterEach([this]()
	{
		Widget = nullptr;
		Journal = nullptr;
		TestWorld.Reset();
	});

	It("unlocks V's line once its memory is collected, each line heard once", [this]()
	{
		if (!Journal || !Widget)
		{
			return;
		}
		TestTrue(TEXT("The opening line plays first"), Widget->HearNextVLine());
		TestTrue(TEXT("It is the opening"), Widget->GetBody().ToString().Contains(TEXT("awake")));
		TestFalse(TEXT("Nothing more before the memory"), Widget->HearNextVLine());

		Journal->RecordMemory(StoryTestMemory());
		TestTrue(TEXT("The memory's line unlocks"), Widget->HearNextVLine());
		TestTrue(TEXT("It is the bar line"), Widget->GetBody().ToString().Contains(TEXT("bar")));
		TestFalse(TEXT("Heard lines don't repeat"), Widget->HearNextVLine());
	});

	It("unlocks a conversation and logs it in the journal once played to the end", [this]()
	{
		if (!Journal || !Widget)
		{
			return;
		}
		TestFalse(TEXT("No conversation before the memory"), Widget->SelectNextConversation());
		Journal->RecordMemory(StoryTestMemory());
		TestTrue(TEXT("Unlocked"), Widget->SelectNextConversation());
		TestTrue(TEXT("First line shown"), Widget->GetBody().ToString().Contains(TEXT("one")));
		TestTrue(TEXT("Second line"), Widget->AdvanceConversation());
		TestFalse(TEXT("Not played until the end"), Journal->HasPlayedConversation(TEXT("BarTalk")));
		TestTrue(TEXT("End"), Widget->AdvanceConversation());
		TestTrue(TEXT("Played: logged"), Journal->HasPlayedConversation(TEXT("BarTalk")));
		TestFalse(TEXT("Nothing past the end"), Widget->AdvanceConversation());

		Widget->ShowJournalPage();
		TestTrue(TEXT("The journal lists the conversation"), Widget->GetBody().ToString().Contains(TEXT("About that night")));
	});

	It("keeps lore once per object and lists memories and lore to reread", [this]()
	{
		if (!Journal || !Widget)
		{
			return;
		}
		Journal->RecordMemory(StoryTestMemory());
		TestTrue(TEXT("Lore kept"), Journal->RecordLore(TEXT("Stone1"), FText::FromString(TEXT("Old stone")),
														FText::FromString(TEXT("Carved words."))));
		TestFalse(TEXT("Same object: kept once"), Journal->RecordLore(TEXT("Stone1"), FText::FromString(TEXT("Old stone")),
																	  FText::FromString(TEXT("Carved words."))));
		Widget->ShowJournalPage();
		const FString Page = Widget->GetBody().ToString();
		TestTrue(TEXT("Memory shown"), Page.Contains(TEXT("The bar night")));
		TestTrue(TEXT("Lore shown"), Page.Contains(TEXT("Carved words.")));
	});
}

#endif
