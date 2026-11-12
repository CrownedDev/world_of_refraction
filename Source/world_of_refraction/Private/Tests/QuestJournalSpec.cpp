// QuestJournalSpec.cpp
// B-388 (MemoryCatalogue §3.12 / §3.13, Crown 2026-11-08): main and side quests are the hidden-quest data with a
// visible Kind: the journal lists them with their title and the current step's goal (main first), a hidden quest is
// never listed, and wor.Quest.Print's text names the goal.
// B-397 (D-171): a hidden quest is never listed before it starts; once found it lists after main and side quests with
// its current goal only (never a later step's or its end memory), for its own player only.
// Q-397b: a found hidden quest is masked ("Hidden Quest <n>", found order) until revealed, masked again on close, and its
// notices never name it.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Memory/HiddenQuestData.h"
#include "Memory/HiddenQuestSubsystem.h"
#include "Memory/MemoryConditions.h"
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

	It("lists a side quest given by a memory with its goal, step by step, and never an unstarted hidden quest", [this]()
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
		TestFalse(TEXT("the hidden quest was never started"), Quests->IsStarted(Hidden->GetQuestId()));
		TestFalse(TEXT("the print shows no unstarted hidden quest"), Quests->GetQuestsString().Contains(TEXT("HiddenQuestSpec")));
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

	It("lists a found hidden quest after main and side quests, with its current goal only (B-397, D-171)", [this]()
	{
		UMemoryJournalSubsystem *Journal = TestWorld->GetSubsystem<UMemoryJournalSubsystem>();
		if (!TestNotNull(TEXT("Journal"), Journal) || !Quests)
		{
			return;
		}
		UMemoryData *Finder = NewObject<UMemoryData>(GetTransientPackage());
		UMemoryData *Secret = NewObject<UMemoryData>(GetTransientPackage());
		Secret->Title = FText::FromString(TEXT("SecretEndSpec"));
		UHiddenQuestData *Hidden = NewQuest(TEXT("FoundHiddenSpec"), EQuestKind::Hidden);
		FHiddenQuestStart FromMemory;
		FromMemory.Memory = Finder;
		Hidden->Starts.Add(FromMemory);
		Hidden->EndMemory = Secret;
		Quests->SetQuestsForStarts({Hidden});
		Quests->StartQuest(NewQuest(TEXT("SideAlsoSpec"), EQuestKind::Side));
		Quests->StartQuest(NewQuest(TEXT("MainAlsoSpec"), EQuestKind::Main));
		TestEqual(TEXT("not found yet: main and side only"), Quests->GetVisibleQuests().Num(), 2);

		Journal->RecordMemory(Finder, EMemorySource::Fragment); // its memory starts it
		TestEqual(TEXT("the private notice is masked (Q-397b)"), Quests->GetLastNotice().ToString(),
				  FString(TEXT("Hidden quest found. Open the journal to view it.")));
		TArray<FVisibleQuest> Visible = Quests->GetVisibleQuests();
		if (!TestEqual(TEXT("found: listed too"), Visible.Num(), 3))
		{
			return;
		}
		TestTrue(TEXT("main, side, then hidden"), Visible[0].Kind == EQuestKind::Main && Visible[1].Kind == EQuestKind::Side &&
													  Visible[2].Kind == EQuestKind::Hidden);
		TestEqual(TEXT("its current goal"), Visible[2].Goal.ToString(), FString(TEXT("FoundHiddenSpec goal 1")));

		UMemoryJournalWidget *Widget = CreateWidget<UMemoryJournalWidget>(TestWorld->GetWorld(), UMemoryJournalWidget::StaticClass());
		if (!TestNotNull(TEXT("journal widget"), Widget))
		{
			return;
		}
		Widget->ShowQuestsPage();
		FString Body = Widget->GetBody().ToString();
		TestTrue(TEXT("masked on the page (Q-397b)"), Body.Contains(TEXT("Hidden Quest 1: masked")) && !Body.Contains(TEXT("FoundHiddenSpec")));
		TestTrue(TEXT("revealed by its button"), Widget->RevealQuest(Hidden->GetQuestId()));
		Body = Widget->GetBody().ToString();
		TestTrue(TEXT("the page shows its current goal"), Body.Contains(TEXT("FoundHiddenSpec goal 1")));
		TestFalse(TEXT("not its later step"), Body.Contains(TEXT("FoundHiddenSpec goal 2")));
		TestFalse(TEXT("not its end memory"), Body.Contains(TEXT("SecretEndSpec")));

		Quests->CheckEvent(FMemoryEvent(), {Hidden}); // the start's own event (Q-377)
		Quests->CheckEvent(AtQuestSpot(), {Hidden});
		TestEqual(TEXT("step done: a masked notice (Q-397b)"), Quests->GetLastNotice().ToString(),
				  FString(TEXT("Hidden quest updated. Open the journal to view it.")));
		Visible = Quests->GetVisibleQuests();
		TestEqual(TEXT("step done: the next goal"), Visible.Last().Goal.ToString(), FString(TEXT("FoundHiddenSpec goal 2")));
		Widget->ShowQuestsPage();
		Body = Widget->GetBody().ToString();
		TestFalse(TEXT("its end memory still hidden"), Body.Contains(TEXT("SecretEndSpec")) || Quests->GetLastNotice().ToString().Contains(TEXT("SecretEndSpec")));

		Quests->CheckEvent(AtQuestSpot(), {Hidden});
		TestTrue(TEXT("finished: its end memory lands"), Journal->HasMemory(Secret));
		TestTrue(TEXT("and only now is named"), Quests->GetLastNotice().ToString().Contains(TEXT("SecretEndSpec")));
		Visible = Quests->GetVisibleQuests();
		TestTrue(TEXT("listed as done, no goal"), Visible.Last().Kind == EQuestKind::Hidden && Visible.Last().bFinished && Visible.Last().Goal.IsEmpty());
	});

	It("masks each found hidden quest as Hidden Quest <n> until revealed, and masks it again when the journal closes (Q-397b)", [this]()
	{
		UMemoryJournalWidget *Widget = Quests ? CreateWidget<UMemoryJournalWidget>(TestWorld->GetWorld(), UMemoryJournalWidget::StaticClass()) : nullptr;
		if (!TestNotNull(TEXT("journal widget"), Widget))
		{
			return;
		}
		UHiddenQuestData *Older = NewQuest(TEXT("MaskOlderSpec"), EQuestKind::Hidden);
		UHiddenQuestData *Newer = NewQuest(TEXT("MaskNewerSpec"), EQuestKind::Hidden);
		Quests->StartQuest(NewQuest(TEXT("MaskSideSpec"), EQuestKind::Side));
		Quests->StartQuest(Older);
		Quests->StartQuest(Newer);
		TestFalse(TEXT("the notice never names it"), Quests->GetLastNotice().ToString().Contains(TEXT("MaskNewerSpec")));

		Widget->ShowQuestsPage();
		FString Body = Widget->GetBody().ToString();
		TestTrue(TEXT("a side quest is never masked"), Body.Contains(TEXT("MaskSideSpec goal 1")));
		TestTrue(TEXT("numbered in found order, newest listed first"),
				 Body.Contains(TEXT("Hidden Quest 2: masked")) && Body.Contains(TEXT("Hidden Quest 1: masked")) &&
					 Body.Find(TEXT("Hidden Quest 2")) < Body.Find(TEXT("Hidden Quest 1")));
		TestFalse(TEXT("no hidden name or goal on the page"), Body.Contains(TEXT("MaskOlderSpec")) || Body.Contains(TEXT("MaskNewerSpec")));
		TestEqual(TEXT("a Reveal button for each"), Widget->GetMaskedQuestIds().Num(), 2);
		TestFalse(TEXT("a side quest has no Reveal"), Widget->RevealQuest(TEXT("MaskSideSpec")));

		TestTrue(TEXT("reveal the older one"), Widget->RevealQuest(Older->GetQuestId()));
		Body = Widget->GetBody().ToString();
		TestTrue(TEXT("its name and goal show"), Body.Contains(TEXT("MaskOlderSpec: MaskOlderSpec goal 1")));
		TestTrue(TEXT("the other stays masked"), Body.Contains(TEXT("Hidden Quest 2: masked")) && !Body.Contains(TEXT("MaskNewerSpec")));
		TestTrue(TEXT("one Reveal button left"), Widget->GetMaskedQuestIds() == TArray<FName>({Newer->GetQuestId()}));
		Widget->ShowJournalPage();
		Widget->ShowQuestsPage();
		TestTrue(TEXT("still revealed while the journal is open"), Widget->GetBody().ToString().Contains(TEXT("MaskOlderSpec goal 1")));

		Widget->Close();
		TestFalse(TEXT("closing forgets the reveal"), Widget->IsQuestRevealed(Older->GetQuestId()));
		Widget->ShowQuestsPage();
		Body = Widget->GetBody().ToString();
		TestTrue(TEXT("masked again next time"), Body.Contains(TEXT("Hidden Quest 1: masked")) && !Body.Contains(TEXT("MaskOlderSpec")));

		Quests->CheckEvent(FMemoryEvent(), {Older}); // the start's own event (Q-377)
		Quests->CheckEvent(AtQuestSpot(), {Older});
		TestEqual(TEXT("a masked step notice"), Quests->GetLastNotice().ToString(), FString(TEXT("Hidden quest updated. Open the journal to view it.")));
		Quests->CheckEvent(AtQuestSpot(), {Older});
		TestTrue(TEXT("finished"), Quests->IsFinished(Older->GetQuestId()));
		Widget->ShowQuestsPage();
		TestTrue(TEXT("a finished one keeps its mask number"), Widget->GetBody().ToString().Contains(TEXT("Hidden Quest 1: done")));
	});

	It("lists the newest found quest first within each group, and keeps that order through a save (Q-397)", [this]()
	{
		if (!Quests)
		{
			return;
		}
		// Ids chosen so id order (A, B, C) is the opposite of found order.
		UHiddenQuestData *First = NewQuest(TEXT("NewestOrderC_Spec"), EQuestKind::Hidden);
		UHiddenQuestData *Second = NewQuest(TEXT("NewestOrderB_Spec"), EQuestKind::Hidden);
		UHiddenQuestData *Third = NewQuest(TEXT("NewestOrderA_Spec"), EQuestKind::Hidden);
		Quests->StartQuest(First);
		Quests->StartQuest(Second);
		Quests->StartQuest(Third);
		auto OrderIs = [this](const TCHAR *What, const TArray<FName> &Expected)
		{
			const TArray<FVisibleQuest> Visible = Quests->GetVisibleQuests();
			TArray<FName> Ids;
			for (const FVisibleQuest &Row : Visible)
			{
				Ids.Add(Row.QuestId);
			}
			TestTrue(What, Ids == Expected);
		};
		OrderIs(TEXT("newest found first"), {Third->GetQuestId(), Second->GetQuestId(), First->GetQuestId()});

		const FSavedHiddenQuests Saved = Quests->Capture();
		Quests->Restore(FSavedHiddenQuests());
		Quests->Restore(Saved);
		OrderIs(TEXT("the same order after a save and load"), {Third->GetQuestId(), Second->GetQuestId(), First->GetQuestId()});

		UHiddenQuestData *AfterLoad = NewQuest(TEXT("NewestOrderZ_Spec"), EQuestKind::Hidden);
		Quests->StartQuest(AfterLoad);
		OrderIs(TEXT("a quest found after loading is the newest"),
				{AfterLoad->GetQuestId(), Third->GetQuestId(), Second->GetQuestId(), First->GetQuestId()});
	});

	It("shows a found hidden quest to its own player only (co-op, D-171)", [this]()
	{
		FWoRTestWorld FriendWorld; // another player's machine: its own game instance and quest subsystem
		if (!Quests || !TestTrue(TEXT("friend's world created"), FriendWorld.Create()))
		{
			return;
		}
		UHiddenQuestSubsystem *FriendQuests = FriendWorld.GetSubsystem<UHiddenQuestSubsystem>();
		if (!TestNotNull(TEXT("friend's quests"), FriendQuests) || !TestTrue(TEXT("a different machine"), FriendQuests != Quests))
		{
			return;
		}
		Quests->StartQuest(NewQuest(TEXT("MineOnlySpec"), EQuestKind::Hidden));
		TestEqual(TEXT("listed for me"), Quests->GetVisibleQuests().Num(), 1);
		TestEqual(TEXT("the friend's list is unchanged"), FriendQuests->GetVisibleQuests().Num(), 0);
		TestTrue(TEXT("and the friend got no notice"), FriendQuests->GetLastNotice().IsEmpty());
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
