// JournalScreenModel.cpp

#include "UI/Menus/JournalScreenModel.h"

#include "Character/CharacterData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/EquipmentDataBase.h"
#include "HAL/IConsoleManager.h"
#include "Memory/CurseData.h"
#include "Memory/CurseSubsystem.h"
#include "Memory/HiddenQuestSubsystem.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Story/StoryLinesData.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRJournalScreen"

namespace JournalScreenBuild
{
	template <typename TSubsystem>
	TSubsystem *Find(const UGameInstance *GameInstance)
	{
		return GameInstance ? GameInstance->GetSubsystem<TSubsystem>() : nullptr;
	}

	// TODO(B-127): character names are FString on the asset until they become FText.
	FText CharacterName(const UCharacterData *Character)
	{
		return Character ? FText::FromString(Character->Name) : FText::GetEmpty();
	}

	FText JoinNames(const TArray<FText> &Names)
	{
		return FText::Join(LOCTEXT("NameSeparator", ", "), Names);
	}

	/** Paragraphs, a blank line between them. */
	FText JoinLines(const TArray<FText> &Lines)
	{
		return FText::Join(FText::AsCultureInvariant(TEXT("\n\n")), Lines);
	}

	FText GetNewTag()
	{
		return LOCTEXT("EntryNew", "NEW");
	}

	FText GetLoreKindName(ELoreKind Kind)
	{
		switch (Kind)
		{
		case ELoreKind::Note:
			return LOCTEXT("LoreNote", "Note");
		case ELoreKind::Overheard:
			return LOCTEXT("LoreOverheard", "Overheard");
		case ELoreKind::ItemDescription:
			return LOCTEXT("LoreItemDescription", "Item description");
		case ELoreKind::Inscription:
			break;
		}
		return LOCTEXT("LoreInscription", "Inscription");
	}

	/** B-463c: one reward in player words; empty for one with nothing to name (a title, until titles exist). */
	FText DescribeReward(const UGameInstance *GameInstance, const FMemoryReward &Reward)
	{
		const UCurseSubsystem *Curses = Find<UCurseSubsystem>(GameInstance);
		const UCurseData *Curse = Curses ? Curses->FindCurseAsset(Reward.RewardId) : nullptr;
		const FText CurseName = Curse && !Curse->Title.IsEmpty() ? Curse->Title : FText::FromName(Reward.RewardId);
		switch (Reward.Kind)
		{
		case EMemoryRewardKind::Item:
			return Reward.Item ? FText::FromString(Reward.Item->GetDisplayName()) : FText::GetEmpty(); // TODO(B-127): FString names
		case EMemoryRewardKind::Skill:
			return Reward.Skill ? FText::FromString(Reward.Skill->Name) : FText::GetEmpty();
		case EMemoryRewardKind::Curse:
			return FText::Format(LOCTEXT("RewardCurse", "the curse {0}"), CurseName);
		case EMemoryRewardKind::RemoveCurse:
			return FText::Format(LOCTEXT("RewardRemoveCurse", "{0} lifted"), CurseName);
		case EMemoryRewardKind::Title:
			break;
		}
		return FText::GetEmpty();
	}

	bool KeepsQuest(EJournalQuestFilter Filter, EQuestKind Kind)
	{
		switch (Filter)
		{
		case EJournalQuestFilter::Main:
			return Kind == EQuestKind::Main;
		case EJournalQuestFilter::Side:
			return Kind == EQuestKind::Side;
		case EJournalQuestFilter::Secret:
			return Kind == EQuestKind::Hidden;
		default:
			return true;
		}
	}

	/** Q-397b: each found secret quest's mask number is its place in found order (oldest 1), so it keeps its number
	 *  as more are found. */
	TMap<FName, int32> NumberSecrets(const TArray<FVisibleQuest> &Quests)
	{
		TArray<const FVisibleQuest *> ByFound;
		for (const FVisibleQuest &Quest : Quests)
		{
			if (Quest.Kind == EQuestKind::Hidden)
			{
				ByFound.Add(&Quest);
			}
		}
		ByFound.StableSort([](const FVisibleQuest &A, const FVisibleQuest &B) { return A.FoundOrder < B.FoundOrder; });
		TMap<FName, int32> Numbers;
		for (int32 Index = 0; Index < ByFound.Num(); ++Index)
		{
			Numbers.Add(ByFound[Index]->QuestId, Index + 1);
		}
		return Numbers;
	}

