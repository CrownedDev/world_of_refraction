// CurseSubsystem.h
// B-378 (MemoryCatalogue §3.8; SLF's curses, D-100): this player's curses. A memory whose rewards include a Curse gives
// it when it goes into this player's journal (UMemoryJournalSubsystem::OnMemoryRecorded, on any machine); a RemoveCurse
// reward lifts it. A curse lasts across runs until removed (saved, FSavedCurses). It is also a key: the CurseHeld
// condition reads it (FWoRConditionContext::HeldCurses). What it weakens in a fight is B-378b.
// Q-378 (changed by Crown, 2026-11-08): a hidden quest gives one too (a step's Rewards, or its end memory); the player
// can clear one (ClearCurse; the hub's button is B-378b); a cleared curse comes back when its giving memory's or
// quest step's conditions are met again (RegainFrom; CheckRegain for memories, UHiddenQuestSubsystem for steps).
// Debug: GetCursesString; wor.Curse.Print / wor.Curse.Clear.

#pragma once

#include "CoreMinimal.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Save/WoRSaveTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CurseSubsystem.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UCurseSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	/** False when CurseId is none or already held. Q-378 (Crown): a curse lifted before can be given again (regained). */
	bool AddCurse(FName CurseId);
	/** Its own memory or quest lifts it (a RemoveCurse reward). False when it was not held. */
	bool RemoveCurse(FName CurseId);
	/** Q-378 (Crown): the player chooses to clear it (the hub's service, B-378b). False when it was not held. */
	bool ClearCurse(FName CurseId);
	bool HasCurse(FName CurseId) const { return HeldCurses.Contains(CurseId); }
	const TSet<FName> &GetHeldCurses() const { return HeldCurses; }
	/** Lifted or cleared before (and not held now): what CheckRegain can give back. */
	bool WasLifted(FName CurseId) const { return LiftedCurses.Contains(CurseId) && !HeldCurses.Contains(CurseId); }

	/** Rewards' Curse and RemoveCurse entries, applied in order. Returns how many changed what is held. */
	int32 ApplyRewards(const TArray<FMemoryReward> &Rewards);
	int32 ApplyRewards(const UMemoryData &Memory) { return ApplyRewards(Memory.Rewards); }

	/** Q-378 (Crown): their giver's conditions were met again: every Curse in Rewards not held now comes back (the curse
	 *  only; the memory is not unlocked again). Returns how many came back. */
	int32 RegainFrom(const TArray<FMemoryReward> &Rewards);
	/** Whether Rewards give a curse this player doesn't hold now. */
	bool CouldRegain(const TArray<FMemoryReward> &Rewards) const;

	/** Q-378: at a memory event, every memory in Candidates this player already has, that gives a curse not held now,
	 *  and whose conditions Event meets for this player: its curses come back. Returns how many came back.
	 *  TODO(co-op): a friend's own curses come back on their machine; the host checks only its own (as B-378's key). */
	int32 CheckRegain(const FMemoryEvent &Event, const TArray<UMemoryData *> &Candidates);

	/** Save (D-059). */
	FSavedCurses Capture() const;
	void Restore(const FSavedCurses &Saved);

	FString GetCursesString() const;

private:
	UFUNCTION()
	void HandleMemoryRecorded(UMemoryData *Memory, EMemorySource Source);

	TSet<FName> HeldCurses;
	/** Curses lifted or cleared before (Q-378: they can be regained). */
	TSet<FName> LiftedCurses;
};
