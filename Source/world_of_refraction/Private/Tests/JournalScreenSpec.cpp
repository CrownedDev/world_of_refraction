// JournalScreenSpec.cpp
// B-463 (D-181, D-196, D-198, D-199): the journal screen's model. A quest shows the steps reached and no later one;
// a secret quest is masked until shown, shows its current step only, and X / Y show and hide it; the quest filters
// keep one kind; memories and lore list newest first, memories by character; a ready conversation plays line by line
// and is then kept under Played. B-463c: NEW on memories and lore until opened (saved), where each was found, the
// lore's kind and place sub-tabs, a finished quest's reward. The screen itself is the PIE sheet (docs/PIE/B-463.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterData.h"
#include "Engine/GameInstance.h"
#include "Memory/HiddenQuestData.h"
#include "Memory/HiddenQuestSubsystem.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Story/StoryLinesData.h"
#include "UI/Menus/JournalScreenModel.h"
#include "UI/Menus/JournalScreenWidget.h"
#include "WoRTestWorld.h"

namespace
{
	const FName JOURNAL_SCREEN_TEST_LOCATION(TEXT("Spot_JournalScreenSpec"));
	const FName JOURNAL_SCREEN_TEST_MEMORY_ID(TEXT("JournalScreenSpecMemory"));
	const FName JOURNAL_SCREEN_TEST_TALK_ID(TEXT("JournalScreenSpecTalk"));
	constexpr int32 JOURNAL_SCREEN_TEST_QUEST_STEPS = 3;

	FMemoryEvent AtJournalScreenSpot()
	{
		FMemoryEvent Event;
		Event.Locations.Add(JOURNAL_SCREEN_TEST_LOCATION);
		return Event;
	}

	/** A three-step quest of Kind, each step at the test spot, with goals "<Name> goal 1" to "<Name> goal 3". */
	UHiddenQuestData *NewJournalQuest(const FString &Name, EQuestKind Kind)
	{
		UHiddenQuestData *Quest = NewObject<UHiddenQuestData>(GetTransientPackage());
		Quest->QuestId = FName(*Name);
		Quest->Kind = Kind;
		Quest->Title = FText::FromString(Name);
		FWoRCondition There;
		There.Subject = EWoRConditionSubject::AtLocation;
		There.LocationId = JOURNAL_SCREEN_TEST_LOCATION;
		for (int32 Index = 1; Index <= JOURNAL_SCREEN_TEST_QUEST_STEPS; ++Index)
		{
			FHiddenQuestStep Step;
			Step.Conditions.Conditions.Add(There);
			Step.Goal = FText::FromString(FString::Printf(TEXT("%s goal %d"), *Name, Index));
			Quest->Steps.Add(Step);
		}
		return Quest;
	}

	UMemoryData *NewJournalMemory(const FString &Name, UCharacterData *Owner = nullptr)
	{
		UMemoryData *Memory = NewObject<UMemoryData>(GetTransientPackage());
		Memory->MemoryId = FName(*Name);
		Memory->Title = FText::FromString(Name);
		Memory->Text = FText::FromString(Name + TEXT(" text"));
		if (Owner)
		{
			Memory->CharacterDepth.Add(Owner, EMemoryDepth::Deep);
		}
		return Memory;
	}

	/** One two-line conversation, unlocked by the test memory. */
	UStoryLinesData *NewJournalStory()
	{
		UStoryLinesData *Story = NewObject<UStoryLinesData>(GetTransientPackage());
		FStoryConversation Talk;
		Talk.ConversationId = JOURNAL_SCREEN_TEST_TALK_ID;
		Talk.Title = FText::FromString(TEXT("JournalTalkSpec"));
		Talk.RequiredMemories.Add(JOURNAL_SCREEN_TEST_MEMORY_ID);
		Talk.Lines = {FText::FromString(TEXT("Talk line one.")), FText::FromString(TEXT("Talk line two."))};
		Story->Conversations.Add(Talk);
		return Story;
	}

