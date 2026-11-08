// QuestJournalSpec.cpp
// B-388 (MemoryCatalogue §3.12 / §3.13, Crown 2026-11-08): main and side quests are the hidden-quest data with a
// visible Kind: the journal lists them with their title and the current step's goal (main first), a hidden quest is
// never listed, and wor.Quest.Print's text names the goal.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Memory/HiddenQuestData.h"
#include "Memory/HiddenQuestSubsystem.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "UI/Hub/MemoryJournalWidget.h"
#include "WoRGameNames.h"

namespace
{
	const FName QUEST_JOURNAL_TEST_LOCATION(TEXT("Spot_QuestJournalSpec"));

	FMemoryEvent AtQuestSpot()
	{
		FMemoryEvent Event;
		Event.Locations.Add(QUEST_JOURNAL_TEST_LOCATION);
		return Event;
	}

	/** A two-step quest of Kind, each step at the test spot, with goals "<Name> goal 1" / "<Name> goal 2". */
	UHiddenQuestData *NewQuest(const FString &Name, EQuestKind Kind)
	{
		UHiddenQuestData *Quest = NewObject<UHiddenQuestData>(GetTransientPackage()); // review: a fresh object each run
		Quest->QuestId = FName(*Name);
		Quest->Kind = Kind;
		Quest->Title = FText::FromString(Name);
		FWoRCondition There;
		There.Subject = EWoRConditionSubject::AtLocation;
		There.LocationId = QUEST_JOURNAL_TEST_LOCATION;
		for (int32 Index = 1; Index <= 2; ++Index)
		{
			FHiddenQuestStep Step;
			Step.Conditions.Conditions.Add(There);
			Step.Goal = FText::FromString(FString::Printf(TEXT("%s goal %d"), *Name, Index));
			Quest->Steps.Add(Step);
		}
		return Quest;
	}
}

BEGIN_DEFINE_SPEC(FQuestJournalSpec, "WoR.Memory.QuestJournal", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UHiddenQuestSubsystem *Quests = nullptr;
END_DEFINE_SPEC(FQuestJournalSpec)

void FQuestJournalSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Quests = TestWorld->GetSubsystem<UHiddenQuestSubsystem>();
		TestNotNull(TEXT("Quests"), Quests);
	});

	AfterEach([this]()
	{
		Quests = nullptr;
		TestWorld.Reset();
	});

	It("lists a side quest given by a memory with its goal, step by step, and never a hidden quest", [this]()
	{
		UMemoryJournalSubsystem *Journal = TestWorld->GetSubsystem<UMemoryJournalSubsystem>();
		if (!TestNotNull(TEXT("Journal"), Journal) || !Quests)
		{
			return;
		}
		UMemoryData *Giver = NewObject<UMemoryData>(GetTransientPackage());
		UHiddenQuestData *Side = NewQuest(TEXT("SideQuestSpec"), EQuestKind::Side);
		FHiddenQuestStart FromMemory;
		FromMemory.Memory = Giver;
		Side->Starts.Add(FromMemory);
		UHiddenQuestData *Hidden = NewQuest(TEXT("HiddenQuestSpec"), EQuestKind::Hidden);
		Quests->SetQuestsForStarts({Side});
		Quests->StartQuest(Hidden);

		Journal->RecordMemory(Giver, EMemorySource::Fragment); // a memory gives the side quest
		TArray<FVisibleQuest> Visible = Quests->GetVisibleQuests();
		if (!TestEqual(TEXT("only the side quest is listed"), Visible.Num(), 1))
		{
			return;
		}
		TestEqual(TEXT("its title"), Visible[0].Title.ToString(), FString(TEXT("SideQuestSpec")));
		TestEqual(TEXT("its first goal"), Visible[0].Goal.ToString(), FString(TEXT("SideQuestSpec goal 1")));

		Quests->CheckEvent(FMemoryEvent(), {Side}); // the start's own event (Q-377)
		Quests->CheckEvent(AtQuestSpot(), {Side});
		TestEqual(TEXT("step 1 done: the next goal"), Quests->GetVisibleQuests()[0].Goal.ToString(), FString(TEXT("SideQuestSpec goal 2")));
		TestTrue(TEXT("the print names the goal"), Quests->GetQuestsString().Contains(TEXT("goal: SideQuestSpec goal 2")));
		TestFalse(TEXT("the print gives a hidden quest no goal"), Quests->GetQuestsString().Contains(TEXT("HiddenQuestSpec goal")));
		Quests->CheckEvent(AtQuestSpot(), {Side});
		Visible = Quests->GetVisibleQuests();
		TestTrue(TEXT("finished"), Visible.Num() == 1 && Visible[0].bFinished && Visible[0].Goal.IsEmpty());
	});

	It("lists main quests before side quests, and the journal's Quests page shows them", [this]()
	{
		if (!Quests)
		{
			return;
		}
		Quests->StartQuest(NewQuest(TEXT("SideFirstSpec"), EQuestKind::Side));
		Quests->StartQuest(NewQuest(TEXT("MainSecondSpec"), EQuestKind::Main));
		const TArray<FVisibleQuest> Visible = Quests->GetVisibleQuests();
		TestTrue(TEXT("main first"), Visible.Num() == 2 && Visible[0].Kind == EQuestKind::Main && Visible[1].Kind == EQuestKind::Side);

		UMemoryJournalWidget *Widget = CreateWidget<UMemoryJournalWidget>(TestWorld->GetWorld(), UMemoryJournalWidget::StaticClass());
		if (!TestNotNull(TEXT("journal widget"), Widget))
		{
			return;
		}
		Widget->ShowQuestsPage();
		TestTrue(TEXT("the page shows the main quest's goal"), Widget->GetBody().ToString().Contains(TEXT("MainSecondSpec goal 1")));
		TestTrue(TEXT("and the side quest"), Widget->GetBody().ToString().Contains(TEXT("SideFirstSpec")));
		TestTrue(TEXT("with the kind's shared name"), Widget->GetBody().ToString().Contains(WoRGameNames::GetQuestKindName(EQuestKind::Main).ToString()));
	});

	It("gives a quest by id only when one has that id", [this]()
	{
		if (Quests)
		{
			TestFalse(TEXT("unknown id"), Quests->StartQuestById(TEXT("NoSuchQuestSpec")));
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
