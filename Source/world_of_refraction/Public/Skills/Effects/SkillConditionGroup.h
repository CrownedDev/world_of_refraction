// SkillConditionGroup.h
// B-337e (Q-337d, Crown 2026-11-04): a skill effect's authored condition group in the one vocabulary (D-130).
// AllOf must all hold, and one of AnyOf when AnyOf has any: the old FSkillCondition AND / OR group, condition for
// condition (SkillConditions::ToAuthored). Stored check-neutral: an event condition is the event, and the impact path
// reads a defense event as "Who defended" at check time (SkillConditions::ToSets), as the read-across did.
// Empty (both sets): no condition, the effect always holds.

#pragma once

#include "CoreMinimal.h"
#include "Conditions/WoRCondition.h"
#include "SkillConditionGroup.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSkillConditionGroup
{
	GENERATED_BODY()

	/** Every one of these must hold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	FWoRConditionSet AllOf;

	/** When not empty, at least one of these must hold too. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	FWoRConditionSet AnyOf;

	bool IsEmpty() const { return AllOf.Conditions.IsEmpty() && AnyOf.Conditions.IsEmpty(); }

	/** True when any condition, in either set, passes Predicate. */
	template <typename PredicateType>
	bool ContainsByPredicate(PredicateType Predicate) const
	{
		return AllOf.Conditions.ContainsByPredicate(Predicate) || AnyOf.Conditions.ContainsByPredicate(Predicate);
	}
};
