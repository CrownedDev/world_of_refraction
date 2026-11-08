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
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "HiddenQuest"

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
		Quests = UHiddenQuestData::GatherAll();
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
	ATrialMapFeature::ShowMessage(LOCTEXT("QuestStarted", "Something has begun.")); // private; no hint of what (D-100)
	UE_LOG(LogWoR, Log, TEXT("[HiddenQuest] %s started (%d step(s))"), *Quest->GetQuestId().ToString(), Quest->Steps.Num());
	return true;
}

int32 UHiddenQuestSubsystem::CheckEvent(const FMemoryEvent &Event, const TArray<UHiddenQuestData *> &Quests)
{
	const UMemoryJournalSubsystem *Journal = GetGameInstance()->GetSubsystem<UMemoryJournalSubsystem>();
	const FWoRConditionContext Context = WoRConditions::MakeContext(Event, Journal);
	int32 StepsDoneNow = 0;
	for (const UHiddenQuestData *Quest : Quests)
	{
		FHiddenQuestProgress *Entry = Quest ? Progress.Find(Quest->GetQuestId()) : nullptr;
		if (!Entry || Entry->bFinished || !Quest->Steps.IsValidIndex(Entry->StepsDone))
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
			Entry->bFinished = true;
			MemoryUnlock::Unlock(GetGameInstance(), Quest->EndMemory, EMemorySource::HiddenQuest);
			UE_LOG(LogWoR, Log, TEXT("[HiddenQuest] %s finished"), *Quest->GetQuestId().ToString());
		}
		else
		{
			ATrialMapFeature::ShowMessage(LOCTEXT("StepDone", "Something has moved forward.")); // no next-step text (D-100)
		}
	}
	return StepsDoneNow;
}

int32 UHiddenQuestSubsystem::CheckEvent(const FMemoryEvent &Event)
{
	return Progress.IsEmpty() ? 0 : CheckEvent(Event, UHiddenQuestData::GatherAll());
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
