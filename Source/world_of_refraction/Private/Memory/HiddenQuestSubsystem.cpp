// HiddenQuestSubsystem.cpp

#include "Memory/HiddenQuestSubsystem.h"

#include "Conditions/WoRConditions.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Memory/HiddenQuestData.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryUnlock.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRPlayerState.h"
#include "Trial/TrialMapFeature.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "HiddenQuest"

/** The quest notices, all in one place. They are private: no hint of which quest or what comes next (D-100). */
namespace HiddenQuestNotice
{
	// TODO: Crown's text (placeholders, Crown writes lore-flavoured lines later; Q-377).
	static FText Started() { return LOCTEXT("QuestStarted", "Something has begun."); }
	// TODO: Crown's text (placeholder; Q-377).
	static FText StepDone() { return LOCTEXT("StepDone", "Something has moved forward."); }

	// B-388: a main or side quest is visible, so its notices name it and its next goal. TODO: Crown's text (placeholders).
	static FText VisibleStarted(const UHiddenQuestData &Quest)
	{
		return FText::Format(LOCTEXT("VisibleStarted", "New quest: {0}"), Quest.GetDisplayTitle());
	}
	static FText VisibleStepDone(const UHiddenQuestData &Quest, int32 StepsDone)
	{
		return FText::Format(LOCTEXT("VisibleStepDone", "{0}: {1}"), Quest.GetDisplayTitle(), Quest.GetGoalAfter(StepsDone));
	}
	static FText VisibleFinished(const UHiddenQuestData &Quest)
	{
		return FText::Format(LOCTEXT("VisibleFinished", "Quest complete: {0}"), Quest.GetDisplayTitle());
	}
}

void UHiddenQuestSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	if (UMemoryJournalSubsystem *Journal = Collection.InitializeDependency<UMemoryJournalSubsystem>())
	{
		Journal->OnMemoryRecorded.AddDynamic(this, &UHiddenQuestSubsystem::HandleMemoryRecorded);
	}
}

void UHiddenQuestSubsystem::Deinitialize()
{
	if (UMemoryJournalSubsystem *Journal = GetGameInstance()->GetSubsystem<UMemoryJournalSubsystem>())
	{
		Journal->OnMemoryRecorded.RemoveAll(this);
	}
	Super::Deinitialize();
}

EMemoryDepth UHiddenQuestSubsystem::GetDepthForPlayer(const UMemoryData *Memory) const
{
	if (!Memory || Memory->CharacterDepth.IsEmpty())
	{
		return EMemoryDepth::None;
	}
	TArray<UCharacterData *> Characters;
	const UWorld *World = GetGameInstance()->GetWorld();
	const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
	if (const AWoRPlayerState *Player = PC ? PC->GetPlayerState<AWoRPlayerState>() : nullptr)
	{
		Characters = Player->GetOwnCharacters();
	}
	if (Characters.IsEmpty())
	{
		const UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>();
		if (const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr)
		{
			for (const FPartyMember &Member : Party->Members)
			{
				Characters.AddUnique(Member.CharacterData);
			}
		}
	}
	EMemoryDepth Deepest = EMemoryDepth::None;
	for (const UCharacterData *Character : Characters)
	{
		const EMemoryDepth Depth = Memory->GetDepthFor(Character);
		Deepest = static_cast<uint8>(Depth) > static_cast<uint8>(Deepest) ? Depth : Deepest;
	}
	return Deepest;
}

void UHiddenQuestSubsystem::HandleMemoryRecorded(UMemoryData *Memory, EMemorySource /*Source*/)
{
	const EMemoryDepth Depth = GetDepthForPlayer(Memory);
	TArray<UHiddenQuestData *> Quests;
	for (UHiddenQuestData *Quest : QuestsForStarts)
	{
		Quests.Add(Quest);
	}
	if (Quests.IsEmpty())
	{
		Quests = GetAllQuests();
	}
	for (const UHiddenQuestData *Quest : Quests)
	{
		if (Quest && Quest->StartsFrom(Memory, Depth))
		{
			StartQuest(Quest);
		}
	}
}

void UHiddenQuestSubsystem::SetQuestsForStarts(const TArray<UHiddenQuestData *> &Quests)
{
	QuestsForStarts.Reset();
	for (UHiddenQuestData *Quest : Quests)
	{
		QuestsForStarts.Add(Quest);
	}
}