	FJournalRow MakeQuestRow(const UGameInstance *GameInstance, const FVisibleQuest &Quest, const int32 *MaskNumber, bool bShown,
							 bool bTracked)
	{
		const FText Done = LOCTEXT("QuestDone", "done");
		FJournalRow Row;
		Row.Id = Quest.QuestId;
		Row.bQuest = true;
		Row.Kind = Quest.Kind;
		Row.bDone = Quest.bFinished;
		Row.Tag = WoRGameNames::GetQuestKindName(Quest.Kind); // D-062: a shared game word
		Row.Kicker = FText::Format(LOCTEXT("QuestKicker", "{0} quest"), Row.Tag);
		if (MaskNumber)
		{
			Row.ToggleVerb = bShown ? LOCTEXT("Hide", "Hide") : LOCTEXT("Show", "Show");
		}
		if (MaskNumber && !bShown)
		{
			Row.Title = FText::Format(LOCTEXT("MaskedQuestName", "Secret Quest {0}"), FText::AsNumber(*MaskNumber)); // D-196
			Row.Subtitle = Quest.bFinished ? Done : LOCTEXT("MaskedQuestSubtitle", "hidden · X to show");
			Row.Body = LOCTEXT("MaskedQuestBody", "Hidden.");
			return Row;
		}
		Row.Title = Quest.Title;
		Row.Subtitle = Quest.bFinished ? Done : Quest.Goal;
		if (!MaskNumber && !Quest.bFinished)
		{
			// B-463b: A tracks one quest (not a secret one: its step would show on a shared screen, D-198).
			Row.SelectVerb = bTracked ? LOCTEXT("Untrack", "Stop tracking") : LOCTEXT("Track", "Track");
			if (bTracked)
			{
				Row.Kicker = FText::Format(LOCTEXT("TrackedKicker", "{0} quest · tracked"), Row.Tag);
			}
		}
		if (MaskNumber)
		{
			// D-171: a secret quest shows that it was found and its current step, nothing else.
			Row.Steps.Add({LOCTEXT("SecretFound", "(found)"), true});
		}
		for (const FText &Goal : Quest.DoneGoals)
		{
			if (!Goal.IsEmpty())
			{
				Row.Steps.Add({Goal, true});
			}
		}
		if (!Quest.bFinished)
		{
			if (!Quest.Goal.IsEmpty())
			{
				Row.Steps.Add({Quest.Goal, false});
			}
			// D-199: later steps are not shown at all. Rewards stay hidden until earned (UIScreens #6).
			Row.Note = MaskNumber ? LOCTEXT("SecretNote", "What comes next stays hidden until you reach it.")
								  : LOCTEXT("RewardNote", "Reward: shown when the quest is done.");
			return Row;
		}
		TArray<FText> Rewards;
		for (const FMemoryReward &Reward : Quest.Rewards)
		{
			const FText Words = DescribeReward(GameInstance, Reward);
			if (!Words.IsEmpty())
			{
				Rewards.Add(Words);
			}
		}
		if (!Rewards.IsEmpty())
		{
			Row.Note = FText::Format(LOCTEXT("RewardDoneNote", "Reward: {0}."), JoinNames(Rewards));
		}
		return Row;
	}

