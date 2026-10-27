// MemoryConditions.h
// B-209 (D-100 part 2, Q-209b): memories by conditions. Each UMemoryData lists unlock conditions
// (all must hold, never shown); an event (a fragment pickup, reaching a map spot, a gear upgrade,
// a fight won) checks every memory that has conditions, plus the memory its source names. Gear and
// skills count equipped, levelled or used in the fight (Q-209d). A
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
class UEvolutionItemData;
class UGameInstance;
class UMemoryData;
class USkillDataBase;
struct FAction;
struct FMemoryCondition;
struct FWorldStatRequirements;

/** Q-209d: what one combatant used in a fight (kept on the battle for that fight, D-064). */
struct FMemoryFightUsage
{
	TArray<const USkillDataBase *> Skills;
	TArray<const UEvolutionItemData *> Breakthroughs;
	/** The weapons held when it made a kill. */
	TArray<const UEquipmentDataBase *> FinishingWeapons;
};

/** One party member as an event sees them: who, what they wear at which level, what they have
 *  slotted and what they used. */
struct FMemoryEventMember
{
	const UCharacterData *Character = nullptr;
	TArray<TPair<const UEquipmentDataBase *, EItemTier>> Worn;
	/** The evolution in the primary slot, and its level. */
	const UEvolutionItemData *Evolution = nullptr;
	EItemTier EvolutionTier = EItemTier::F_Tier;
	/** Attacks, abilities and spells in the active loadout. */
	TArray<const USkillDataBase *> Slotted;
	/** Filled for a fight won. */
	FMemoryFightUsage Used;
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
	/** An evolution level-up: the levelled evolution, equipped as far as EvolutionEquipped is
	 *  concerned (its level in UpgradedTier). */
	const UEvolutionItemData *UpgradedEvolution = nullptr;
	EItemTier UpgradedTier = EItemTier::F_Tier;
	/** Whose item was levelled; null when unknown (then any party member may be its wearer). */
	const UCharacterData *UpgradedBy = nullptr;
	/** Memories the source itself names; checked even with no conditions (none: always met). */
	TArray<UMemoryData *> Named;
};

namespace MemoryConditions
{
	/** An event from what's true out of a fight: the party's characters (no worn gear: loadouts live
	 *  on combat pawns until B-085) and the active trial's name as a location. */
	WORLD_OF_REFRACTION_API FMemoryEvent MakeEvent(const UGameInstance *GameInstance, EMemorySource Source);

	/** A combatant: its character, worn gear and primary-slot evolution with levels, and slotted
	 *  skills (not what it used: that comes from the fight's record). */
	WORLD_OF_REFRACTION_API FMemoryEventMember MakeMember(const AActor *Combatant);

	/** Q-209d: Actor used Action: its skill, and a Breakthrough spell's evolution. */
	WORLD_OF_REFRACTION_API void RecordAction(FMemoryFightUsage &Usage, const AActor *Actor, const FAction &Action);

	/** Q-209d: Killer made a kill: the weapon it held. */
	WORLD_OF_REFRACTION_API void RecordKill(FMemoryFightUsage &Usage, const AActor *Killer);

	WORLD_OF_REFRACTION_API bool IsMet(const FMemoryCondition &Condition, const FMemoryEvent &Event,
									   const UMemoryJournalSubsystem *Journal);

	/** Q-209e: Wearer fits Condition's RequirementFit against Requirements (null: none, which is
	 *  always Met). An unknown wearer fits only Any. */
	WORLD_OF_REFRACTION_API bool FitsRequirements(const FMemoryCondition &Condition, const FWorldStatRequirements *Requirements,
												  const UCharacterData *Wearer);

	/** Every unlock condition of Memory holds (none: true). */
	WORLD_OF_REFRACTION_API bool AreMet(const UMemoryData &Memory, const FMemoryEvent &Event, const UMemoryJournalSubsystem *Journal);

	/** Every authored memory with unlock conditions (the asset registry, sorted by path). TODO: loads
	 *  every memory asset per event; cache it once authored memories make that slow. */
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
