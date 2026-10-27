// MemoryUnlock.h
// B-209 (D-100): every way a memory is unlocked goes through here, into the run's memories
// (UTrialRunSubsystem::CollectMemory) and the journal with its source: a map fragment, a boss or
// mini-boss beaten (UBossData::MemoryOnDefeat), a reaction that fired (FReactionTrigger::
// UnlockMemory), a special spot on the map (AMemorySpot) and a bound item's memories
// (UEquipmentDataBase::BoundMemories: by upgrade level and/or trigger conditions).
// wor.Memory.Print shows each memory's source.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"
#include "Memory/MemoryJournalSubsystem.h"

class AActor;
class UBossData;
class UCharacterData;
class UEquipmentDataBase;
class UGameInstance;
class UMemoryData;
struct FBoundMemory;
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

	/** How many tiers InstanceTier is above Item's authored tier. */
	WORLD_OF_REFRACTION_API int32 GetUpgradeSteps(const UEquipmentDataBase &Item, EItemTier InstanceTier);

	/** Every condition set on Bound holds. Wearer null = out of a fight (no trigger can hold). */
	WORLD_OF_REFRACTION_API bool AreConditionsMet(const FBoundMemory &Bound, const UEquipmentDataBase &Item, int32 UpgradeSteps,
												  const UCharacterData *Wearer, const TArray<AActor *> &Opposing);

	/** Item levelled up to InstanceTier: its bound memories with no trigger whose upgrade level is
	 *  reached. Returns how many were new. */
	WORLD_OF_REFRACTION_API int32 UnlockBoundOnLevelUp(UGameInstance *GameInstance, const UEquipmentDataBase *Item,
													   EItemTier InstanceTier);

	/** A fight starting: each fired reaction's UnlockMemory, and every bound memory of the items the
	 *  party wears whose conditions hold now. Returns how many were new. */
	WORLD_OF_REFRACTION_API int32 UnlockAtFightStart(UGameInstance *GameInstance, const TArray<FFiredReaction> &Fired,
													 const TArray<AActor *> &Party, const TArray<AActor *> &Opposing);
}
