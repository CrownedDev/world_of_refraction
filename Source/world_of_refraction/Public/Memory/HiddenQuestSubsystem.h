// HiddenQuestSubsystem.h
// B-377 (MemoryCatalogue §3.6, Crown 2026-11-08): this player's hidden quest progress. A quest starts when the journal
// records one of its start memories at the right depth (UMemoryJournalSubsystem::OnMemoryRecorded); each memory event
// (MemoryConditions::CheckEvent) checks every started quest's current step; a step done is kept across runs (saved,
// FSavedHiddenQuests), and only the count of the step in progress resets at run end (EndRun, from ExitTrial). The last
// step unlocks the quest's end memory. Notices are private and say nothing of the next step (D-100).
// Debug: GetQuestsString; wor.Quest.Print.
// B-377b: a co-op friend's quests stay on their machine; it reports them to the host, which tells it when its own part of
// an event meets a step (CheckFriendsQuests / ApplyStepMetFromHost) or its worn gear starts one.
// B-397 (D-171): a hidden quest's start is never hinted; once started, this player alone is told ("Hidden quest
// discovered: <title>") and it lists in their journal with the current step's goal, like main and side quests. Each
// machine keeps its own player's quests and notices, so in co-op each player sees only their own.
// Q-397b (Crown): a hidden quest's notices are masked ("Hidden quest found. Open the journal to view it."), and the
// journal masks it as "Secret Quest <n>" until shown (UMemoryJournalWidget::ToggleQuestShown). D-196: the player reads
// "Secret quest" everywhere; the code names stay "Hidden".

#pragma once

#include "CoreMinimal.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Memory/HiddenQuestData.h" // EQuestKind (B-388)
#include "Memory/MemoryData.h" // EMemoryDepth (a non-unity compile of this file found it missing)
#include "Save/WoRSaveTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HiddenQuestSubsystem.generated.h"

class UCurseSubsystem;
class UHiddenQuestData;
struct FHiddenQuestStep;
struct FMemoryEvent;
struct FWoRConditionContext;

/** Where one quest stands for this player. */
struct FHiddenQuestProgress
{
	int32 StepsDone = 0;
	bool bFinished = false;
	/** Times the step in progress was met this run; reset at run end. */
	int32 RunCount = 0;
	/** Q-397 (Crown): when this player found it, counting up (higher = newer); the journal lists newest first. */
	int32 FoundOrder = 0;
};

/** B-388: a started quest as the journal shows it (B-397: a found hidden quest too). */
struct FVisibleQuest
{
	FName QuestId;
	EQuestKind Kind = EQuestKind::Main;
	FText Title;
	/** The step in progress's goal only (later steps and the end memory never); empty once finished. */
	FText Goal;
	bool bFinished = false;
	/** Q-397: FHiddenQuestProgress::FoundOrder (higher = found later). */
	int32 FoundOrder = 0;
};

