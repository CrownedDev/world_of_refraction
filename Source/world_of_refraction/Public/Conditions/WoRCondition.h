// WoRCondition.h
// B-337 (D-130): one condition vocabulary for the game, on the deckbuilder's trigger -> condition ->
// effect model (wor-deckbuilder docs/reference/05-triggers-conditions.md): a condition says WHEN it is
// checked (Trigger) and WHAT must be true then (Subject, with its operands, Operator / Value and whose
// it is). A FWoRConditionSet holds when every condition in it holds (the deckbuilder's Nested list).
// Memories read it first (B-337); boss phases, boss triggers and reactions move onto it in B-337b.
// The evaluator is WoRConditions (WoRConditions.h). Enums are append-only (saved in assets).

#pragma once

#include "CoreMinimal.h"
#include "WoRCondition.generated.h"

class UCharacterData;
class UEquipmentDataBase;
class UEvolutionItemData;
class UMemoryData;
class USkillDataBase;

/** When a condition is checked: the event it watches. Any: at every event that checks conditions. */
UENUM(BlueprintType)
enum class EWoRConditionTrigger : uint8
{
	Any,
	FragmentFound,
	BossBeaten,
	ReactionFired,
	MapSpotReached,
	BoundGearUsed,
	FightWon
};

/** What must be true (the deckbuilder's Subject). Append only. */
UENUM(BlueprintType)
enum class EWoRConditionSubject : uint8
{
	/** Nothing to check: holds whenever the trigger does. */
	None,
	/** Character is in the party. */
	CharacterInParty,
	/** Item is worn (by Character when set, else anyone); its tiers above its authored tier compare
	 *  to Value by Operator. A gear upgrade counts the levelled item as worn. */
	GearEquipped,
	/** LocationId is where the event happens. */
	AtLocation,
	/** Enemy was beaten in the fight just won. */
	EnemyBeaten,
	/** The fight was won with no party member below their HP at its start. */
	NoDamageTaken,
	/** PriorMemory is in the journal. */
	MemoryCollected,
	/** TODO: hidden conditions (pillar 10) are not built; never holds. */
	Hidden,
	/** Evolution is in the primary slot (of Character / anyone); its tiers above its authored tier
	 *  compare to Value. A level-up counts the levelled evolution as equipped. */
	EvolutionEquipped,
	/** Skill is in the loadout (of Character / anyone). */
	SkillSlotted,
	/** Skill was used in the fight just won. */
	SkillUsed,
	/** A Breakthrough spell was cast from Evolution in the fight just won. */
	BreakthroughUsed,
	/** Whoever held the weapon Item made a kill in the fight just won. */
	FinishingBlow
};

/** How a number compares to Value. */
UENUM(BlueprintType)
enum class EWoRConditionOperator : uint8
{
	AtLeast,
	AtMost,
	Equal
};

/** The wearer against what an equipped item or skill requires, per pillar (was EMemoryRequirementFit). */
UENUM(BlueprintType)
enum class EWoRRequirementFit : uint8
{
	/** Requirements don't matter. */
	Any,
	/** Every pillar meets them. */
	Met,
	/** Met, and every pillar asked for is RequirementMargin or more over. */
	Above,
	/** Some pillar is RequirementMargin (at least 1) or more short. */
	Under
};

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FWoRCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	EWoRConditionTrigger Trigger = EWoRConditionTrigger::Any;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	EWoRConditionSubject Subject = EWoRConditionSubject::None;

	/** Whose: CharacterInParty's character; for gear, evolution and skill subjects the one it must
	 *  belong to (empty: anyone in the party). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	TObjectPtr<UCharacterData> Character = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	TObjectPtr<UEquipmentDataBase> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	TObjectPtr<UEvolutionItemData> Evolution = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	TObjectPtr<USkillDataBase> Skill = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	TObjectPtr<UCharacterData> Enemy = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	TObjectPtr<UMemoryData> PriorMemory = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	FName LocationId;

	/** Numeric subjects (GearEquipped / EvolutionEquipped: tiers above the authored tier). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	EWoRConditionOperator Operator = EWoRConditionOperator::AtLeast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition", meta = (ClampMin = "0"))
	int32 Value = 0;

	/** GearEquipped / EvolutionEquipped / SkillSlotted: the wearer against its requirements. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	EWoRRequirementFit RequirementFit = EWoRRequirementFit::Any;

	/** Above / Under: by how many levels on a pillar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition", meta = (ClampMin = "0"))
	int32 RequirementMargin = 0;
};

/** Every condition must hold (empty: holds). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FWoRConditionSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	TArray<FWoRCondition> Conditions;
};
