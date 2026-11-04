// WoRConditions.h
// B-337 (D-130): the one condition evaluator. A condition holds when the context's trigger matches
// its Trigger (Any matches every event) and its Subject is true in the context.
// Memories evaluate through it (B-337); the old memory condition types convert on read (FromMemory)
// until they are retired. B-337b moves boss phases, boss triggers and reactions here and widens
// the context with fight state.
// Debug: GetConditionString / GetConditionSetString; wor.Memory.Conditions prints through them.

#pragma once

#include "CoreMinimal.h"
#include "Conditions/WoRCondition.h"
#include "Memory/MemoryJournalSubsystem.h"

class UCharacterData;
struct FMemoryCondition;
struct FMemoryEvent;
struct FWorldStatRequirements;

/** What is true when conditions are checked. Memories fill Event (party, worn gear, slotted and used
 *  skills, locations, enemies beaten, level-ups) and Journal. TODO(B-337b): fight state for boss phases
 *  and reactions. */
struct FWoRConditionContext
{
	EWoRConditionTrigger Trigger = EWoRConditionTrigger::Any;
	const FMemoryEvent *Event = nullptr;
	const UMemoryJournalSubsystem *Journal = nullptr;
};

namespace WoRConditions
{
	WORLD_OF_REFRACTION_API bool IsMet(const FWoRCondition &Condition, const FWoRConditionContext &Context);

	/** Every condition in Set holds (empty: true). */
	WORLD_OF_REFRACTION_API bool IsMet(const FWoRConditionSet &Set, const FWoRConditionContext &Context);

	/** Any one set holds (no sets: true, nothing to meet). */
	WORLD_OF_REFRACTION_API bool IsAnyMet(const TArray<FWoRConditionSet> &Sets, const FWoRConditionContext &Context);

	/** Compare Actual to Value by Operator. */
	WORLD_OF_REFRACTION_API bool Compare(int32 Actual, EWoRConditionOperator Operator, int32 Value);

	/** Wearer fits Condition's RequirementFit against Requirements (null: none, which is always Met).
	 *  An unknown wearer fits only Any. */
	WORLD_OF_REFRACTION_API bool FitsRequirements(const FWoRCondition &Condition, const FWorldStatRequirements *Requirements,
												  const UCharacterData *Wearer);

	/** A memory event's source as a trigger. */
	WORLD_OF_REFRACTION_API EWoRConditionTrigger TriggerFor(EMemorySource Source);

	/** A context for a memory event. */
	WORLD_OF_REFRACTION_API FWoRConditionContext MakeContext(const FMemoryEvent &Event, const UMemoryJournalSubsystem *Journal);

	/** The old memory condition in the one vocabulary (Trigger Any; UpgradeLevel as "at least" Value). */
	WORLD_OF_REFRACTION_API FWoRCondition FromMemory(const FMemoryCondition &Condition);

	WORLD_OF_REFRACTION_API FWoRConditionSet FromMemory(const TArray<FMemoryCondition> &Conditions);

	/** Debug: one line per condition ("GearEquipped Sword at least 2, Lord"). */
	WORLD_OF_REFRACTION_API FString GetConditionString(const FWoRCondition &Condition);

	/** Debug: a set as "a + b", each marked met / not yet for Context. */
	WORLD_OF_REFRACTION_API FString GetConditionSetString(const FWoRConditionSet &Set, const FWoRConditionContext &Context);
}
