// HiddenQuestSubsystem.h
// B-377 (MemoryCatalogue §3.6, Crown 2026-11-08): this player's hidden quest progress. A quest starts when the journal
// records one of its start memories at the right depth (UMemoryJournalSubsystem::OnMemoryRecorded); each memory event
// (MemoryConditions::CheckEvent) checks every started quest's current step; a step done is kept across runs (saved,
// FSavedHiddenQuests), and only the count of the step in progress resets at run end (EndRun, from ExitTrial). The last
// step unlocks the quest's end memory. Notices are private and say nothing of the next step (D-100).
// Debug: GetQuestsString; wor.Quest.Print.
// B-377b: a co-op friend's quests stay on their machine; it reports them to the host, which tells it when its own part of
// an event meets a step (CheckFriendsQuests / ApplyStepMetFromHost) or its worn gear starts one.

#pragma once

#include "CoreMinimal.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Save/WoRSaveTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HiddenQuestSubsystem.generated.h"

class UHiddenQuestData;
struct FMemoryEvent;

/** Where one quest stands for this player. */
struct FHiddenQuestProgress
{
	int32 StepsDone = 0;
	bool bFinished = false;
	/** Times the step in progress was met this run; reset at run end. */
	int32 RunCount = 0;
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

	/** B-377b (client): the host saw this player's part of an event meet QuestId's current step (CheckFriendsQuests). */
	void ApplyStepMetFromHost(FName QuestId);

	/** Run end: the step in progress starts its count again next run; steps done stay. */
	void EndRun();

	bool IsStarted(FName QuestId) const { return Progress.Contains(QuestId); }
	bool IsFinished(FName QuestId) const;
	int32 GetStepsDone(FName QuestId) const;
	int32 GetRunCount(FName QuestId) const;

	/** Save (D-059: explicit structs): every started quest's steps done and whether finished. */
	FSavedHiddenQuests Capture() const;
	void Restore(const FSavedHiddenQuests &Saved);

	/** Debug: one line per started quest. */
	FString GetQuestsString() const;

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

	TMap<FName, FHiddenQuestProgress> Progress;

	/** Q-377: started since the last CheckEvent; those wait for the next event. */
	TSet<FName> JustStarted;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHiddenQuestData>> AllQuests;
	bool bGatheredQuests = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHiddenQuestData>> QuestsForStarts;
};
