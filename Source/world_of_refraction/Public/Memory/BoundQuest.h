// BoundQuest.h
// B-381 (D-165, MemoryCatalogue §3.11; NieR: Automata's weapon stories): a hidden quest a weapon, adapter, apparel
// piece or evolution carries. It starts at a memory event while the player wears the item (worn, upgraded, used a
// certain way: Conditions), for the player who wears it (Q-235d). Read by UHiddenQuestSubsystem::StartGearQuests.

#pragma once

#include "CoreMinimal.h"
#include "Conditions/WoRCondition.h"
#include "BoundQuest.generated.h"

class UHiddenQuestData;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FBoundQuest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bound")
	TObjectPtr<UHiddenQuestData> Quest = nullptr;

	/** Met at a memory event while the item is worn (empty: being worn is enough). E.g. GearEquipped at least 2 for
	 *  "upgraded", SkillUsed for "used a certain way". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bound")
	FWoRConditionSet Conditions;
};
