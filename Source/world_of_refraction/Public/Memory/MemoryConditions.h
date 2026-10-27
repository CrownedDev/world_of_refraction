// MemoryConditions.h
// B-209 (D-100 part 2, Q-209b): memories by conditions. Each UMemoryData lists unlock conditions
// (all must hold, never shown); an event (a fragment pickup, reaching a map spot, a gear upgrade,
// a fight won) checks every memory that has conditions, plus the memory its source names. A
// memory earned in an event can meet the next one's PriorMemory in the same event (chains). All
// grants go through MemoryUnlock::Unlock (journal once, B-210).
// Debug: wor.Memory.Conditions (each memory's conditions, met or not, for the party now).

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"
#include "Memory/MemoryJournalSubsystem.h"

class AActor;
class UCharacterData;
class UEquipmentDataBase;
class UGameInstance;
class UMemoryData;
struct FMemoryCondition;

/** One party member as an event sees them: who, and what they wear at which level. */
struct FMemoryEventMember
{
	const UCharacterData *Character = nullptr;
	TArray<TPair<const UEquipmentDataBase *, EItemTier>> Worn;
};

/** What is true when an event checks memory conditions. */
struct FMemoryEvent
{
	/** Recorded in the journal for what this event unlocks. */
	EMemorySource Source = EMemorySource::Fragment;
	TArray<FMemoryEventMember> Party;
	/** The trial's asset name and the fragment / spot's id, where set. */
	TArray<FName> Locations;
	TArray<const UCharacterData *> Beaten;
	bool bNoDamageTaken = false;
	/** A gear upgrade: the levelled item, worn as far as GearEquipped is concerned. */
	const UEquipmentDataBase *UpgradedItem = nullptr;
	EItemTier UpgradedTier = EItemTier::F_Tier;
	/** Memories the source itself names; checked even with no conditions (none: always met). */
	TArray<UMemoryData *> Named;
};

namespace MemoryConditions
{
	/** An event from what's true out of a fight: the party's characters (no worn gear: loadouts live
	 *  on combat pawns until B-085) and the active trial's name as a location. */
	WORLD_OF_REFRACTION_API FMemoryEvent MakeEvent(const UGameInstance *GameInstance, EMemorySource Source);

	/** A combatant: its character and worn gear with levels. */
	WORLD_OF_REFRACTION_API FMemoryEventMember MakeMember(const AActor *Combatant);

	WORLD_OF_REFRACTION_API bool IsMet(const FMemoryCondition &Condition, const FMemoryEvent &Event,
									   const UMemoryJournalSubsystem *Journal);

	/** Every unlock condition of Memory holds (none: true). */
	WORLD_OF_REFRACTION_API bool AreMet(const UMemoryData &Memory, const FMemoryEvent &Event, const UMemoryJournalSubsystem *Journal);

	/** Every authored memory with unlock conditions (the asset registry, sorted by path). */
	WORLD_OF_REFRACTION_API TArray<UMemoryData *> GatherConditional();

	/** Unlock every memory among Conditional and Event.Named whose conditions hold, repeating so
	 *  one earned now can unlock the next. Returns how many were new. */
	WORLD_OF_REFRACTION_API int32 CheckEvent(UGameInstance *GameInstance, const FMemoryEvent &Event,
											 const TArray<UMemoryData *> &Conditional);

	/** CheckEvent against every authored conditional memory. */
	WORLD_OF_REFRACTION_API int32 CheckEvent(UGameInstance *GameInstance, const FMemoryEvent &Event);

	/** Debug: Memory's conditions, each marked met or not for Event. */
	WORLD_OF_REFRACTION_API FString GetConditionsString(const UMemoryData &Memory, const FMemoryEvent &Event,
														const UMemoryJournalSubsystem *Journal);
}
