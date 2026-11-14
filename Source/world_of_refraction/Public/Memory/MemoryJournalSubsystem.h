// MemoryJournalSubsystem.h
// B-190 (RunStructure §5, Q-190), B-194 (Q-194): the memories collected across runs, the lore read,
// the V lines heard and the conversations played: what the hub journal shows and V's lines and
// conversations unlock from (StoryLinesData). A memory stays once collected, even when the
// run that found it fails. Debug: wor.Memory.Print.
//
// TODO(B-090): saved with the profile; for now it lasts while the game runs.

#pragma once

#include "CoreMinimal.h"
#include "Save/WoRSaveTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MemoryJournalSubsystem.generated.h"

class UMemoryData;

/** B-209 (D-100): where a memory came from. Append-only. */
UENUM(BlueprintType)
enum class EMemorySource : uint8
{
	Fragment UMETA(DisplayName = "Map fragment"),
	Boss UMETA(DisplayName = "Boss or mini-boss beaten"),
	Reaction UMETA(DisplayName = "Reaction trigger"),
	MapSpot UMETA(DisplayName = "Special spot on the map"),
	BoundGear UMETA(DisplayName = "Bound gear"),
	/** B-209 (D-100 part 2): a fight won met a memory's unlock conditions. */
	FightWon UMETA(DisplayName = "Fight won"),
	/** B-377: a hidden quest's last step was done (its end memory). */
	HiddenQuest UMETA(DisplayName = "Hidden quest")
};

/** B-377: a memory went into this journal (hidden quests start from it). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMemoryRecorded, UMemoryData *, Memory, EMemorySource, Source);

/** B-463c: what a lore object is, shown in the journal (UIScreens #6). Append-only (saved as its byte). */
UENUM(BlueprintType)
enum class ELoreKind : uint8
{
	Inscription,
	Note,
	Overheard,
	ItemDescription UMETA(DisplayName = "Item description")
};

/** A lore object read on a trial map (B-194). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FJournalLoreEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Journal")
	FName LoreId;

	UPROPERTY(BlueprintReadOnly, Category = "Journal")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "Journal")
	FText Body;

	/** B-463c: where it was read ("Ember Trial, Present"; the hub's name outside a run). */
	UPROPERTY(BlueprintReadOnly, Category = "Journal")
	FText Place;

	UPROPERTY(BlueprintReadOnly, Category = "Journal")
	ELoreKind Kind = ELoreKind::Inscription;
};

UCLASS()
class WORLD_OF_REFRACTION_API UMemoryJournalSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** B-377: after RecordMemory adds a memory (once per memory). */
	UPROPERTY(BlueprintAssignable, Category = "Journal")
	FOnMemoryRecorded OnMemoryRecorded;

	/** Add Memory to the journal, noting where it came from. False when it is null or already
	 *  there (the first source stays). */
	bool RecordMemory(UMemoryData *Memory, EMemorySource Source);

	/** B-463c: where Memory was found (the place at RecordMemory); empty when unknown (older saves). */
	FText GetPlace(const UMemoryData *Memory) const;

	/** B-463c: a memory or lore entry not opened in the journal yet (memory id or lore id). Saved. */
	bool IsNew(FName EntryId) const { return NewEntries.Contains(EntryId); }
	/** The journal opened it: no longer new. False when it wasn't. */
	bool MarkOpened(FName EntryId) { return NewEntries.Remove(EntryId) > 0; }

	/** Where the player is now, as the journal names a place: "{trial}, {variation}" in a run, else the hub. */
	static FText GetCurrentPlace(const UGameInstance *GameInstance);

	/** Where Memory first came from; unset when it isn't in the journal. */
	TOptional<EMemorySource> GetSource(const UMemoryData *Memory) const;

	bool HasMemory(const UMemoryData *Memory) const { return Memories.Contains(Memory); }

	/** In the order collected. */
	const TArray<TObjectPtr<UMemoryData>> &GetMemories() const { return Memories; }

	/** By UMemoryData::GetMemoryId (what V's lines and conversations name). */
	bool HasMemoryId(FName MemoryId) const;

	/** Keep a lore object's text for rereading in the hub. False when LoreId is already kept. */
	bool RecordLore(FName LoreId, const FText &Title, const FText &Body, ELoreKind Kind = ELoreKind::Inscription);

	const TArray<FJournalLoreEntry> &GetLore() const { return Lore; }

	void MarkVLineHeard(FName LineId) { HeardVLines.Add(LineId); }
	bool HasHeardVLine(FName LineId) const { return HeardVLines.Contains(LineId); }

	/** A conversation played to its end: listed in the journal as logged dialogue. */
	void MarkConversationPlayed(FName ConversationId) { PlayedConversations.AddUnique(ConversationId); }
	bool HasPlayedConversation(FName ConversationId) const { return PlayedConversations.Contains(ConversationId); }
	const TArray<FName> &GetPlayedConversations() const { return PlayedConversations; }

	/** B-377b (D-059): the journal for the save, and back. Restore does not announce the memories (OnMemoryRecorded):
	 *  quests and curses keep their own saved state. An unresolvable memory asset is dropped with a warning. */
	FSavedJournal Capture() const;
	void Restore(const FSavedJournal &Saved);

	/** Debug: one line per memory (with its character depths) and per lore entry. */
	FString GetJournalString() const;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMemoryData>> Memories;

	/** Parallel to Memories (RecordMemory is the only writer of either). */
	TArray<EMemorySource> Sources;
	/** B-463c: parallel to Memories too. */
	TArray<FText> Places;
	/** B-463c: memory and lore ids not opened in the journal yet. */
	TSet<FName> NewEntries;

	UPROPERTY(Transient)
	TArray<FJournalLoreEntry> Lore;

	TSet<FName> HeardVLines;
	TArray<FName> PlayedConversations;
};