	FString StepsOf(const FJournalRow &Row)
	{
		FString Out;
		for (const FJournalStep &Step : Row.Steps)
		{
			Out += FString::Printf(TEXT("%s%s;"), Step.bDone ? TEXT("done:") : TEXT("now:"), *Step.Text.ToString());
		}
		return Out;
	}
}

BEGIN_DEFINE_SPEC(FJournalScreenSpec, "WoR.UI.Journal", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UHiddenQuestSubsystem *Quests = nullptr;
	UMemoryJournalSubsystem *Journal = nullptr;
	TArray<UStoryLinesData *> Story;
	FJournalState State;

	UGameInstance *Game() const { return TestWorld->GetWorld()->GetGameInstance(); }
	FJournalView View(EJournalPage Page, int32 Filter = 0) const { return WoRJournalScreen::BuildView(Game(), Page, Filter, State, Story); }
	FJournalView QuestView(EJournalQuestFilter Filter = EJournalQuestFilter::All) const { return View(EJournalPage::Quests, static_cast<int32>(Filter)); }
	/** Does the quest's next step (after the start's own event, Q-377). */
	void DoStep(UHiddenQuestData *Quest) const { Quests->CheckEvent(AtJournalScreenSpot(), {Quest}); }
	void Start(UHiddenQuestData *Quest) const
	{
		Quests->StartQuest(Quest);
		Quests->CheckEvent(FMemoryEvent(), {Quest});
	}
END_DEFINE_SPEC(FJournalScreenSpec)

void FJournalScreenSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Quests = TestWorld->GetSubsystem<UHiddenQuestSubsystem>();
		Journal = TestWorld->GetSubsystem<UMemoryJournalSubsystem>();
		Story = {NewJournalStory()};
		State = FJournalState();
	});
	AfterEach([this]()
	{
		Quests = nullptr;
		Journal = nullptr;
		Story.Reset();
		TestWorld.Reset();
	});

	It("has four named pages, and says so when a page is empty", [this]()
	{
		for (int32 Index = 0; Index < WoRJournalScreen::PAGE_COUNT; ++Index)
		{
			const EJournalPage Page = static_cast<EJournalPage>(Index);
			TestFalse(TEXT("a page is named"), WoRJournalScreen::GetPageName(Page).IsEmpty());
			TestEqual(TEXT("no rows"), View(Page).Rows.Num(), 0);
			TestFalse(TEXT("and a line says so"), View(Page).ListNote.IsEmpty());
		}
		TestEqual(TEXT("quests: All, Main, Side, Secret"), QuestView().Filters.Num(), 4);
		TestEqual(TEXT("lore: Everything only, until something is read"), View(EJournalPage::Lore).Filters.Num(), 1);
	});

	It("shows a quest's steps reached and no later one (D-199)", [this]()
	{
		UHiddenQuestData *Main = NewJournalQuest(TEXT("StepsMainSpec"), EQuestKind::Main);
		Start(Main);
		FJournalView Page = QuestView();
		if (!TestEqual(TEXT("one quest"), Page.Rows.Num(), 1))
		{
			return;
		}
		TestEqual(TEXT("the row names the current goal"), Page.Rows[0].Subtitle.ToString(), FString(TEXT("StepsMainSpec goal 1")));
		TestEqual(TEXT("only the first step"), StepsOf(Page.Rows[0]), FString(TEXT("now:StepsMainSpec goal 1;")));
		TestFalse(TEXT("the reward stays hidden"), Page.Rows[0].Note.IsEmpty());
		TestTrue(TEXT("a main quest has no Show / Hide"), Page.Rows[0].ToggleVerb.IsEmpty());

		DoStep(Main);
		Page = QuestView();
		TestEqual(TEXT("step 1 ticked, step 2 lit, step 3 not shown"), StepsOf(Page.Rows[0]),
				  FString(TEXT("done:StepsMainSpec goal 1;now:StepsMainSpec goal 2;")));

		DoStep(Main);
		DoStep(Main);
		Page = QuestView();
		TestTrue(TEXT("finished: dimmed, reads done"), Page.Rows[0].bDone && Page.Rows[0].Subtitle.ToString() == TEXT("done"));
		TestEqual(TEXT("every step ticked"), StepsOf(Page.Rows[0]),
				  FString(TEXT("done:StepsMainSpec goal 1;done:StepsMainSpec goal 2;done:StepsMainSpec goal 3;")));
	});

	It("tracks one quest with A, saves it, and drops it when the quest finishes; a secret quest can't be tracked (B-463b)", [this]()
	{
		UHiddenQuestData *Main = NewJournalQuest(TEXT("TrackMainSpec"), EQuestKind::Main);
		UHiddenQuestData *Side = NewJournalQuest(TEXT("TrackSideSpec"), EQuestKind::Side);
		UHiddenQuestData *Secret = NewJournalQuest(TEXT("TrackSecretSpec"), EQuestKind::Hidden);
		Start(Main);
		Start(Side);
		Start(Secret);
		FJournalView Page = QuestView();
		const FJournalRow *MainRow = Page.Rows.FindByPredicate([](const FJournalRow &Row) { return Row.Id == TEXT("TrackMainSpec"); });
		const FJournalRow *SideRow = Page.Rows.FindByPredicate([](const FJournalRow &Row) { return Row.Id == TEXT("TrackSideSpec"); });
		const FJournalRow *SecretRow = Page.Rows.FindByPredicate([](const FJournalRow &Row) { return Row.Id == TEXT("TrackSecretSpec"); });
		if (!TestTrue(TEXT("three quests"), MainRow && SideRow && SecretRow)) return;
		TestEqual(TEXT("A is Track"), MainRow->SelectVerb.ToString(), FString(TEXT("Track")));
		TestTrue(TEXT("a secret quest has no Track"), SecretRow->SelectVerb.IsEmpty() && !WoRJournalScreen::ToggleTrack(Game(), *SecretRow));
		TestFalse(TEXT("nor through the subsystem"), Quests->TrackQuest(TEXT("TrackSecretSpec")));

		TestTrue(TEXT("tracked"), WoRJournalScreen::ToggleTrack(Game(), *MainRow));
		TestEqual(TEXT("the line: title and current goal"), Quests->GetTrackedLine().ToString(), FString(TEXT("TrackMainSpec: TrackMainSpec goal 1")));
		Page = QuestView();
		MainRow = Page.Rows.FindByPredicate([](const FJournalRow &Row) { return Row.Id == TEXT("TrackMainSpec"); });
		TestTrue(TEXT("the row says so"), MainRow && MainRow->SelectVerb.ToString() == TEXT("Stop tracking") && MainRow->Kicker.ToString().Contains(TEXT("tracked")));
		TestTrue(TEXT("one at a time"), WoRJournalScreen::ToggleTrack(Game(), *SideRow) && Quests->GetTrackedQuest() == FName(TEXT("TrackSideSpec")));

		const FSavedHiddenQuests Saved = Quests->Capture();
		Quests->TrackQuest(NAME_None);
		TestTrue(TEXT("stopped"), Quests->GetTrackedLine().IsEmpty());
		Quests->Restore(Saved);
		TestEqual(TEXT("saved and restored"), Quests->GetTrackedQuest(), FName(TEXT("TrackSideSpec")));

		DoStep(Side);
		TestEqual(TEXT("the line follows the step"), Quests->GetTrackedLine().ToString(), FString(TEXT("TrackSideSpec: TrackSideSpec goal 2")));
		DoStep(Side);
		DoStep(Side);
		TestTrue(TEXT("finished: nothing tracked"), Quests->GetTrackedQuest().IsNone() && Quests->GetTrackedLine().IsEmpty());
	});

	It("masks a secret quest until X shows it, with its current step only; Y hides them all (D-171, D-196, D-198)", [this]()
	{
		UHiddenQuestData *Older = NewJournalQuest(TEXT("SecretOlderSpec"), EQuestKind::Hidden);
		UHiddenQuestData *Newer = NewJournalQuest(TEXT("SecretNewerSpec"), EQuestKind::Hidden);
		Start(Older);
		Start(Newer);
		DoStep(Older);
		FJournalView Page = QuestView();
		if (!TestEqual(TEXT("two secret quests"), Page.Rows.Num(), 2))
		{
			return;
		}
		TestEqual(TEXT("the newest first, numbered in found order"), Page.Rows[0].Title.ToString(), FString(TEXT("Secret Quest 2")));
		TestEqual(TEXT("the older keeps number 1"), Page.Rows[1].Title.ToString(), FString(TEXT("Secret Quest 1")));
		TestEqual(TEXT("its hint reads Show"), Page.Rows[1].ToggleVerb.ToString(), FString(TEXT("Show")));
		TestTrue(TEXT("masked: no step, no name"), Page.Rows[1].Steps.IsEmpty() && !Page.Rows[1].Subtitle.ToString().Contains(TEXT("SecretOlderSpec")));
		TestFalse(TEXT("no player-facing Hidden quest (D-196)"), Page.Rows[1].Kicker.ToString().Contains(TEXT("Hidden")));

		TestTrue(TEXT("X shows it"), WoRJournalScreen::ToggleSecret(Page.Rows[1], State));
		Page = QuestView();
		TestEqual(TEXT("its name"), Page.Rows[1].Title.ToString(), FString(TEXT("SecretOlderSpec")));
		TestEqual(TEXT("its hint reads Hide"), Page.Rows[1].ToggleVerb.ToString(), FString(TEXT("Hide")));
		TestEqual(TEXT("found, and the current step only: not the done one, not the next"), StepsOf(Page.Rows[1]),
				  FString(TEXT("done:(found);now:SecretOlderSpec goal 2;")));
		TestEqual(TEXT("the other stays masked"), Page.Rows[0].Title.ToString(), FString(TEXT("Secret Quest 2")));

		TestTrue(TEXT("X hides it again"), WoRJournalScreen::ToggleSecret(Page.Rows[1], State));
		TestEqual(TEXT("masked again"), QuestView().Rows[1].Title.ToString(), FString(TEXT("Secret Quest 1")));

		WoRJournalScreen::ToggleSecret(Page.Rows[0], State);
		WoRJournalScreen::ToggleSecret(Page.Rows[1], State);
		TestTrue(TEXT("Y hides them all"), WoRJournalScreen::HideAllSecrets(State));
		Page = QuestView();
		TestTrue(TEXT("both masked"), Page.Rows[0].Title.ToString() == TEXT("Secret Quest 2") && Page.Rows[1].Title.ToString() == TEXT("Secret Quest 1"));
		TestFalse(TEXT("Y with none shown does nothing"), WoRJournalScreen::HideAllSecrets(State));
	});

	It("filters the quests by kind, and a secret quest keeps its number under a filter", [this]()
	{
		Start(NewJournalQuest(TEXT("FilterMainSpec"), EQuestKind::Main));
		Start(NewJournalQuest(TEXT("FilterSideSpec"), EQuestKind::Side));
		Start(NewJournalQuest(TEXT("FilterSecretASpec"), EQuestKind::Hidden));
		Start(NewJournalQuest(TEXT("FilterSecretBSpec"), EQuestKind::Hidden));
		TestEqual(TEXT("All: every quest"), QuestView().Rows.Num(), 4);
		const FJournalView Main = QuestView(EJournalQuestFilter::Main);
		TestTrue(TEXT("Main: the main quest"), Main.Rows.Num() == 1 && Main.Rows[0].Kind == EQuestKind::Main);
		TestTrue(TEXT("no Show / Hide on it"), !WoRJournalScreen::ToggleSecret(Main.Rows[0], State));
		const FJournalView Side = QuestView(EJournalQuestFilter::Side);
		TestTrue(TEXT("Side: the side quest"), Side.Rows.Num() == 1 && Side.Rows[0].Kind == EQuestKind::Side);
		const FJournalView Secret = QuestView(EJournalQuestFilter::Secret);
		TestTrue(TEXT("Secret: both, numbered as under All"),
				 Secret.Rows.Num() == 2 && Secret.Rows[0].Title.ToString() == TEXT("Secret Quest 2") && Secret.Rows[1].Title.ToString() == TEXT("Secret Quest 1"));
		TestFalse(TEXT("the Secret tab says why they are hidden"), Secret.ListNote.IsEmpty());
	});

	It("lists memories newest first, by character, and counts only the found ones", [this]()
	{
		UCharacterData *Sera = NewObject<UCharacterData>(GetTransientPackage());
		Sera->Name = TEXT("SeraSpec");
		UCharacterData *Bram = NewObject<UCharacterData>(GetTransientPackage());
		Bram->Name = TEXT("BramSpec");
		Journal->RecordMemory(NewJournalMemory(TEXT("OldSeraMemory"), Sera), EMemorySource::Fragment);
		Journal->RecordMemory(NewJournalMemory(TEXT("BramMemory"), Bram), EMemorySource::Fragment);
		Journal->RecordMemory(NewJournalMemory(TEXT("NewSeraMemory"), Sera), EMemorySource::Fragment);

		const FJournalView Everyone = View(EJournalPage::Memories);
		if (!TestEqual(TEXT("three memories"), Everyone.Rows.Num(), 3))
		{
			return;
		}
		TestEqual(TEXT("newest first"), Everyone.Rows[0].Title.ToString(), FString(TEXT("NewSeraMemory")));
		TestEqual(TEXT("where it was found (the hub: no run)"), Everyone.Rows[0].Subtitle.ToString(), FString(TEXT("Void Realm")));
		TestEqual(TEXT("whose it is, and where"), Everyone.Rows[0].Kicker.ToString(), FString(TEXT("SeraSpec · found in Void Realm")));
		TestEqual(TEXT("its text in the reader"), Everyone.Rows[0].Body.ToString(), FString(TEXT("NewSeraMemory text")));
		TestEqual(TEXT("found, never 'of'"), Everyone.ListNote.ToString(), FString(TEXT("3 found")));
		TestTrue(TEXT("Everyone, then each character by name"),
				 Everyone.Filters.Num() == 3 && Everyone.Filters[1].ToString() == TEXT("BramSpec") && Everyone.Filters[2].ToString() == TEXT("SeraSpec"));

		const FJournalView SeraOnly = View(EJournalPage::Memories, 2);
		TestTrue(TEXT("Sera's two, newest first"), SeraOnly.Rows.Num() == 2 && SeraOnly.Rows[0].Title.ToString() == TEXT("NewSeraMemory") &&
													   SeraOnly.Rows[1].Title.ToString() == TEXT("OldSeraMemory"));
		TestEqual(TEXT("counted"), SeraOnly.ListNote.ToString(), FString(TEXT("2 found")));
	});

	It("lists lore newest first with its text", [this]()
	{
		Journal->RecordLore(TEXT("LoreOldSpec"), FText::FromString(TEXT("Old note")), FText::FromString(TEXT("Old body")));
		Journal->RecordLore(TEXT("LoreNewSpec"), FText::FromString(TEXT("New note")), FText::FromString(TEXT("New body")));
		const FJournalView Lore = View(EJournalPage::Lore);
		TestTrue(TEXT("newest first"), Lore.Rows.Num() == 2 && Lore.Rows[0].Title.ToString() == TEXT("New note") && Lore.Rows[1].Body.ToString() == TEXT("Old body"));
	});

	It("marks memories and lore NEW until opened, saves it, and gives the lore's kind and place (B-463c)", [this]()
	{
		Journal->RecordMemory(NewJournalMemory(TEXT("NewMarkMemorySpec")), EMemorySource::Fragment);
		Journal->RecordLore(TEXT("NewMarkLoreSpec"), FText::FromString(TEXT("A ledger")), FText::FromString(TEXT("Body")), ELoreKind::Note);
		const FJournalView Memories = View(EJournalPage::Memories);
		const FJournalView Lore = View(EJournalPage::Lore);
		if (!TestTrue(TEXT("one of each"), Memories.Rows.Num() == 1 && Lore.Rows.Num() == 1)) return;
		TestTrue(TEXT("both NEW"), Memories.Rows[0].bNew && Memories.Rows[0].Tag.ToString() == TEXT("NEW") && Lore.Rows[0].bNew);
		TestEqual(TEXT("the lore's kind"), Lore.Rows[0].Subtitle.ToString(), FString(TEXT("Note")));
		TestEqual(TEXT("and place"), Lore.Rows[0].Kicker.ToString(), FString(TEXT("Note · Void Realm")));
		TestTrue(TEXT("a sub-tab per place"), Lore.Filters.Num() == 2 && Lore.Filters[1].ToString() == TEXT("Void Realm"));
		TestEqual(TEXT("the place's sub-tab keeps it"), View(EJournalPage::Lore, 1).Rows.Num(), 1);

		const FSavedJournal Saved = Journal->Capture();
		TestTrue(TEXT("opened"), WoRJournalScreen::MarkOpened(Game(), Memories.Rows[0]));
		TestFalse(TEXT("once"), WoRJournalScreen::MarkOpened(Game(), View(EJournalPage::Memories).Rows[0]));
		TestTrue(TEXT("no longer NEW"), View(EJournalPage::Memories).Rows[0].Tag.IsEmpty());
		TestTrue(TEXT("the lore still is"), View(EJournalPage::Lore).Rows[0].bNew);

		Journal->Restore(Saved);
		TestTrue(TEXT("the save keeps what was new"), View(EJournalPage::Memories).Rows.Num() == 1 && View(EJournalPage::Memories).Rows[0].bNew);
		const FJournalView Restored = View(EJournalPage::Lore);
		TestTrue(TEXT("and the lore's kind and place"),
				 Restored.Rows.Num() == 1 && Restored.Rows[0].Kicker.ToString() == TEXT("Note · Void Realm"));
		TestEqual(TEXT("and the memory's place"), View(EJournalPage::Memories).Rows[0].Subtitle.ToString(), FString(TEXT("Void Realm")));

		Journal->Restore(FSavedJournal());
		Journal->RecordLore(TEXT("OldSaveLoreSpec"), FText::FromString(TEXT("Old")), FText::FromString(TEXT("Body")));
		FSavedJournal OlderSave = Journal->Capture();
		OlderSave.NewEntries.Reset(); // a save from before B-463c
		OlderSave.Lore[0].Place = FText::GetEmpty();
		Journal->Restore(OlderSave);
		const FJournalView Older = View(EJournalPage::Lore);
		TestTrue(TEXT("an older save: nothing NEW, no place, no place sub-tab"),
				 Older.Rows.Num() == 1 && !Older.Rows[0].bNew && Older.Rows[0].Kicker.ToString() == TEXT("Inscription") && Older.Filters.Num() == 1);
	});

	It("names a finished quest's reward; an unfinished one keeps it hidden (B-463c)", [this]()
	{
		UHiddenQuestData *Quest = NewJournalQuest(TEXT("RewardQuestSpec"), EQuestKind::Side);
		FMemoryReward Lift;
		Lift.Kind = EMemoryRewardKind::RemoveCurse;
		Lift.RewardId = TEXT("RewardSpecCurse");
		FMemoryReward Title;
		Title.Kind = EMemoryRewardKind::Title; // titles don't exist yet: not named
		Quest->Steps.Last().Rewards = {Lift, Title};
		Start(Quest);
		TestEqual(TEXT("unfinished"), QuestView().Rows[0].Note.ToString(), FString(TEXT("Reward: shown when the quest is done.")));
		for (int32 Step = 0; Step < JOURNAL_SCREEN_TEST_QUEST_STEPS; ++Step)
		{
			DoStep(Quest);
		}
		const FJournalView Done = QuestView();
		if (!TestEqual(TEXT("one quest"), Done.Rows.Num(), 1)) return;
		TestTrue(TEXT("finished"), Done.Rows[0].bDone);
		TestEqual(TEXT("its reward in words"), Done.Rows[0].Note.ToString(), FString(TEXT("Reward: RewardSpecCurse lifted.")));
	});

	It("plays a ready conversation line by line, then keeps it under Played (Q-463)", [this]()
	{
		const int32 Ready = static_cast<int32>(EJournalTalkFilter::Ready);
		const int32 Played = static_cast<int32>(EJournalTalkFilter::Played);
		TestEqual(TEXT("locked: not listed"), View(EJournalPage::Conversations, Ready).Rows.Num(), 0);
		Journal->RecordMemory(NewJournalMemory(JOURNAL_SCREEN_TEST_MEMORY_ID.ToString()), EMemorySource::Fragment);

		FJournalView Page = View(EJournalPage::Conversations, Ready);
		if (!TestEqual(TEXT("its memory unlocks it"), Page.Rows.Num(), 1))
		{
			return;
		}
		TestEqual(TEXT("the tab counts it"), Page.Filters[Ready].ToString(), FString(TEXT("Ready  1")));
		TestTrue(TEXT("new, nothing read yet, A plays"),
				 !Page.Rows[0].Tag.IsEmpty() && Page.Rows[0].Body.IsEmpty() && Page.Rows[0].SelectVerb.ToString() == TEXT("Play"));
		TestEqual(TEXT("nothing under Played"), View(EJournalPage::Conversations, Played).Rows.Num(), 0);

		TestTrue(TEXT("A: the first line"), WoRJournalScreen::PlayNextLine(Game(), Page.Rows[0], State, Story));
		Page = View(EJournalPage::Conversations, Ready);
		TestEqual(TEXT("line one"), Page.Rows[0].Body.ToString(), FString(TEXT("Talk line one.")));
		TestEqual(TEXT("A is Next"), Page.Rows[0].SelectVerb.ToString(), FString(TEXT("Next")));
		TestFalse(TEXT("not played yet"), Journal->HasPlayedConversation(JOURNAL_SCREEN_TEST_TALK_ID));

		TestTrue(TEXT("A: the last line"), WoRJournalScreen::PlayNextLine(Game(), Page.Rows[0], State, Story));
		TestTrue(TEXT("played"), Journal->HasPlayedConversation(JOURNAL_SCREEN_TEST_TALK_ID));
		Page = View(EJournalPage::Conversations, Ready);
		TestTrue(TEXT("still here to read, both lines, nothing more to play"),
				 Page.Rows.Num() == 1 && Page.Rows[0].Body.ToString().Contains(TEXT("Talk line two.")) && Page.Rows[0].SelectVerb.IsEmpty());
		TestFalse(TEXT("A past the end does nothing"), WoRJournalScreen::PlayNextLine(Game(), Page.Rows[0], State, Story));

		State = FJournalState(); // the journal closes (or the page changes)
		TestEqual(TEXT("no longer ready"), View(EJournalPage::Conversations, Ready).Rows.Num(), 0);
		const FJournalView Kept = View(EJournalPage::Conversations, Played);
		TestTrue(TEXT("kept under Played, whole"), Kept.Rows.Num() == 1 && Kept.Rows[0].Body.ToString().Contains(TEXT("Talk line one.")) &&
													   Kept.Rows[0].Body.ToString().Contains(TEXT("Talk line two.")));
		TestTrue(TEXT("the debug text names it"), WoRJournalScreen::GetJournalString(Game(), Story).Contains(TEXT("JournalTalkSpec")));
	});

	It("stays on the talk being played when the one read before it leaves Ready (the screen, headless)", [this]()
	{
		FStoryConversation Second = Story[0]->Conversations[0];
		Second.ConversationId = TEXT("JournalScreenSpecTalkTwo");
		Second.Title = FText::FromString(TEXT("JournalTalkTwoSpec"));
		Story[0]->Conversations.Add(Second);
		FStoryConversation Third = Second;
		Third.ConversationId = TEXT("JournalScreenSpecTalkThree");
		Third.Title = FText::FromString(TEXT("JournalTalkThreeSpec"));
		Story[0]->Conversations.Add(Third);
		Journal->RecordMemory(NewJournalMemory(JOURNAL_SCREEN_TEST_MEMORY_ID.ToString()), EMemorySource::Fragment);
		UJournalScreenWidget *Screen = CreateWidget<UJournalScreenWidget>(TestWorld->GetWorld(), UJournalScreenWidget::StaticClass());
		if (!TestNotNull(TEXT("screen created"), Screen))
		{
			return;
		}
		Screen->SetStory(Story);
		Screen->ActivateWidget();
		Screen->MovePage(-1);
		if (!TestEqual(TEXT("three ready talks"), Screen->GetView().Rows.Num(), 3))
		{
			return;
		}
		Screen->PressSelect();
		Screen->PressSelect();
		TestTrue(TEXT("the first talk is read to its end and stays in the list"),
				 Screen->GetView().Rows.Num() == 3 && Screen->GetFocusedRow()->SelectVerb.IsEmpty());
		Screen->MoveSelection(1);
		Screen->PressSelect();
		TestEqual(TEXT("the first talk left Ready"), Screen->GetView().Rows.Num(), 2);
		TestEqual(TEXT("still on the second talk"), Screen->GetFocusedRow()->Title.ToString(), FString(TEXT("JournalTalkTwoSpec")));
		TestEqual(TEXT("with its first line out"), Screen->GetFocusedRow()->Body.ToString(), FString(TEXT("Talk line one.")));
	});

	It("opens on Quests, wraps its pages and sub-tabs, and forgets shown secret quests on closing (the screen, headless)", [this]()
	{
		Start(NewJournalQuest(TEXT("ScreenMainSpec"), EQuestKind::Main));
		Start(NewJournalQuest(TEXT("ScreenSecretSpec"), EQuestKind::Hidden));
		UJournalScreenWidget *Screen = CreateWidget<UJournalScreenWidget>(TestWorld->GetWorld(), UJournalScreenWidget::StaticClass());
		if (!TestNotNull(TEXT("screen created"), Screen))
		{
			return;
		}
		Screen->SetStory(Story);
		Screen->ActivateWidget();
		TestTrue(TEXT("on Quests, All"), Screen->GetPage() == EJournalPage::Quests && Screen->GetFilter() == 0);
		Screen->MovePage(-1);
		TestTrue(TEXT("back from the first page: the last"), Screen->GetPage() == EJournalPage::Conversations);
		Screen->MovePage(1);
		Screen->MoveFilter(-1);
		TestEqual(TEXT("back from All: Secret"), Screen->GetFilter(), static_cast<int32>(EJournalQuestFilter::Secret));
		TestEqual(TEXT("one secret quest"), Screen->GetView().Rows.Num(), 1);
		TestTrue(TEXT("the hint offers Show"), Screen->GetHints().ToString().Contains(TEXT("X  Show")));

		Screen->PressToggle();
		TestEqual(TEXT("X shows the focused one"), Screen->GetFocusedRow()->Title.ToString(), FString(TEXT("ScreenSecretSpec")));
		TestTrue(TEXT("the hint flips to Hide"), Screen->GetHints().ToString().Contains(TEXT("X  Hide")));
		Screen->PressHideAll();
		TestEqual(TEXT("Y hides it"), Screen->GetFocusedRow()->Title.ToString(), FString(TEXT("Secret Quest 1")));

		Screen->PressToggle();
		Screen->MoveFilter(1);
		Screen->MoveSelection(1);
		TestEqual(TEXT("All again: down to the secret quest, still shown"), Screen->GetFocusedRow()->Title.ToString(), FString(TEXT("ScreenSecretSpec")));
		Screen->MoveSelection(1);
		TestEqual(TEXT("the list wraps"), Screen->GetSelectedRow(), 0);
		Screen->PressToggle();
		TestEqual(TEXT("X on a main quest does nothing"), Screen->GetView().Rows[1].Title.ToString(), FString(TEXT("ScreenSecretSpec")));

		Screen->PressBack();
		TestFalse(TEXT("Back closes it"), Screen->IsActivated());
		Screen->ActivateWidget();
		TestEqual(TEXT("opened again: hidden again (Q-397b)"), Screen->GetView().Rows[1].Title.ToString(), FString(TEXT("Secret Quest 1")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