	void BuildQuests(UGameInstance *GameInstance, int32 Filter, const FJournalState &State, FJournalView &View)
	{
		View.Filters = {LOCTEXT("FilterAll", "All"), WoRGameNames::GetQuestKindName(EQuestKind::Main),
						WoRGameNames::GetQuestKindName(EQuestKind::Side), WoRGameNames::GetQuestKindName(EQuestKind::Hidden)};
		const EJournalQuestFilter Kept = static_cast<EJournalQuestFilter>(FMath::Clamp(Filter, 0, View.Filters.Num() - 1));
		UHiddenQuestSubsystem *Quests = Find<UHiddenQuestSubsystem>(GameInstance);
		const TArray<FVisibleQuest> Visible = Quests ? Quests->GetVisibleQuests() : TArray<FVisibleQuest>();
		const TMap<FName, int32> MaskNumbers = NumberSecrets(Visible);
		for (const FVisibleQuest &Quest : Visible)
		{
			if (KeepsQuest(Kept, Quest.Kind))
			{
				View.Rows.Add(MakeQuestRow(GameInstance, Quest, MaskNumbers.Find(Quest.QuestId), State.ShownSecrets.Contains(Quest.QuestId),
										   Quests->GetTrackedQuest() == Quest.QuestId));
			}
		}
		if (View.Rows.IsEmpty())
		{
			View.ListNote = LOCTEXT("NoQuests", "No quests yet.");
		}
		else if (Kept == EJournalQuestFilter::Secret)
		{
			View.ListNote = LOCTEXT("SecretListNote", "Secret quests are hidden when the journal opens, so a shared screen doesn't spoil them.");
		}
	}

	/** The characters Memory means something to, in a fixed order. */
	TArray<const UCharacterData *> GetOwners(const UMemoryData &Memory)
	{
		TArray<const UCharacterData *> Owners;
		for (const TPair<TObjectPtr<UCharacterData>, EMemoryDepth> &Entry : Memory.CharacterDepth)
		{
			if (Entry.Key && Entry.Value != EMemoryDepth::None)
			{
				Owners.Add(Entry.Key);
			}
		}
		Owners.Sort([](const UCharacterData &A, const UCharacterData &B) { return A.Name < B.Name; });
		return Owners;
	}

	void BuildMemories(const UGameInstance *GameInstance, int32 Filter, FJournalView &View)
	{
		const UMemoryJournalSubsystem *Journal = Find<UMemoryJournalSubsystem>(GameInstance);
		const TArray<TObjectPtr<UMemoryData>> NoMemories;
		const TArray<TObjectPtr<UMemoryData>> &Found = Journal ? Journal->GetMemories() : NoMemories;

		// The sub-tabs: everyone, then each character a found memory belongs to. Unfound memories are never
		// listed or counted (pillar 10).
		TArray<const UCharacterData *> Characters;
		for (const UMemoryData *Memory : Found)
		{
			if (Memory)
			{
				for (const UCharacterData *Owner : GetOwners(*Memory))
				{
					Characters.AddUnique(Owner);
				}
			}
		}
		Characters.Sort([](const UCharacterData &A, const UCharacterData &B) { return A.Name < B.Name; });
		View.Filters.Add(LOCTEXT("FilterEveryone", "Everyone"));
		for (const UCharacterData *Character : Characters)
		{
			View.Filters.Add(CharacterName(Character));
		}
		const int32 Kept = FMath::Clamp(Filter, 0, View.Filters.Num() - 1);
		const UCharacterData *Only = Kept > 0 ? Characters[Kept - 1] : nullptr;

		for (int32 Index = Found.Num() - 1; Index >= 0; --Index) // newest first
		{
			const UMemoryData *Memory = Found[Index];
			if (!Memory)
			{
				continue;
			}
			const TArray<const UCharacterData *> Owners = GetOwners(*Memory);
			if (Only && !Owners.Contains(Only))
			{
				continue;
			}
			TArray<FText> Names;
			for (const UCharacterData *Owner : Owners)
			{
				Names.Add(CharacterName(Owner));
			}
			FJournalRow &Row = View.Rows.AddDefaulted_GetRef();
			Row.Id = Memory->GetMemoryId();
			Row.Title = Memory->Title.IsEmpty() ? FText::FromName(Row.Id) : Memory->Title;
			Row.bNew = Journal->IsNew(Row.Id);
			Row.Tag = Row.bNew ? GetNewTag() : FText::GetEmpty();
			// B-463c: the place it was found (a memory from before places were kept has none).
			const FText Place = Journal->GetPlace(Memory);
			Row.Subtitle = Place.IsEmpty() ? JoinNames(Names) : Place;
			Row.Kicker = Place.IsEmpty() ? JoinNames(Names)
										 : FText::Format(LOCTEXT("MemoryKicker", "{0} · found in {1}"), JoinNames(Names), Place);
			Row.Body = Memory->Text;
		}
		View.ListNote = View.Rows.IsEmpty() ? LOCTEXT("NoMemories", "No memories yet.")
											: FText::Format(LOCTEXT("MemoriesFound", "{0} found"), FText::AsNumber(View.Rows.Num()));
	}

