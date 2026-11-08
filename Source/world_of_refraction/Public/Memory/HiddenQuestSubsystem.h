// HiddenQuestSubsystem.h
// B-377 (MemoryCatalogue §3.6, Crown 2026-11-08): this player's hidden quest progress. A quest starts when the journal
// records one of its start memories at the right depth (UMemoryJournalSubsystem::OnMemoryRecorded); each memory event
// (MemoryConditions::CheckEvent) checks every started quest's current step; a step done is kept across runs (saved,
// FSavedHiddenQuests), and only the count of the step in progress resets at run end (EndRun, from ExitTrial). The last
// step unlocks the quest's end memory. Notices are private and say nothing of the next step (D-100).
// Debug: GetQuestsString; wor.Quest.Print.
// TODO(B-377b): a co-op friend's quests (events run on the host; their progress is on their own machine).

#pragma once

#include "CoreMinimal.h"
#include "Memory/MemoryJournalSubsystem.h"
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
	/** The same, against every hidden quest asset. */
	int32 CheckEvent(const FMemoryEvent &Event);

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

	/** Q-378: a step's Rewards (curses through Curses, which may be null; other kinds logged). */
	void GiveStepRewards(const UHiddenQuestData &Quest, const FHiddenQuestStep &Step, UCurseSubsystem *Curses);

	/** Q-378 (Crown): a done step met again gives back the curses it (or a finished quest's end memory) gave and that were
	 *  cleared since. Returns how many came back. */
	int32 RegainCurses(UCurseSubsystem &Curses, const FWoRConditionContext &Context, const TArray<UHiddenQuestData *> &Quests);

	TMap<FName, FHiddenQuestProgress> Progress;

	/** Q-377: started since the last CheckEvent; those wait for the next event. */
	TSet<FName> JustStarted;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHiddenQuestData>> AllQuests;
	bool bGatheredQuests = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHiddenQuestData>> QuestsForStarts;
};