UCLASS()
class WORLD_OF_REFRACTION_API UHiddenQuestSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	/** Starts Quest for this player (once). False when null, already started or finished. */
	bool StartQuest(const UHiddenQuestData *Quest);

	/** Every started quest among Quests checks its current step against Event. Returns how many steps were done. */
	int32 CheckEvent(const FMemoryEvent &Event, const TArray<UHiddenQuestData *> &Quests);
	/** The same, against every hidden quest asset; first (B-381) the quests the event's worn gear and evolutions carry. */
	int32 CheckEvent(const FMemoryEvent &Event);

	/** B-381 (D-165): starts each quest an item worn in Event carries (FBoundQuest) whose conditions hold, for this
	 *  player's own characters only (a co-op friend's gear starts theirs: B-377b). Returns how many started. */
	int32 StartGearQuests(const FMemoryEvent &Event);

	/** B-377b (client): the host saw this player's part of an event meet QuestId's step StepIndex (CheckFriendsQuests);
	 *  counted only while that is still the current step. */
	void ApplyStepMetFromHost(FName QuestId, int32 StepIndex);

	/** Run end: the step in progress starts its count again next run; steps done stay. */
	void EndRun();

	bool IsStarted(FName QuestId) const { return Progress.Contains(QuestId); }
	bool IsFinished(FName QuestId) const;
	int32 GetStepsDone(FName QuestId) const;
	int32 GetRunCount(FName QuestId) const;

	/** Save (D-059: explicit structs): every started quest's steps done and whether finished. */
	FSavedHiddenQuests Capture() const;
	void Restore(const FSavedHiddenQuests &Saved);

	/** Debug: one line per started quest with its kind, title and current goal (B-388; B-397: hidden ones too), and
	 *  the last notice shown. */
	FString GetQuestsString() const;

	/** B-388: this player's started quests: main first, then side, then (B-397) found hidden ones; unfinished before
	 *  finished; newest found first (Q-397), then by id. Not const: a quest restored from the save is found by gathering
	 *  the quest assets. */
	TArray<FVisibleQuest> GetVisibleQuests();

	/** B-397: the last private quest notice this player was shown (debug and specs; empty before any). */
	const FText &GetLastNotice() const { return LastNotice; }

	/** B-388: gives the quest QuestId (a main or side quest from the story, V, a conversation or an NPC; any quest asset).
	 *  False when no asset has that id, or it was started already. */
	bool StartQuestById(FName QuestId);

	/** Q-377: the depth Memory has for this player's characters (the local player's own, else the local party's); None
	 *  for a memory for every character. */
	EMemoryDepth GetDepthForPlayer(const UMemoryData *Memory) const;

	/** Quests a newly recorded memory may start; empty: every hidden quest asset (tests set a list). */
	void SetQuestsForStarts(const TArray<UHiddenQuestData *> &Quests);

private:
	UFUNCTION()
	void HandleMemoryRecorded(UMemoryData *Memory, EMemorySource Source);

	/** Every hidden quest asset, gathered once (UHiddenQuestData::GatherAll). */
	TArray<UHiddenQuestData *> GetAllQuests();

	/** The last step is done: finished, and its end memory into this player's run and journal only. */
	void Finish(const UHiddenQuestData &Quest);

	/** Quest's current step was met once more: counts it, moves on when its times are reached (notice, finish).
	 *  True when a step was done. */
	bool CountStepMet(const UHiddenQuestData &Quest);

	/** B-377b (host): each friend's reported quests check their current step on the friend's own part of Event. */
	void CheckFriendsQuests(const FMemoryEvent &Event, const TArray<UHiddenQuestData *> &Quests, const TSet<FName> &Team);

	/** B-377b (client): sends this player's quest progress to the host (AWoRPlayerState::ServerReportQuests). */
	void ReportToHost() const;

	/** Q-378: a step's Rewards (curses through Curses, which may be null; other kinds logged). */
	void GiveStepRewards(const UHiddenQuestData &Quest, const FHiddenQuestStep &Step, UCurseSubsystem *Curses);

	/** Q-378 (Crown): a done step met again gives back the curses it (or a finished quest's end memory) gave and that were
	 *  cleared since. Returns how many came back. */
	int32 RegainCurses(UCurseSubsystem &Curses, const FWoRConditionContext &Context, const TArray<UHiddenQuestData *> &Quests);

	TMap<FName, FHiddenQuestProgress> Progress;

	/** Q-397: the next quest found gets this FoundOrder (Restore sets it past the save's newest). */
	int32 NextFoundOrder = 1;

	/** Q-377: started since the last CheckEvent; those wait for the next event. */
	TSet<FName> JustStarted;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHiddenQuestData>> AllQuests;
	bool bGatheredQuests = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHiddenQuestData>> QuestsForStarts;

	/** B-388: every quest asset seen (started here, or gathered), by id, so the journal can show a started quest's title
	 *  and goal; a quest restored from the save is found through GetAllQuests. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UHiddenQuestData>> KnownQuests;

	/** The asset for QuestId among KnownQuests (null when not seen). */
	const UHiddenQuestData *FindKnownQuest(FName QuestId) const;

	/** B-397: shows Notice on this machine's screen only (this player's own) and keeps it as LastNotice. */
	void ShowNotice(const FText &Notice);

	FText LastNotice;
};