	void BuildLore(const UGameInstance *GameInstance, int32 Filter, FJournalView &View)
	{
		const UMemoryJournalSubsystem *Journal = Find<UMemoryJournalSubsystem>(GameInstance);
		const TArray<FJournalLoreEntry> NoLore;
		const TArray<FJournalLoreEntry> &Read = Journal ? Journal->GetLore() : NoLore;

		// B-463c (UIScreens #6): sub-tabs by place, in the order first read there; only places something was read in.
		TArray<FString> Places;
		View.Filters.Add(LOCTEXT("FilterEverything", "Everything"));
		for (const FJournalLoreEntry &Entry : Read)
		{
			if (!Entry.Place.IsEmpty() && !Places.Contains(Entry.Place.ToString()))
			{
				Places.Add(Entry.Place.ToString());
				View.Filters.Add(Entry.Place);
			}
		}
		const int32 Kept = FMath::Clamp(Filter, 0, View.Filters.Num() - 1);

		for (int32 Index = Read.Num() - 1; Index >= 0; --Index) // newest first
		{
			const FJournalLoreEntry &Entry = Read[Index];
			if (Kept > 0 && Entry.Place.ToString() != Places[Kept - 1])
			{
				continue;
			}
			FJournalRow &Row = View.Rows.AddDefaulted_GetRef();
			Row.Id = Entry.LoreId;
			Row.Title = Entry.Title;
			Row.Body = Entry.Body;
			Row.bNew = Journal->IsNew(Row.Id);
			Row.Tag = Row.bNew ? GetNewTag() : FText::GetEmpty();
			Row.Subtitle = GetLoreKindName(Entry.Kind);
			Row.Kicker = Entry.Place.IsEmpty() ? Row.Subtitle
											   : FText::Format(LOCTEXT("LoreKicker", "{0} · {1}"), Row.Subtitle, Entry.Place);
		}
		View.ListNote = View.Rows.IsEmpty() ? LOCTEXT("NoLore", "Nothing read yet.")
											: FText::Format(LOCTEXT("LoreFound", "{0} found"), FText::AsNumber(View.Rows.Num()));
	}

	/** The conversation being played stays under Ready until another is started or the journal closes, so its
	 *  last line can be read where it was played. */
	bool IsReady(const FStoryConversation &Talk, const UMemoryJournalSubsystem &Journal, const FJournalState &State)
	{
		return Talk.ConversationId == State.TalkId || !Journal.HasPlayedConversation(Talk.ConversationId);
	}

