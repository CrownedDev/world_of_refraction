// StoryLinesData.h
// B-194 (HubAndStory §4, Q-194): V's lines and character conversations, unlocked by the memories
// in the journal (keyed by memory id, the same ids reaction triggers and bosses read). Any number
// of these assets, gathered by the asset registry. Placeholder text only; the story is Crown's.
// Shown by the placeholder hub journal (UMemoryJournalWidget). Debug: PrintStory, wor.Story.Print.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StoryLinesData.generated.h"

class UCharacterData;
class UMemoryJournalSubsystem;

/** Something V says between runs, once its memories are all in the journal. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FVLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FName LineId;

	/** Memory ids (UMemoryData::GetMemoryId); all must be collected. Empty: from the start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	TArray<FName> RequiredMemories;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story", meta = (MultiLine = true))
	FText Text;
};

/** A talk between characters (Expedition 33 camp style), unlocked like a V line. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FStoryConversation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FName ConversationId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FText Title;

	/** Who takes part (shown in the list; not checked against the party yet). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	TArray<TObjectPtr<UCharacterData>> Speakers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	TArray<FName> RequiredMemories;

	/** Played one at a time; write the speaker into the line for now ("V: ..."). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story", meta = (MultiLine = true))
	TArray<FText> Lines;
};

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UStoryLinesData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** In the order V says them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	TArray<FVLine> VLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	TArray<FStoryConversation> Conversations;

	FString GetStoryString() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintStory() const;
};

namespace StoryUnlocks
{
	/** Every story asset, sorted by path so the order is fixed. */
	WORLD_OF_REFRACTION_API TArray<UStoryLinesData *> GatherStoryData();

	/** True when every id is a memory in the journal. */
	WORLD_OF_REFRACTION_API bool AreUnlocked(const TArray<FName> &RequiredMemories, const UMemoryJournalSubsystem &Journal);

	/** V's next line: the first unlocked one not heard yet, or null. */
	WORLD_OF_REFRACTION_API const FVLine *FindNextVLine(const TArray<UStoryLinesData *> &Story,
														const UMemoryJournalSubsystem &Journal);

	/** Every unlocked conversation, played or not, in order. */
	WORLD_OF_REFRACTION_API TArray<const FStoryConversation *> GetUnlockedConversations(
		const TArray<UStoryLinesData *> &Story, const UMemoryJournalSubsystem &Journal);
}
