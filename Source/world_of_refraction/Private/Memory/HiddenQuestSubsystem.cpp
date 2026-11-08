// HiddenQuestSubsystem.cpp

#include "Memory/HiddenQuestSubsystem.h"

#include "Conditions/WoRConditions.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Memory/HiddenQuestData.h"
#include "Equipment/EquipmentDataBase.h"
#include "Equipment/Crystals/EvolutionItemData.h"
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
	ReportToHost(); // B-377b: a client's host checks its steps
	ATrialMapFeature::ShowMessage(LOCTEXT("QuestStarted", "Something has begun.")); // private; no hint of what (D-100)
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
	// B-377b: in co-op this player's steps read their own part of the event (Q-235d), with the team's memories.
	TArray<UCharacterData *> FriendsCharacters;
	TArray<const TSet<FName> *> FriendsKnown;
	for (const AWoRPlayerState *Friend : MemoryUnlock::GetRemotePlayers(GetGameInstance()))
	{
		FriendsCharacters.Append(Friend->GetOwnCharacters());
		FriendsKnown.Add(&Friend->GetKnownMemoryIds());
	}
	const FMemoryEvent OwnPart = FriendsCharacters.IsEmpty() ? Event : MemoryConditions::FilterParty(Event, FriendsCharacters, /*bKeep=*/false);
	const TSet<FName> Team = FriendsKnown.IsEmpty() ? TSet<FName>() : MemoryConditions::GetTeamMemoryIds(Journal, FriendsKnown);
	FWoRConditionContext Context = WoRConditions::MakeContext(OwnPart, Journal);
	Context.KnownMemoryIds = FriendsKnown.IsEmpty() ? nullptr : &Team;
	int32 StepsDoneNow = 0;
	// Quests started by this event (or since the last one) wait for the next event; one this loop starts waits too.
	const TSet<FName> StartedJustNow = MoveTemp(JustStarted);
	JustStarted.Reset();
	for (const UHiddenQuestData *Quest : Quests)
	{
		const FHiddenQuestProgress *Entry = Quest ? Progress.Find(Quest->GetQuestId()) : nullptr;
		if (!Entry || Entry->bFinished || !Quest->Steps.IsValidIndex(Entry->StepsDone) || StartedJustNow.Contains(Quest->GetQuestId()) ||
			JustStarted.Contains(Quest->GetQuestId()))
		{
			continue;
		}
		if (WoRConditions::IsMet(Quest->Steps[Entry->StepsDone].Conditions, Context) && CountStepMet(*Quest))
		{
			++StepsDoneNow;
		}
	}
	CheckFriendsQuests(Event, Quests, Team);
	return StepsDoneNow;
}

bool UHiddenQuestSubsystem::CountStepMet(const UHiddenQuestData &Quest)
{
	FHiddenQuestProgress *Entry = Progress.Find(Quest.GetQuestId());
	if (!Entry || Entry->bFinished || !Quest.Steps.IsValidIndex(Entry->StepsDone))
	{
		return false;
	}
	const FHiddenQuestStep &Step = Quest.Steps[Entry->StepsDone];
	if (++Entry->RunCount < FMath::Max(Step.TimesInOneRun, 1))
	{
		return false;
	}
	++Entry->StepsDone;
	Entry->RunCount = 0;
	UE_LOG(LogWoR, Log, TEXT("[HiddenQuest] %s: step %d of %d done"), *Quest.GetQuestId().ToString(), Entry->StepsDone, Quest.Steps.Num());
	if (Entry->StepsDone >= Quest.Steps.Num())
	{
		Finish(Quest); // may start another quest (Progress grows): Entry is not used after this
	}
	else
	{
		ATrialMapFeature::ShowMessage(LOCTEXT("StepDone", "Something has moved forward.")); // no next-step text (D-100)
	}
	ReportToHost();
	return true;
}

void UHiddenQuestSubsystem::CheckFriendsQuests(const FMemoryEvent &Event, const TArray<UHiddenQuestData *> &Quests, const TSet<FName> &Team)
{
	const UMemoryJournalSubsystem *Journal = GetGameInstance()->GetSubsystem<UMemoryJournalSubsystem>();
	for (AWoRPlayerState *Friend : MemoryUnlock::GetRemotePlayers(GetGameInstance()))
	{
		if (Friend->GetReportedQuests().IsEmpty())
		{
			continue;
		}
		const FMemoryEvent FriendsPart = MemoryConditions::FilterParty(Event, Friend->GetOwnCharacters(), /*bKeep=*/true);
		FWoRConditionContext Context = WoRConditions::MakeContext(FriendsPart, Journal);
		Context.KnownMemoryIds = &Team;
		Context.HeldCurses = nullptr; // not this machine's (their own curses and counters stay on their machine)
		Context.Counters = nullptr;
		for (const TPair<FName, int32> &Reported : Friend->GetReportedQuests())
		{
			const UHiddenQuestData *const *Found = Quests.FindByPredicate([&Reported](const UHiddenQuestData *Quest)
			{
				return Quest && Quest->GetQuestId() == Reported.Key;
			});
			const UHiddenQuestData *Quest = Found ? *Found : nullptr;
			if (Quest && Quest->Steps.IsValidIndex(Reported.Value) && WoRConditions::IsMet(Quest->Steps[Reported.Value].Conditions, Context))
			{
				Friend->Client_QuestStepMet(Reported.Key, Reported.Value); // their machine counts it (for that step) and reports back
			}
		}
	}
}