	void BuildConversations(const UGameInstance *GameInstance, int32 Filter, const FJournalState &State,
							const TArray<UStoryLinesData *> &Story, FJournalView &View)
	{
		const UMemoryJournalSubsystem *Journal = Find<UMemoryJournalSubsystem>(GameInstance);
		const TArray<const FStoryConversation *> Unlocked =
			Journal ? StoryUnlocks::GetUnlockedConversations(Story, *Journal) : TArray<const FStoryConversation *>();
		const int32 ReadyCount = Unlocked.FilterByPredicate([&](const FStoryConversation *Talk) { return IsReady(*Talk, *Journal, State); }).Num();
		View.Filters = {ReadyCount > 0 ? FText::Format(LOCTEXT("FilterReadyCount", "Ready  {0}"), FText::AsNumber(ReadyCount))
									   : LOCTEXT("FilterReady", "Ready"),
						LOCTEXT("FilterPlayed", "Played")};
		const bool bShowReady = FMath::Clamp(Filter, 0, View.Filters.Num() - 1) == static_cast<int32>(EJournalTalkFilter::Ready);

		for (const FStoryConversation *Talk : Unlocked)
		{
			if (IsReady(*Talk, *Journal, State) != bShowReady)
			{
				continue;
			}
			TArray<FText> Names;
			for (const UCharacterData *Speaker : Talk->Speakers)
			{
				if (Speaker)
				{
					Names.Add(CharacterName(Speaker));
				}
			}
			FJournalRow &Row = View.Rows.AddDefaulted_GetRef();
			Row.Id = Talk->ConversationId;
			Row.Title = Talk->Title;
			Row.Subtitle = JoinNames(Names);
			if (!bShowReady)
			{
				Row.Kicker = LOCTEXT("TalkPlayed", "Played");
				Row.Body = JoinLines(Talk->Lines);
				continue;
			}
			const int32 LinesOut = Talk->ConversationId == State.TalkId ? FMath::Min(State.TalkLines, Talk->Lines.Num()) : 0;
			const bool bEnded = LinesOut > 0 && LinesOut >= Talk->Lines.Num();
			Row.Kicker = LOCTEXT("TalkReady", "Ready");
			Row.Tag = bEnded ? FText::GetEmpty() : LOCTEXT("TalkNew", "NEW");
			Row.Body = JoinLines(TArray<FText>(Talk->Lines.GetData(), LinesOut));
			Row.Note = bEnded ? LOCTEXT("TalkEnded", "The talk is kept under Played to re-read.")
							  : LOCTEXT("TalkKept", "Once played, the talk is kept here to re-read.");
			if (!bEnded)
			{
				Row.SelectVerb = LinesOut > 0 ? LOCTEXT("TalkNext", "Next") : LOCTEXT("TalkPlay", "Play");
			}
		}
		if (bShowReady)
		{
			View.ListNote = LOCTEXT("TalkListNote", "New talks unlock as memories return.");
		}
		else if (View.Rows.IsEmpty())
		{
			View.ListNote = LOCTEXT("NoTalksPlayed", "No talk played yet.");
		}
	}
}

FText WoRJournalScreen::GetPageName(EJournalPage Page)
{
	switch (Page)
	{
	case EJournalPage::Quests:
		return LOCTEXT("PageQuests", "Quests");
	case EJournalPage::Memories:
		return LOCTEXT("PageMemories", "Memories");
	case EJournalPage::Lore:
		return LOCTEXT("PageLore", "Lore");
	case EJournalPage::Conversations:
		return LOCTEXT("PageConversations", "Conversations");
	}
	return FText::GetEmpty();
}

FJournalView WoRJournalScreen::BuildView(UGameInstance *GameInstance, EJournalPage Page, int32 Filter, const FJournalState &State,
										 const TArray<UStoryLinesData *> &Story)
{
	FJournalView View;
	switch (Page)
	{
	case EJournalPage::Quests:
		JournalScreenBuild::BuildQuests(GameInstance, Filter, State, View);
		break;
	case EJournalPage::Memories:
		JournalScreenBuild::BuildMemories(GameInstance, Filter, View);
		break;
	case EJournalPage::Lore:
		JournalScreenBuild::BuildLore(GameInstance, Filter, View);
		break;
	case EJournalPage::Conversations:
		JournalScreenBuild::BuildConversations(GameInstance, Filter, State, Story, View);
		break;
	}
	return View;
}

bool WoRJournalScreen::ToggleSecret(const FJournalRow &Row, FJournalState &State)
{
	if (!Row.IsSecret())
	{
		return false;
	}
	const bool bNowShown = !State.ShownSecrets.Contains(Row.Id);
	if (bNowShown)
	{
		State.ShownSecrets.Add(Row.Id);
	}
	else
	{
		State.ShownSecrets.Remove(Row.Id);
	}
	UE_LOG(LogWoRUI, Log, TEXT("[Journal] %s %s"), *Row.Id.ToString(), bNowShown ? TEXT("shown until the journal closes") : TEXT("hidden again"));
	return true;
}

bool WoRJournalScreen::HideAllSecrets(FJournalState &State)
{
	if (State.ShownSecrets.IsEmpty())
	{
		return false;
	}
	State.ShownSecrets.Reset();
	UE_LOG(LogWoRUI, Log, TEXT("[Journal] every secret quest hidden again"));
	return true;
}

