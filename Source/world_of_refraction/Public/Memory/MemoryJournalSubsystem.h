// MemoryJournalSubsystem.h
// B-190 (RunStructure §5, Q-190), B-194 (Q-194): the memories collected across runs, the lore read,
// the V lines heard and the conversations played: what the hub journal shows and V's lines and
// conversations unlock from (StoryLinesData). A memory stays once collected, even when the
// run that found it fails. Debug: wor.Memory.Print.
//
// TODO(B-090): saved with the profile; for now it lasts while the game runs.

#pragma once

#include "CoreMinimal.h"
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
	BoundGear UMETA(DisplayName = "Bound gear")
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
};

UCLASS()
class WORLD_OF_REFRACTION_API UMemoryJournalSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Add Memory to the journal, noting where it came from. False when it is null or already
	 *  there (the first source stays). */
	bool RecordMemory(UMemoryData *Memory, EMemorySource Source);

	/** Where Memory first came from; unset when it isn't in the journal. */
	TOptional<EMemorySource> GetSource(const UMemoryData *Memory) const;

	bool HasMemory(const UMemoryData *Memory) const { return Memories.Contains(Memory); }

	/** In the order collected. */
	const TArray<TObjectPtr<UMemoryData>> &GetMemories() const { return Memories; }

	/** By UMemoryData::GetMemoryId (what V's lines and conversations name). */
	bool HasMemoryId(FName MemoryId) const;

	/** Keep a lore object's text for rereading in the hub. False when LoreId is already kept. */
	bool RecordLore(FName LoreId, const FText &Title, const FText &Body);

	const TArray<FJournalLoreEntry> &GetLore() const { return Lore; }

	void MarkVLineHeard(FName LineId) { HeardVLines.Add(LineId); }
	bool HasHeardVLine(FName LineId) const { return HeardVLines.Contains(LineId); }

	/** A conversation played to its end: listed in the journal as logged dialogue. */
	void MarkConversationPlayed(FName ConversationId) { PlayedConversations.AddUnique(ConversationId); }
	bool HasPlayedConversation(FName ConversationId) const { return PlayedConversations.Contains(ConversationId); }
	const TArray<FName> &GetPlayedConversations() const { return PlayedConversations; }

	/** Debug: one line per memory (with its character depths) and per lore entry. */
	FString GetJournalString() const;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMemoryData>> Memories;

	/** Parallel to Memories (RecordMemory is the only writer of either). */
	TArray<EMemorySource> Sources;

	UPROPERTY(Transient)
	TArray<FJournalLoreEntry> Lore;

	TSet<FName> HeardVLines;
	TArray<FName> PlayedConversations;
};