bool UHiddenQuestSubsystem::StartQuest(const UHiddenQuestData *Quest)
{
	if (!Quest || Progress.Contains(Quest->GetQuestId()))
	{
		return false;
	}
	Progress.Add(Quest->GetQuestId());
	KnownQuests.Add(Quest->GetQuestId(), const_cast<UHiddenQuestData *>(Quest)); // B-388: the journal shows its title and goal
	JustStarted.Add(Quest->GetQuestId()); // Q-377: the event that started it does not also do its first step
	ATrialMapFeature::ShowMessage(Quest->IsVisible() ? HiddenQuestNotice::VisibleStarted(*Quest) : HiddenQuestNotice::Started());
	UE_LOG(LogWoR, Log, TEXT("[HiddenQuest] %s started (%d step(s))"), *Quest->GetQuestId().ToString(), Quest->Steps.Num());
	if (Quest->Steps.IsEmpty())
	{
		UE_LOG(LogWoR, Warning, TEXT("[HiddenQuest] %s has no steps: finished at once"), *Quest->GetQuestId().ToString());
		Finish(*Quest);
	}
	return true;
}

int32 UHiddenQuestSubsystem::CheckEvent(const FMemoryEvent &Event, const TArray<UHiddenQuestData *> &Quests)
{
	const UMemoryJournalSubsystem *Journal = GetGameInstance()->GetSubsystem<UMemoryJournalSubsystem>();
	const FWoRConditionContext Context = WoRConditions::MakeContext(Event, Journal);
	int32 StepsDoneNow = 0;
	// Quests started by this event (or since the last one) wait for the next event; one this loop starts waits too.
	const TSet<FName> StartedJustNow = MoveTemp(JustStarted);
	JustStarted.Reset();
	for (const UHiddenQuestData *Quest : Quests)
	{
		FHiddenQuestProgress *Entry = Quest ? Progress.Find(Quest->GetQuestId()) : nullptr;
		if (!Entry || Entry->bFinished || !Quest->Steps.IsValidIndex(Entry->StepsDone) || StartedJustNow.Contains(Quest->GetQuestId()) ||
			JustStarted.Contains(Quest->GetQuestId()))
		{
			continue;
		}
		const FHiddenQuestStep &Step = Quest->Steps[Entry->StepsDone];
		if (!WoRConditions::IsMet(Step.Conditions, Context) || ++Entry->RunCount < FMath::Max(Step.TimesInOneRun, 1))
		{
			continue;
		}
		++Entry->StepsDone;
		Entry->RunCount = 0;
		++StepsDoneNow;
		UE_LOG(LogWoR, Log, TEXT("[HiddenQuest] %s: step %d of %d done"), *Quest->GetQuestId().ToString(), Entry->StepsDone, Quest->Steps.Num());
		if (Entry->StepsDone >= Quest->Steps.Num())
		{
			Finish(*Quest); // may start another quest (Progress grows): Entry is not used after this
		}
		else
		{
			ATrialMapFeature::ShowMessage(Quest->IsVisible() ? HiddenQuestNotice::VisibleStepDone(*Quest, Entry->StepsDone)
															 : HiddenQuestNotice::StepDone());
		}
	}
	return StepsDoneNow;
}

int32 UHiddenQuestSubsystem::CheckEvent(const FMemoryEvent &Event)
{
	return Progress.IsEmpty() ? 0 : CheckEvent(Event, GetAllQuests());
}

TArray<UHiddenQuestData *> UHiddenQuestSubsystem::GetAllQuests()
{
	if (!bGatheredQuests)
	{
		AllQuests.Reset();
		for (UHiddenQuestData *Quest : UHiddenQuestData::GatherAll())
		{
			AllQuests.Add(Quest);
			if (Quest)
			{
				KnownQuests.FindOrAdd(Quest->GetQuestId(), Quest);
			}
		}
		// Once per game instance: the registry load is not repeated per event. B-377 review: but not while the
		// registry is still scanning (an early memory would otherwise leave the list empty for the whole session).
		bGatheredQuests = UHiddenQuestData::IsCatalogueReady();
	}
	TArray<UHiddenQuestData *> Quests;
	for (UHiddenQuestData *Quest : AllQuests)
	{
		Quests.Add(Quest);
	}
	return Quests;
}

