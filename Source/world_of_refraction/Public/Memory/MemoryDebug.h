// MemoryDebug.h
// B-375 (MemoryCatalogue §6, HiddenQuests.md §3): why a memory did or did not fire. For any memory: who it is for
// (its scope and depths), each condition set with every condition marked met / not yet, for each player present (the
// host's part of the event and each co-op friend's, with the team's memories for PriorMemory, B-235e), and who would
// receive it now (Q-235d).
// Console: wor.Memory.Why <MemoryId> (out of a fight, the party now).

#pragma once

#include "CoreMinimal.h"
#include "Memory/MemoryConditions.h"

class UGameInstance;
class UMemoryData;

namespace MemoryDebug
{
	/** The authored memory whose UMemoryData::GetMemoryId is MemoryId (the asset registry); null when none. */
	WORLD_OF_REFRACTION_API UMemoryData *FindMemory(FName MemoryId);

	/** "for every character" or each listed character with its depth. */
	WORLD_OF_REFRACTION_API FString GetScopeString(const UMemoryData &Memory);

	/** One line per condition set (any one opens it), each condition marked [met] / [not yet], read on Event with
	 *  KnownMemoryIds for PriorMemory when set (else Journal). "by its source only" with no conditions. */
	WORLD_OF_REFRACTION_API FString GetRoutesString(const UMemoryData &Memory, const FMemoryEvent &Event, const UMemoryJournalSubsystem *Journal,
													const TSet<FName> *KnownMemoryIds = nullptr);

	/** The whole answer: scope, the host's part and each friend's (their characters and routes), and who receives it
	 *  now (MemoryConditions::PickReceivers). */
	WORLD_OF_REFRACTION_API FString GetWhyString(const UGameInstance *GameInstance, const UMemoryData &Memory, const FMemoryEvent &Event);
}