void UHiddenQuestSubsystem::ApplyStepMetFromHost(FName QuestId, int32 StepIndex)
{
	// Review: the host checked the step it last heard of; a late message for a step already done is not counted on the next.
	const FHiddenQuestProgress *Entry = Progress.Find(QuestId);
	if (!Entry || Entry->StepsDone != StepIndex)
	{
		return;
	}
	for (const UHiddenQuestData *Quest : GetAllQuests())
	{
		if (Quest && Quest->GetQuestId() == QuestId)
		{
			// The host only checks quests this player reported before the event, so the starting event never counts.
			JustStarted.Remove(QuestId);
			CountStepMet(*Quest);
			return;
		}
	}
}

void UHiddenQuestSubsystem::ReportToHost() const
{
	const UWorld *World = GetGameInstance()->GetWorld();
	if (!World || World->GetNetMode() != NM_Client)
	{
		return;
	}
	const APlayerController *PC = World->GetFirstPlayerController();
	if (AWoRPlayerState *Self = PC ? PC->GetPlayerState<AWoRPlayerState>() : nullptr)
	{
		Self->ServerReportQuests(Capture().Quests);
	}
}


int32 UHiddenQuestSubsystem::CheckEvent(const FMemoryEvent &Event)
{
	StartGearQuests(Event); // started now: their steps wait for the next event (Q-377)
	// Review: a friend's quests move even when this player has none started.
	const bool bNothingToCheck = Progress.IsEmpty() && MemoryUnlock::GetRemotePlayers(GetGameInstance()).IsEmpty();
	return bNothingToCheck ? 0 : CheckEvent(Event, GetAllQuests());
}

int32 UHiddenQuestSubsystem::StartGearQuests(const FMemoryEvent &Event)
{
	const UMemoryJournalSubsystem *Journal = GetGameInstance()->GetSubsystem<UMemoryJournalSubsystem>();
	const FWoRConditionContext Context = WoRConditions::MakeContext(Event, Journal);
	int32 Started = 0;
	auto TryStart = [&](const TArray<FBoundQuest> &BoundQuests)
	{
		for (const FBoundQuest &Bound : BoundQuests)
		{
			if (Bound.Quest && !IsStarted(Bound.Quest->GetQuestId()) && WoRConditions::IsMet(Bound.Conditions, Context) && StartQuest(Bound.Quest))
			{
				++Started;
			}
		}
	};
	for (const FMemoryEventMember &Member : Event.Party)
	{
		if (AWoRPlayerState *Friend = MemoryUnlock::FindRemotePlayerOf(GetGameInstance(), Member.Character))
		{
			// B-377b (Q-235d): a friend's worn gear starts the quest on their machine.
			auto TellFriend = [&](const TArray<FBoundQuest> &BoundQuests)
			{
				for (const FBoundQuest &Bound : BoundQuests)
				{
					if (Bound.Quest && !Friend->GetReportedQuests().Contains(Bound.Quest->GetQuestId()) &&
						!Friend->WasQuestOffered(Bound.Quest->GetQuestId()) && WoRConditions::IsMet(Bound.Conditions, Context))
					{
						Friend->MarkQuestOffered(Bound.Quest->GetQuestId());
						Friend->Client_StartQuest(Bound.Quest);
					}
				}
			};
			for (const TPair<const UEquipmentDataBase *, EItemTier> &Worn : Member.Worn)
			{
				if (Worn.Key)
				{
					TellFriend(Worn.Key->BoundQuests);
				}
			}
			if (Member.Evolution)
			{
				TellFriend(Member.Evolution->BoundQuests);
			}
			continue;
		}
		for (const TPair<const UEquipmentDataBase *, EItemTier> &Worn : Member.Worn)
		{
			if (Worn.Key)
			{
				TryStart(Worn.Key->BoundQuests);
			}
		}
		if (Member.Evolution)
		{
			TryStart(Member.Evolution->BoundQuests);
		}
	}
	return Started;
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
