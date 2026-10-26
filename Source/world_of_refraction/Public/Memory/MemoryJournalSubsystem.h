// MemoryJournalSubsystem.h
// B-190 (RunStructure §5, Q-190): the memories collected across runs, the record the journal,
// V's lines and conversations unlock from (B-194). A memory stays once collected, even when the
// run that found it fails. Debug: wor.Memory.Print.
//
// TODO(B-090): saved with the profile; for now it lasts while the game runs.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MemoryJournalSubsystem.generated.h"

class UMemoryData;

UCLASS()
class WORLD_OF_REFRACTION_API UMemoryJournalSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Add Memory to the journal. False when it is null or already there. */
	bool RecordMemory(UMemoryData *Memory);

	bool HasMemory(const UMemoryData *Memory) const { return Memories.Contains(Memory); }

	/** In the order collected. */
	const TArray<TObjectPtr<UMemoryData>> &GetMemories() const { return Memories; }

	/** Debug: one line per memory with its character depths. */
	FString GetJournalString() const;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMemoryData>> Memories;
};
