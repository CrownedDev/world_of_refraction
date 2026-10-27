// MemoryUnlock.h
// B-209 (D-100): every way a memory is unlocked goes through here, into the run's memories
// (UTrialRunSubsystem::CollectMemory) and the journal with its source: a map fragment, a boss or
// mini-boss beaten (UBossData::MemoryOnDefeat), a reaction that fired (FReactionTrigger::
// UnlockMemory), a special spot on the map (AMemorySpot) and bound gear worn by its owner
// (UEquipmentDataBase::BoundMemories). wor.Memory.Print shows each memory's source.

#pragma once

#include "CoreMinimal.h"
#include "Memory/MemoryJournalSubsystem.h"

class AActor;
class UBossData;
class UCharacterData;
class UGameInstance;
class UMemoryData;
struct FFiredReaction;

namespace MemoryUnlock
{
	/** Into the run and the journal. False when Memory is null or both already have it. */
	WORLD_OF_REFRACTION_API bool Unlock(UGameInstance *GameInstance, UMemoryData *Memory, EMemorySource Source);

	/** BossData beaten: its MemoryOnDefeat, if any. */
	WORLD_OF_REFRACTION_API bool UnlockBossDrop(UGameInstance *GameInstance, const UBossData *BossData);

	/** A fight won: each beaten character with boss data unlocks its MemoryOnDefeat. Returns how
	 *  many were new. */
	WORLD_OF_REFRACTION_API int32 UnlockBossDrops(UGameInstance *GameInstance, const TArray<UCharacterData *> &Beaten);

	/** A fight starting: each fired reaction's UnlockMemory, and the BoundMemories of every bound
	 *  item a party member wears as its owner. Returns how many were new. */
	WORLD_OF_REFRACTION_API int32 UnlockAtFightStart(UGameInstance *GameInstance, const TArray<FFiredReaction> &Fired,
													 const TArray<AActor *> &Party);
}
