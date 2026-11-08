// CurseSubsystem.h
// B-378 (MemoryCatalogue §3.8; SLF's curses, D-100): this player's curses. A memory whose rewards include a Curse gives
// it when it goes into this player's journal (UMemoryJournalSubsystem::OnMemoryRecorded, on any machine); a RemoveCurse
// reward lifts it. A curse lasts across runs until removed (saved, FSavedCurses). It is also a key: the CurseHeld
// condition reads it (FWoRConditionContext::HeldCurses). What it weakens in a fight is B-378b.
// Debug: GetCursesString; wor.Curse.Print.

#pragma once

#include "CoreMinimal.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
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

	/** False when CurseId is none, already held, or lifted before (review: the journal is not saved yet, so its memory
	 *  could be recorded again after a restart; a lifted curse stays lifted). */
	bool AddCurse(FName CurseId);
	/** False when it was not held. */
	bool RemoveCurse(FName CurseId);
	bool HasCurse(FName CurseId) const { return HeldCurses.Contains(CurseId); }
	const TSet<FName> &GetHeldCurses() const { return HeldCurses; }

	/** Memory's Curse and RemoveCurse rewards, applied in order. Returns how many changed what is held. */
	int32 ApplyRewards(const UMemoryData &Memory);

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

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UCurseData>> CurseAssets;
	bool bGatheredCurses = false;

	/** Review: the held curses' summed penalties, rebuilt when what is held or the asset list changes. */
	void RebuildPenalty() const;
	mutable FEquipmentStatBonus CachedStatPenalty;
	mutable FResistanceBonus CachedResistancePenalty;
	mutable bool bPenaltyDirty = true;
	/** Curses lifted before; never given again. */
	TSet<FName> LiftedCurses;
};