void UHiddenQuestSubsystem::Finish(const UHiddenQuestData &Quest)
{
	if (FHiddenQuestProgress *Entry = Progress.Find(Quest.GetQuestId()))
	{
		Entry->bFinished = true;
	}
	UE_LOG(LogWoR, Log, TEXT("[HiddenQuest] %s finished"), *Quest.GetQuestId().ToString());
	if (Quest.IsVisible())
	{
		ATrialMapFeature::ShowMessage(HiddenQuestNotice::VisibleFinished(Quest)); // B-388
	}
	if (!Quest.EndMemory)
	{
		UE_LOG(LogWoR, Warning, TEXT("[HiddenQuest] %s has no end memory"), *Quest.GetQuestId().ToString());
		return;
	}
	// Review: the end memory is this player's alone (they did the quest, Q-235d), never shared with friends, and not
	// refused on a client for whose character it is: recorded straight into this player's run and journal.
	UGameInstance *GI = GetGameInstance();
	if (UTrialRunSubsystem *TrialRun = GI->GetSubsystem<UTrialRunSubsystem>())
	{
		TrialRun->CollectMemory(Quest.EndMemory);
	}
	UMemoryJournalSubsystem *Journal = GI->GetSubsystem<UMemoryJournalSubsystem>();
	if (Journal && Journal->RecordMemory(Quest.EndMemory, EMemorySource::HiddenQuest))
	{
		ATrialMapFeature::ShowMessage(FText::Format(LOCTEXT("QuestEnded", "New memory: {0}"), Quest.EndMemory->Title));
	}
}

void UHiddenQuestSubsystem::EndRun()
{
	for (TPair<FName, FHiddenQuestProgress> &Entry : Progress)
	{
		Entry.Value.RunCount = 0; // only the step in progress resets (Crown, MemoryCatalogue §3.6)
	}
}

bool UHiddenQuestSubsystem::IsFinished(FName QuestId) const
{
	const FHiddenQuestProgress *Entry = Progress.Find(QuestId);
	return Entry && Entry->bFinished;
}

int32 UHiddenQuestSubsystem::GetStepsDone(FName QuestId) const
{
	const FHiddenQuestProgress *Entry = Progress.Find(QuestId);
	return Entry ? Entry->StepsDone : 0;
}

int32 UHiddenQuestSubsystem::GetRunCount(FName QuestId) const
{
	const FHiddenQuestProgress *Entry = Progress.Find(QuestId);
	return Entry ? Entry->RunCount : 0;
}

FSavedHiddenQuests UHiddenQuestSubsystem::Capture() const
{
	FSavedHiddenQuests Saved;
	for (const TPair<FName, FHiddenQuestProgress> &Entry : Progress)
	{
		FSavedQuestProgress &Out = Saved.Quests.AddDefaulted_GetRef();
		Out.QuestId = Entry.Key;
		Out.StepsDone = Entry.Value.StepsDone;
		Out.bFinished = Entry.Value.bFinished;
	}
	return Saved;
}

void UHiddenQuestSubsystem::Restore(const FSavedHiddenQuests &Saved)
{
	Progress.Reset();
	for (const FSavedQuestProgress &In : Saved.Quests)
	{
		if (!In.QuestId.IsNone())
		{
			FHiddenQuestProgress &Entry = Progress.Add(In.QuestId);
			Entry.StepsDone = FMath::Max(In.StepsDone, 0);
			Entry.bFinished = In.bFinished;
		}
	}
}

const UHiddenQuestData *UHiddenQuestSubsystem::FindKnownQuest(FName QuestId) const
{
	const TObjectPtr<UHiddenQuestData> *Found = KnownQuests.Find(QuestId);
	return Found ? Found->Get() : nullptr;
}