bool WoRJournalScreen::PlayNextLine(UGameInstance *GameInstance, const FJournalRow &Row, FJournalState &State,
									const TArray<UStoryLinesData *> &Story)
{
	UMemoryJournalSubsystem *Journal = JournalScreenBuild::Find<UMemoryJournalSubsystem>(GameInstance);
	if (!Journal || Row.SelectVerb.IsEmpty())
	{
		return false;
	}
	for (const FStoryConversation *Talk : StoryUnlocks::GetUnlockedConversations(Story, *Journal))
	{
		if (Talk->ConversationId != Row.Id)
		{
			continue;
		}
		State.TalkLines = State.TalkId == Row.Id ? State.TalkLines + 1 : 1;
		State.TalkId = Row.Id;
		if (State.TalkLines >= Talk->Lines.Num())
		{
			Journal->MarkConversationPlayed(Row.Id);
			UE_LOG(LogWoRUI, Log, TEXT("[Journal] Conversation %s played"), *Row.Id.ToString());
		}
		return true;
	}
	return false;
}

bool WoRJournalScreen::ToggleTrack(UGameInstance *GameInstance, const FJournalRow &Row)
{
	UHiddenQuestSubsystem *Quests = JournalScreenBuild::Find<UHiddenQuestSubsystem>(GameInstance);
	if (!Quests || !Row.bQuest || Row.SelectVerb.IsEmpty())
	{
		return false;
	}
	return Quests->TrackQuest(Quests->GetTrackedQuest() == Row.Id ? NAME_None : Row.Id);
}

bool WoRJournalScreen::MarkOpened(UGameInstance *GameInstance, const FJournalRow &Row)
{
	UMemoryJournalSubsystem *Journal = JournalScreenBuild::Find<UMemoryJournalSubsystem>(GameInstance);
	return Row.bNew && Journal && Journal->MarkOpened(Row.Id);
}

FString WoRJournalScreen::GetJournalString(UGameInstance *GameInstance, const TArray<UStoryLinesData *> &Story)
{
	FString Out = TEXT("[Journal]");
	const FJournalState Closed; // as the journal opens: secret quests masked, no talk playing
	for (int32 PageIndex = 0; PageIndex < PAGE_COUNT; ++PageIndex)
	{
		const EJournalPage Page = static_cast<EJournalPage>(PageIndex);
		const int32 Filters = FMath::Max(BuildView(GameInstance, Page, 0, Closed, Story).Filters.Num(), 1);
		// The first sub-tab of Quests and Memories is everything; Conversations has no such tab.
		for (int32 Filter = 0; Filter < (Page == EJournalPage::Conversations ? Filters : 1); ++Filter)
		{
			const FJournalView View = BuildView(GameInstance, Page, Filter, Closed, Story);
			Out += FString::Printf(TEXT("\n  %s%s:"), *GetPageName(Page).ToString(),
								   View.Filters.IsValidIndex(Filter) ? *FString::Printf(TEXT(" (%s)"), *View.Filters[Filter].ToString()) : TEXT(""));
			for (const FJournalRow &Row : View.Rows)
			{
				Out += FString::Printf(TEXT("\n    %s%s%s"), Row.Tag.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("[%s] "), *Row.Tag.ToString()),
									   *Row.Title.ToString(), Row.Subtitle.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(": %s"), *Row.Subtitle.ToString()));
			}
			if (!View.ListNote.IsEmpty())
			{
				Out += FString::Printf(TEXT("\n    %s"), *View.ListNote.ToString());
			}
		}
	}
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GJournalPrintCommand(
	TEXT("wor.UI.PrintJournal"),
	TEXT("DEBUG (B-463): every journal page's rows, as the journal opens them (secret quests masked)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UE_LOG(LogWoRUI, Display, TEXT("%s"), *WoRJournalScreen::GetJournalString(World ? World->GetGameInstance() : nullptr, StoryUnlocks::GatherStoryData()));
	}));
#endif

#undef LOCTEXT_NAMESPACE
