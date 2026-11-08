// SkillConditionGroup.h
// B-337e (Q-337d, Crown 2026-11-04): a skill effect's authored condition group in the one vocabulary (D-130).
// AllOf must all hold, and one of AnyOf when AnyOf has any: the old FSkillCondition AND / OR group, condition for
// condition (SkillConditions::ToAuthored). Stored check-neutral: an event condition is the event, and the impact path
// reads a defense event as "Who defended" at check time (SkillConditions::ToSets), as the read-across did.
// Empty (both sets): no condition, the effect always holds.
// Q-337e (Crown 2026-11-08): the deckbuilder's shape: one trigger on top, the All of checks nested under it (shown as an
// indented tree, SkillConditions::GetConditionTreeString). Any of stays, collapsed (AdvancedDisplay), only so migrated
// old OR rows keep their result; new skills author All of only. The deckbuilder has no OR: the content sweep lists
// every Any of (wor.Conditions.ListAnyOf) and deletes it once each can be rewritten without "or".

#pragma once

#include "CoreMinimal.h"
#include "Conditions/WoRCondition.h"
#include "SkillConditionGroup.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSkillConditionGroup
{
	GENERATED_BODY()

	/** The checks under the trigger: every one of these must hold (the default authoring, Q-337e). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	FWoRConditionSet AllOf;

	/** Kept only for migrated old OR rows (Q-337e): when not empty, at least one of these must hold too. New skills
	 *  leave it empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Condition")
	FWoRConditionSet AnyOf;

	bool IsEmpty() const { return AllOf.Conditions.IsEmpty() && AnyOf.Conditions.IsEmpty(); }

	/** True when any condition, in either set, passes Predicate. */
	template <typename PredicateType>
	bool ContainsByPredicate(PredicateType Predicate) const
	{
		return AllOf.Conditions.ContainsByPredicate(Predicate) || AnyOf.Conditions.ContainsByPredicate(Predicate);
	}
};