TArray<FVisibleQuest> UHiddenQuestSubsystem::GetVisibleQuests()
{
	// A quest restored from the save is not known until the assets are gathered (once per game instance).
	for (const TPair<FName, FHiddenQuestProgress> &Entry : Progress)
	{
		if (!FindKnownQuest(Entry.Key))
		{
			GetAllQuests();
			break;
		}
	}
	TArray<FVisibleQuest> Out;
	for (const TPair<FName, FHiddenQuestProgress> &Entry : Progress)
	{
		const UHiddenQuestData *Quest = FindKnownQuest(Entry.Key);
		if (!Quest || !Quest->IsVisible())
		{
			continue; // a hidden quest is never listed (D-100)
		}
		FVisibleQuest &Row = Out.AddDefaulted_GetRef();
		Row.QuestId = Entry.Key;
		Row.Kind = Quest->Kind;
		Row.Title = Quest->GetDisplayTitle();
		Row.bFinished = Entry.Value.bFinished;
		Row.Goal = Row.bFinished ? FText::GetEmpty() : Quest->GetGoalAfter(Entry.Value.StepsDone);
	}
	Out.Sort([](const FVisibleQuest &A, const FVisibleQuest &B)
	{
		if (A.Kind != B.Kind)
		{
			return static_cast<uint8>(A.Kind) < static_cast<uint8>(B.Kind); // Main before Side
		}
		if (A.bFinished != B.bFinished)
		{
			return !A.bFinished;
		}
		return A.QuestId.LexicalLess(B.QuestId);
	});
	return Out;
}

bool UHiddenQuestSubsystem::StartQuestById(FName QuestId)
{
	const UHiddenQuestData *Quest = FindKnownQuest(QuestId);
	if (!Quest)
	{
		GetAllQuests(); // fills KnownQuests from the asset registry
		Quest = FindKnownQuest(QuestId);
	}
	if (!Quest)
	{
		UE_LOG(LogWoR, Warning, TEXT("[HiddenQuest] no quest asset with id %s"), *QuestId.ToString());
		return false;
	}
	return StartQuest(Quest);
}

FString UHiddenQuestSubsystem::GetQuestsString() const
{
	if (Progress.IsEmpty())
	{
		return TEXT("no quests started");
	}
	TArray<FString> Lines;
	for (const TPair<FName, FHiddenQuestProgress> &Entry : Progress)
	{
		const UHiddenQuestData *Quest = FindKnownQuest(Entry.Key);
		const FString Visible = Quest && Quest->IsVisible()
									? FString::Printf(TEXT(" [%s \"%s\"%s]"), *UEnum::GetDisplayValueAsText(Quest->Kind).ToString(),
													  *Quest->GetDisplayTitle().ToString(),
													  Entry.Value.bFinished ? TEXT("") : *FString::Printf(TEXT(", goal: %s"), *Quest->GetGoalAfter(Entry.Value.StepsDone).ToString()))
									: FString();
		Lines.Add(FString::Printf(TEXT("%s%s: %d step(s) done%s, step in progress met %d time(s) this run"), *Entry.Key.ToString(), *Visible,
								  Entry.Value.StepsDone, Entry.Value.bFinished ? TEXT(", finished") : TEXT(""), Entry.Value.RunCount));
	}
	Lines.Sort();
	return FString::Join(Lines, TEXT("\n"));
}

#undef LOCTEXT_NAMESPACE

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GQuestGiveCommand(
	TEXT("wor.Quest.Give"),
	TEXT("B-388: give a quest by id (stands in for the story, V, a conversation or an NPC until they give quests)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		UHiddenQuestSubsystem *Quests = GameInstance ? GameInstance->GetSubsystem<UHiddenQuestSubsystem>() : nullptr;
		if (!Quests || Args.IsEmpty())
		{
			UE_LOG(LogWoR, Warning, TEXT("[HiddenQuest] usage: wor.Quest.Give <quest id>"));
			return;
		}
		UE_LOG(LogWoR, Display, TEXT("[HiddenQuest] give %s: %s"), *Args[0],
			   Quests->StartQuestById(FName(*Args[0])) ? TEXT("started") : TEXT("not started (unknown, or already started)"));
	}));

static FAutoConsoleCommandWithWorld GQuestPrintCommand(
	TEXT("wor.Quest.Print"),
	TEXT("B-377: this player's quests: steps done, finished, the step in progress's count this run; main and side quests with their title and goal (B-388)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		const UHiddenQuestSubsystem *Quests = GameInstance ? GameInstance->GetSubsystem<UHiddenQuestSubsystem>() : nullptr;
		UE_LOG(LogWoR, Display, TEXT("[HiddenQuest]\n%s"), Quests ? *Quests->GetQuestsString() : TEXT("no quest subsystem"));
	}));
#endif
