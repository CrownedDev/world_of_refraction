// MemoryUnlock.h
// B-209 (D-100): every way a memory is unlocked goes through here, into the run's memories
// (UTrialRunSubsystem::CollectMemory) and the journal with its source: a map fragment, a boss or
// mini-boss beaten (UBossData::MemoryOnDefeat), a reaction that fired (FReactionTrigger::
// UnlockMemory), a special spot on the map (AMemorySpot) and a bound item's memories
// (UEquipmentDataBase::BoundMemories: by upgrade level and/or trigger conditions).
// B-337c (D-130): a bound memory's conditions are one FWoRConditionSet checked by WoRConditions
// (GetBoundConditions reads old items across: the upgrade level as GearEquipped "at least", the
// old triggers as FightStart conditions).
// wor.Memory.Print shows each memory's source.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"
#include "Memory/MemoryJournalSubsystem.h"

class AActor;
class AWoRPlayerState;
class UBossData;
class UCharacterData;
class UEquipmentDataBase;
class UGameInstance;
class UMemoryData;
struct FWoRConditionContext;
struct FWoRConditionSet;
struct FBoundMemory;
struct FFiredReaction;

namespace MemoryUnlock
{
	/** Into the run and the journal. False when Memory is null or already collected. B-235d (Q-235d, Crown): the host
	 *  found it (a source with no player attribution yet), so in co-op only a memory for every character (it lists no
	 *  character) also goes to each friend. */
	WORLD_OF_REFRACTION_API bool Unlock(UGameInstance *GameInstance, UMemoryData *Memory, EMemorySource Source);

	/** B-235d (Q-235d, Crown): into the run, into the host's journal when bHostReceives, and to each of Friends (once
	 *  each, privately: AWoRPlayerState::Client_UnlockMemory). False when Memory is null or already in the run (or, for
	 *  the host, already in its journal). */
	WORLD_OF_REFRACTION_API bool UnlockFor(UGameInstance *GameInstance, UMemoryData *Memory, EMemorySource Source, bool bHostReceives,
										   const TArray<AWoRPlayerState *> &Friends);

	/** B-235d: a memory is for one of Characters (UMemoryData::CharacterDepth above None), or for every character
	 *  when it lists no character. */
	WORLD_OF_REFRACTION_API bool QualifiesFor(const UMemoryData *Memory, const TArray<UCharacterData *> &PlayerCharacters);

	/** B-235d (Q-235d, Crown, after Shangri-La Frontier's unique scenarios): a friend gets Memory when it lists no
	 *  character, or when it is for one of the friend's own characters and the friend met its conditions themselves
	 *  (bMetThemselves: read against their own characters). */
	WORLD_OF_REFRACTION_API bool ShouldFriendReceive(const UMemoryData *Memory, const TArray<UCharacterData *> &FriendCharacters,
													 bool bMetThemselves);

	/** B-235d (Q-235d, Crown): the host gets Memory when it lists no character, when it is for one of the host's own
	 *  characters and the host met its conditions, or when no friend gets it (the host's run found it; solo always). */
	WORLD_OF_REFRACTION_API bool ShouldHostReceive(const UMemoryData *Memory, const TArray<UCharacterData *> &HostCharacters,
												   bool bHostMet, bool bAnyFriendReceives);

	/** B-235d: the remote players on a listen server (empty solo or on a client). */
	WORLD_OF_REFRACTION_API TArray<AWoRPlayerState *> GetRemotePlayers(const UGameInstance *GameInstance);

	/** B-210 (Q-190 changed by Crown): Memory is in the journal, so no source grants it again. */
	WORLD_OF_REFRACTION_API bool IsCollected(const UGameInstance *GameInstance, const UMemoryData *Memory);

	/** BossData beaten: its MemoryOnDefeat, if any. Obsolete since B-209's conditions (a fight won is
	 *  MemoryConditions' event; ABattleGameMode::CheckFightWonMemories): no game caller; remove after
	 *  Crown's PIE pass. */
	WORLD_OF_REFRACTION_API bool UnlockBossDrop(UGameInstance *GameInstance, const UBossData *BossData);

	/** A fight won: each beaten character with boss data unlocks its MemoryOnDefeat. Returns how
	 *  many were new. */
	WORLD_OF_REFRACTION_API int32 UnlockBossDrops(UGameInstance *GameInstance, const TArray<UCharacterData *> &Beaten);

	/** The items Actor wears now, each with the level of the owned copy. */
	WORLD_OF_REFRACTION_API TArray<TPair<const UEquipmentDataBase *, EItemTier>> GetWornWithTiers(const AActor *Actor);

	/** How many tiers InstanceTier is above Item's authored tier. */
	WORLD_OF_REFRACTION_API int32 GetUpgradeSteps(const UEquipmentDataBase &Item, EItemTier InstanceTier);

	/** OBSOLETE (B-337c: IsBoundMet through the one evaluator; kept as the parity reference until PIE).
	 *  Every condition set on Bound holds. Wearer null = out of a fight (no trigger can hold). */
	WORLD_OF_REFRACTION_API bool AreConditionsMet(const FBoundMemory &Bound, const UEquipmentDataBase &Item, int32 UpgradeSteps,
												  const UCharacterData *Wearer, const TArray<AActor *> &Opposing);

	/** B-337c: Bound's conditions in the one vocabulary: its Conditions when authored, else the old fields read
	 *  across (GearEquipped Item "at least" UpgradeLevel, worn by Item's BoundOwner when bNeedsOwnerWearing, plus
	 *  FacingEnemy FacingCharacter; with either old trigger these check at FightStart only; owner wearing with no
	 *  BoundOwner never holds, as before). */
	WORLD_OF_REFRACTION_API FWoRConditionSet GetBoundConditions(const FBoundMemory &Bound, const UEquipmentDataBase &Item);

	/** B-337c: Bound has a memory and its conditions hold in Context (a level-up: trigger BoundGearUsed and the
	 *  event's upgraded item; a fight start: trigger FightStart, the party's worn gear and the opposing team). */
	WORLD_OF_REFRACTION_API bool IsBoundMet(const FBoundMemory &Bound, const UEquipmentDataBase &Item, const FWoRConditionContext &Context);

	/** Item levelled up to InstanceTier: its bound memories with no trigger whose upgrade level is
	 *  reached. Returns how many were new. */
	WORLD_OF_REFRACTION_API int32 UnlockBoundOnLevelUp(UGameInstance *GameInstance, const UEquipmentDataBase *Item,
													   EItemTier InstanceTier);

	/** A fight starting: each fired reaction's UnlockMemory, and every bound memory of the items the
	 *  party wears whose conditions hold now. Returns how many were new. */
	WORLD_OF_REFRACTION_API int32 UnlockAtFightStart(UGameInstance *GameInstance, const TArray<FFiredReaction> &Fired,
													 const TArray<AActor *> &Party, const TArray<AActor *> &Opposing);
}
