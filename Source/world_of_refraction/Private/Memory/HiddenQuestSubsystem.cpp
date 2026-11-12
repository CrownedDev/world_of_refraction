// HiddenQuestSubsystem.cpp

#include "Memory/HiddenQuestSubsystem.h"

#include "Conditions/WoRConditions.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Memory/CurseSubsystem.h"
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

/** The quest notices, all in one place. They are private (this player's screen only); how a hidden quest starts is
 *  never hinted (D-100). */
namespace HiddenQuestNotice
{
	// B-397 (D-171, replaces B-377's vague "Something has begun." / "Something has moved forward."): a found hidden quest
	// is named, then its steps show their next goal as main and side quests do. TODO: Crown's text (placeholder).
	static FText Discovered(const UHiddenQuestData &Quest)
	{
		return FText::Format(LOCTEXT("HiddenDiscovered", "Hidden quest discovered: {0}"), Quest.GetDisplayTitle());
	}

	// B-388: a main or side quest is visible, so its notices name it and its next goal. TODO: Crown's text (placeholders).
	static FText VisibleStarted(const UHiddenQuestData &Quest)
	{
		return FText::Format(LOCTEXT("VisibleStarted", "New quest: {0}"), Quest.GetDisplayTitle());
	}
	static FText VisibleStepDone(const UHiddenQuestData &Quest, int32 StepsDone)
	{
		const FText Goal = Quest.GetGoalAfter(StepsDone);
		return Goal.IsEmpty() ? FText::Format(LOCTEXT("VisibleStepDoneNoGoal", "{0}: next step"), Quest.GetDisplayTitle()) // goal not authored yet
							  : FText::Format(LOCTEXT("VisibleStepDone", "{0}: {1}"), Quest.GetDisplayTitle(), Goal);
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
	Progress.Add(Quest->GetQuestId()).FoundOrder = NextFoundOrder++; // Q-397: the journal lists newest first
	KnownQuests.Add(Quest->GetQuestId(), const_cast<UHiddenQuestData *>(Quest)); // B-388: the journal shows its title and goal
	JustStarted.Add(Quest->GetQuestId()); // Q-377: the event that started it does not also do its first step
	ReportToHost(); // B-377b: a client's host checks its steps
	ShowNotice(Quest->IsVisible() ? HiddenQuestNotice::VisibleStarted(*Quest) : HiddenQuestNotice::Discovered(*Quest));
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
	UCurseSubsystem *Curses = GetGameInstance()->GetSubsystem<UCurseSubsystem>();
	if (Curses)
	{
		RegainCurses(*Curses, Context, Quests); // before this event moves any step on (a step done now gives it anyway)
	}
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
	GiveStepRewards(Quest, Step, GetGameInstance()->GetSubsystem<UCurseSubsystem>()); // Q-378: on this player's own machine
	UE_LOG(LogWoR, Log, TEXT("[HiddenQuest] %s: step %d of %d done"), *Quest.GetQuestId().ToString(), Entry->StepsDone, Quest.Steps.Num());
	if (Entry->StepsDone >= Quest.Steps.Num())
	{
		Finish(Quest); // may start another quest (Progress grows): Entry is not used after this
	}
	else
	{
		// B-388: the next goal; B-397 (D-171): a found hidden quest's too (its later steps stay unnamed until reached).
		ShowNotice(HiddenQuestNotice::VisibleStepDone(Quest, Entry->StepsDone));
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
	// Merge fix: the same quest list a start reads (QuestsForStarts when set, else the catalogue).
	TArray<UHiddenQuestData *> Quests(QuestsForStarts);
	if (Quests.IsEmpty())
	{
		Quests = GetAllQuests();
	}
	for (const UHiddenQuestData *Quest : Quests)
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
	FWoRConditionContext Context = WoRConditions::MakeContext(Event, Journal);
	int32 Started = 0;
	// B-381 activations: each item's quests are checked with that item as BoundItem (a WornItemState condition reads it).
	auto MetFor = [&Context](const FBoundQuest &Bound, const FWornItemState *Item)
	{
		Context.BoundItem = Item;
		const bool bMet = WoRConditions::IsMet(Bound.Conditions, Context);
		Context.BoundItem = nullptr;
		return bMet;
	};
	auto TryStart = [&](const TArray<FBoundQuest> &BoundQuests, const FWornItemState *Item)
	{
		for (const FBoundQuest &Bound : BoundQuests)
		{
			if (Bound.Quest && !IsStarted(Bound.Quest->GetQuestId()) && MetFor(Bound, Item) && StartQuest(Bound.Quest))
			{
				++Started;
			}
		}
	};
	for (const FMemoryEventMember &Member : Event.Party)
	{
		// Every worn item with its state; an event built without states (tests, older callers) reads Worn's tiers only.
		TArray<FWornItemState> Items = Member.WornStates;
		for (const TPair<const UEquipmentDataBase *, EItemTier> &Worn : Member.Worn)
		{
			if (Worn.Key && !Items.ContainsByPredicate([&Worn](const FWornItemState &Item) { return Item.Item == Worn.Key; }))
			{
				FWornItemState &Item = Items.AddDefaulted_GetRef();
				Item.Item = Worn.Key;
				Item.Tier = Worn.Value;
				Item.UpgradeSteps = MemoryUnlock::GetUpgradeSteps(*Worn.Key, Worn.Value);
			}
		}
		if (AWoRPlayerState *Friend = MemoryUnlock::FindRemotePlayerOf(GetGameInstance(), Member.Character))
		{
			// B-377b (Q-235d): a friend's worn gear starts the quest on their machine.
			auto TellFriend = [&](const TArray<FBoundQuest> &BoundQuests, const FWornItemState *Item)
			{
				for (const FBoundQuest &Bound : BoundQuests)
				{
					if (Bound.Quest && !Friend->GetReportedQuests().Contains(Bound.Quest->GetQuestId()) &&
						!Friend->WasQuestOffered(Bound.Quest->GetQuestId()) && MetFor(Bound, Item))
					{
						Friend->MarkQuestOffered(Bound.Quest->GetQuestId());
						Friend->Client_StartQuest(Bound.Quest);
					}
				}
			};
			for (const FWornItemState &Item : Items)
			{
				if (Item.Item) // review: a caller-filled state can lack its item
				{
					TellFriend(Item.Item->BoundQuests, &Item);
				}
			}
			if (Member.Evolution)
			{
				TellFriend(Member.Evolution->BoundQuests, nullptr);
			}
			continue;
		}
		for (const FWornItemState &Item : Items)
		{
			if (Item.Item) // review: a caller-filled state can lack its item
			{
				TryStart(Item.Item->BoundQuests, &Item);
			}
		}
		if (Member.Evolution)
		{
			TryStart(Member.Evolution->BoundQuests, nullptr); // its own state is EvolutionEquipped's (not a bound item)
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
	ShowNotice(HiddenQuestNotice::VisibleFinished(Quest)); // B-388; B-397: a found hidden quest too
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
		ShowNotice(FText::Format(LOCTEXT("QuestEnded", "New memory: {0}"), Quest.EndMemory->Title)); // only once earned (D-171)
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
		Out.FoundOrder = Entry.Value.FoundOrder;
	}
	return Saved;
}

void UHiddenQuestSubsystem::Restore(const FSavedHiddenQuests &Saved)
{
	Progress.Reset();
	NextFoundOrder = 1;
	for (const FSavedQuestProgress &In : Saved.Quests)
	{
		if (!In.QuestId.IsNone())
		{
			FHiddenQuestProgress &Entry = Progress.Add(In.QuestId);
			Entry.StepsDone = FMath::Max(In.StepsDone, 0);
			Entry.bFinished = In.bFinished;
			Entry.FoundOrder = FMath::Max(In.FoundOrder, 0);
			NextFoundOrder = FMath::Max(NextFoundOrder, Entry.FoundOrder + 1); // a quest found after loading is the newest
		}
	}
}

const UHiddenQuestData *UHiddenQuestSubsystem::FindKnownQuest(FName QuestId) const
{
	const TObjectPtr<UHiddenQuestData> *Found = KnownQuests.Find(QuestId);
	return Found ? Found->Get() : nullptr;
}

void UHiddenQuestSubsystem::ShowNotice(const FText &Notice)
{
	LastNotice = Notice;
	ATrialMapFeature::ShowMessage(Notice); // this machine's screen: a co-op friend's quests run (and notify) on theirs
}

namespace HiddenQuestJournal
{
	/** The journal lists main quests first, then side, then hidden (B-397, Q-397); newest found first within each. */
	constexpr int32 JOURNAL_RANK_MAIN = 0;
	constexpr int32 JOURNAL_RANK_SIDE = 1;
	constexpr int32 JOURNAL_RANK_HIDDEN = 2;

	static int32 GetListRank(EQuestKind Kind)
	{
		switch (Kind)
		{
		case EQuestKind::Main:
			return JOURNAL_RANK_MAIN;
		case EQuestKind::Side:
			return JOURNAL_RANK_SIDE;
		case EQuestKind::Hidden:
			return JOURNAL_RANK_HIDDEN;
		}
		return JOURNAL_RANK_HIDDEN;
	}
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
		if (!Quest)
		{
			continue; // B-397 (D-171): a started hidden quest is listed; an unstarted one never is (not in Progress)
		}
		FVisibleQuest &Row = Out.AddDefaulted_GetRef();
		Row.QuestId = Entry.Key;
		Row.Kind = Quest->Kind;
		Row.Title = Quest->GetDisplayTitle();
		Row.bFinished = Entry.Value.bFinished;
		Row.FoundOrder = Entry.Value.FoundOrder;
		Row.Goal = Row.bFinished ? FText::GetEmpty() : Quest->GetGoalAfter(Entry.Value.StepsDone);
	}
	Out.Sort([](const FVisibleQuest &A, const FVisibleQuest &B)
	{
		if (A.Kind != B.Kind)
		{
			return HiddenQuestJournal::GetListRank(A.Kind) < HiddenQuestJournal::GetListRank(B.Kind); // main, side, hidden
		}
		if (A.bFinished != B.bFinished)
		{
			return !A.bFinished;
		}
		if (A.FoundOrder != B.FoundOrder)
		{
			return A.FoundOrder > B.FoundOrder; // Q-397 (Crown): the newest found first
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
		// B-397: a found hidden quest shows its title and current goal like the others.
		const FString Visible = Quest
									? FString::Printf(TEXT(" [%s \"%s\"%s]"), *UEnum::GetDisplayValueAsText(Quest->Kind).ToString(),
													  *Quest->GetDisplayTitle().ToString(),
													  Entry.Value.bFinished ? TEXT("") : *FString::Printf(TEXT(", goal: %s"), *Quest->GetGoalAfter(Entry.Value.StepsDone).ToString()))
									: FString();
		Lines.Add(FString::Printf(TEXT("%s%s: %d step(s) done%s, step in progress met %d time(s) this run, found #%d"), *Entry.Key.ToString(),
								  *Visible, Entry.Value.StepsDone, Entry.Value.bFinished ? TEXT(", finished") : TEXT(""), Entry.Value.RunCount,
								  Entry.Value.FoundOrder));
	}
	Lines.Sort();
	if (!LastNotice.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("last notice: %s"), *LastNotice.ToString()));
	}
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
	TEXT("B-377: this player's quests: steps done, finished, the step in progress's count this run; each quest's kind, title and current goal (B-388, B-397); the last notice."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		const UHiddenQuestSubsystem *Quests = GameInstance ? GameInstance->GetSubsystem<UHiddenQuestSubsystem>() : nullptr;
		UE_LOG(LogWoR, Display, TEXT("[HiddenQuest]\n%s"), Quests ? *Quests->GetQuestsString() : TEXT("no quest subsystem"));
	}));
#endif
