// HiddenQuestSubsystem.cpp

#include "Memory/HiddenQuestSubsystem.h"

#include "Conditions/WoRConditions.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Memory/CurseSubsystem.h"
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
	JustStarted.Add(Quest->GetQuestId()); // Q-377: the event that started it does not also do its first step
	ATrialMapFeature::ShowMessage(HiddenQuestNotice::Started());
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
	UCurseSubsystem *Curses = GetGameInstance()->GetSubsystem<UCurseSubsystem>();
	if (Curses)
	{
		RegainCurses(*Curses, Context, Quests); // before this event moves any step on (a step done now gives it anyway)
	}
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
		GiveStepRewards(*Quest, Step, Curses);
		UE_LOG(LogWoR, Log, TEXT("[HiddenQuest] %s: step %d of %d done"), *Quest->GetQuestId().ToString(), Entry->StepsDone, Quest->Steps.Num());
		if (Entry->StepsDone >= Quest->Steps.Num())
		{
			Finish(*Quest); // may start another quest (Progress grows): Entry is not used after this
		}
		else
		{
			ATrialMapFeature::ShowMessage(HiddenQuestNotice::StepDone());
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

void UHiddenQuestSubsystem::GiveStepRewards(const UHiddenQuestData &Quest, const FHiddenQuestStep &Step, UCurseSubsystem *Curses)
{
	for (const FMemoryReward &Reward : Step.Rewards)
	{
		if (Reward.Kind != EMemoryRewardKind::Curse && Reward.Kind != EMemoryRewardKind::RemoveCurse)
		{
			UE_LOG(LogWoR, Log, TEXT("[HiddenQuest] %s step reward: %s (logged, not granted yet)"), *Quest.GetQuestId().ToString(),
				   *UMemoryData::GetRewardString(Reward));
		}
	}
	if (Curses)
	{
		Curses->ApplyRewards(Step.Rewards); // Q-378: a quest step gives (or lifts) a curse
	}
}

int32 UHiddenQuestSubsystem::RegainCurses(UCurseSubsystem &Curses, const FWoRConditionContext &Context, const TArray<UHiddenQuestData *> &Quests)
{
	// Q-378 (Crown, 2026-11-08): a curse a done step (or a finished quest's end memory) gave, cleared since, comes back
	// when that step's conditions are met again. One meeting is enough (TimesInOneRun is the first time's count).
	int32 Regained = 0;
	for (const UHiddenQuestData *Quest : Quests)
	{
		const FHiddenQuestProgress *Entry = Quest ? Progress.Find(Quest->GetQuestId()) : nullptr;
		if (!Entry)
		{
			continue;
		}
		const int32 Done = FMath::Min(Entry->StepsDone, Quest->Steps.Num());
		for (int32 Index = 0; Index < Done; ++Index)
		{
			const FHiddenQuestStep &Step = Quest->Steps[Index];
			const bool bGivesEndCurse = Entry->bFinished && Index == Quest->Steps.Num() - 1 && Quest->EndMemory &&
										Curses.CouldRegain(Quest->EndMemory->Rewards);
			if ((!Curses.CouldRegain(Step.Rewards) && !bGivesEndCurse) || !WoRConditions::IsMet(Step.Conditions, Context))
			{
				continue;
			}
			const int32 Back = Curses.RegainFrom(Step.Rewards) + (bGivesEndCurse ? Curses.RegainFrom(Quest->EndMemory->Rewards) : 0);
			Regained += Back;
			UE_LOG(LogWoR, Log, TEXT("[HiddenQuest] %s step %d met again: %d curse(s) back"), *Quest->GetQuestId().ToString(), Index + 1, Back);
		}
	}
	return Regained;
}

void UHiddenQuestSubsystem::Finish(const UHiddenQuestData &Quest)
{
	if (FHiddenQuestProgress *Entry = Progress.Find(Quest.GetQuestId()))
	{
		Entry->bFinished = true;
	}
	UE_LOG(LogWoR, Log, TEXT("[HiddenQuest] %s finished"), *Quest.GetQuestId().ToString());
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

FString UHiddenQuestSubsystem::GetQuestsString() const
{
	if (Progress.IsEmpty())
	{
		return TEXT("no hidden quests started");
	}
	TArray<FString> Lines;
	for (const TPair<FName, FHiddenQuestProgress> &Entry : Progress)
	{
		Lines.Add(FString::Printf(TEXT("%s: %d step(s) done%s, step in progress met %d time(s) this run"), *Entry.Key.ToString(),
								  Entry.Value.StepsDone, Entry.Value.bFinished ? TEXT(", finished") : TEXT(""), Entry.Value.RunCount));
	}
	Lines.Sort();
	return FString::Join(Lines, TEXT("\n"));
}

#undef LOCTEXT_NAMESPACE

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GQuestPrintCommand(
	TEXT("wor.Quest.Print"),
	TEXT("B-377: this player's hidden quests: steps done, finished, and the step in progress's count this run."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		const UHiddenQuestSubsystem *Quests = GameInstance ? GameInstance->GetSubsystem<UHiddenQuestSubsystem>() : nullptr;
		UE_LOG(LogWoR, Display, TEXT("[HiddenQuest]\n%s"), Quests ? *Quests->GetQuestsString() : TEXT("no quest subsystem"));
	}));
#endif
