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
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "Memory/MemoryData.h" // FMemoryReward, UMemoryData::Rewards (review: the inline ApplyRewards reads them)
#include "Save/WoRSaveTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CurseSubsystem.generated.h"

struct FMemoryEvent;

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
	/** B-390 (D-169): the hub's paid clear. Wallet pays the curse's price (UCurseData::GetClearPrice in its
	 *  GetClearCurrency; a curse with no asset prices as the asset defaults), then ClearCurse. Short of it, or not
	 *  held, or the spend refused (no server authority): nothing changes and OutRefusal says why. */
	bool PayToClearCurse(FName CurseId, class UCurrencyComponent *Wallet, FText &OutRefusal);
	/** The curse's asset (by CurseId), or null when none is authored. */
	const class UCurseData *FindCurseAsset(FName CurseId) const;
	bool HasCurse(FName CurseId) const { return HeldCurses.Contains(CurseId); }
	const TSet<FName> &GetHeldCurses() const { return HeldCurses; }
	/** Lifted or cleared before (and not held now): what CheckRegain can give back. */
	bool WasLifted(FName CurseId) const { return LiftedCurses.Contains(CurseId) && !HeldCurses.Contains(CurseId); }

	/** Rewards' Curse and RemoveCurse entries, applied in order. Returns how many changed what is held. */
	int32 ApplyRewards(const TArray<FMemoryReward> &Rewards);
	int32 ApplyRewards(const UMemoryData &Memory) { return ApplyRewards(Memory.Rewards); }

	/** Q-378 (Crown): their giver's conditions were met again: every Curse in Rewards not held now comes back (the curse
	 *  only; the memory is not unlocked again), except one lifted during this event. Returns how many came back. */
	int32 RegainFrom(const TArray<FMemoryReward> &Rewards);
	/** Whether Rewards give a curse this player doesn't hold now. */
	bool CouldRegain(const TArray<FMemoryReward> &Rewards) const;

	/** Q-378: at a memory event, every memory in Candidates this player already has, that gives a curse not held now,
	 *  and whose conditions Event meets for this player: its curses come back. Returns how many came back.
	 *  TODO(co-op): a friend's own curses come back on their machine; the host checks only its own (as B-378's key). */
	int32 CheckRegain(const FMemoryEvent &Event, const TArray<UMemoryData *> &Candidates);

	/** Review: a new memory event starts; a curse lifted during it is not regained by the same event (a cure found where
	 *  the curse was given would otherwise undo itself). MemoryConditions::CheckEvent calls it first. */
	void BeginEvent() { LiftedThisEvent.Reset(); }

	/** Save (D-059). */
	FSavedCurses Capture() const;
	void Restore(const FSavedCurses &Saved);

	FString GetCursesString() const;

	/** B-378b: the summed penalty of every held curse that has a UCurseData (by CurseId); zero for none. */
	FEquipmentStatBonus GetStatPenalty() const;
	FResistanceBonus GetResistancePenalty() const;

	/** B-378b: Character is one of this player's own party characters (the local party's, not a co-op friend's): the
	 *  only ones this player's curses weaken (a friend's curses are theirs, B-377b). */
	bool IsCursedHere(const class UCharacterData *Character) const;

	/** The curse assets to read (tests set a list); unset, every curse asset is gathered once the registry has loaded. */
	void SetCurseAssets(const TArray<class UCurseData *> &Curses);

private:
	UFUNCTION()
	void HandleMemoryRecorded(UMemoryData *Memory, EMemorySource Source);

	TSet<FName> HeldCurses;

	/** B-378b: the held curses' assets. */
	TArray<const class UCurseData *> GetHeldCurseAssets() const;
	/** Gathers every curse asset once the registry has loaded; false while it is still loading (and nothing set). */
	bool EnsureCurseAssetsGathered() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UCurseData>> CurseAssets;
	bool bGatheredCurses = false;

	/** Review: the held curses' summed penalties, rebuilt when what is held or the asset list changes. */
	void RebuildPenalty() const;
	mutable FEquipmentStatBonus CachedStatPenalty;
	mutable FResistanceBonus CachedResistancePenalty;
	mutable bool bPenaltyDirty = true;
	/** Curses lifted or cleared before (Q-378: they can be regained). */
	TSet<FName> LiftedCurses;
	/** Lifted since BeginEvent: not regained by the same event. */
	TSet<FName> LiftedThisEvent;
};
