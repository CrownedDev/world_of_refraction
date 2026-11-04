// WoRCondition.h
// B-337 (D-130): one condition vocabulary for the game, on the deckbuilder's trigger -> condition ->
// effect model (wor-deckbuilder docs/reference/05-triggers-conditions.md): a condition says WHEN it is
// checked (Trigger) and WHAT must be true then (Subject, with its operands, Operator / Value and whose
// it is). A FWoRConditionSet holds when every condition in it holds (the deckbuilder's Nested list).
// Memories read it first (B-337, with their equip conditions); boss rules (B-355); reactions (B-337b); a bound
// item's memories (B-337c); skill effects, their FSkillCondition read across (B-337d, SkillConditions.h).
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
	Any = 0,
	FragmentFound = 1,
	BossBeaten = 2,
	ReactionFired = 3,
	MapSpotReached = 4,
	BoundGearUsed = 5,
	FightWon = 6,
	/** B-365 (D-158): a defender perfectly guarded or dodged every hit of one attack or spell. */
	FlawlessParry = 7,
	/** B-337c: the party goes into a fight wearing what it wears (a bound item's memory triggers). A skill
	 *  effect's On Battle Start (B-337d). */
	FightStart = 8,
	// ---- A skill effect's events (B-337d, ESkillTrigger): fired by USkillEffectManager / UActionExecutor ----
	Hit = 9,
	Crit = 10,
	TakeDamage = 11,
	TurnStart = 12,
	TurnEnd = 13,
	Kill = 14,
	StatusApplied = 15,
	StatusReceived = 16,
	SpellCast = 17,
	AbilityUsed = 18,
	Dodge = 19,
	Block = 20,
	Guard = 21,
	PerfectGuard = 22,
	PerfectDodge = 23,
	/** Retired by Guard (D-084): still fired with a Perfect Guard. */
	Parry = 24,
	PerfectParry = 25,
	PerfectBlock = 26,
};

/** What must be true (the deckbuilder's Subject). Append only. */
UENUM(BlueprintType)
enum class EWoRConditionSubject : uint8
{
	/** Nothing to check: holds whenever the trigger does. */
	None = 0,
	/** Character is in the party. */
	CharacterInParty = 1,
	/** Item is worn (by Character when set, else anyone); its tiers above its authored tier compare
	 *  to Value by Operator. A gear upgrade counts the levelled item as worn. */
	GearEquipped = 2,
	/** LocationId is where the event happens. */
	AtLocation = 3,
	/** Enemy was beaten in the fight just won. */
	EnemyBeaten = 4,
	/** The fight was won with no party member below their HP at its start. */
	NoDamageTaken = 5,
	/** PriorMemory is in the journal. */
	MemoryCollected = 6,
	/** TODO: hidden conditions (pillar 10) are not built; never holds. */
	Hidden = 7,
	/** Evolution is in the primary slot (of Character / anyone); its tiers above its authored tier
	 *  compare to Value. A level-up counts the levelled evolution as equipped. */
	EvolutionEquipped = 8,
	/** Skill is in the loadout (of Character / anyone). */
	SkillSlotted = 9,
	/** Skill was used in the fight just won. */
	SkillUsed = 10,
	/** A Breakthrough spell was cast from Evolution in the fight just won. */
	BreakthroughUsed = 11,
	/** Whoever held the weapon Item made a kill in the fight just won. */
	FinishingBlow = 12,
	// ---- The boss (B-355, D-144): read from FWoRConditionContext::Boss ----
	/** The boss's HP is at most Value percent of its max. */
	BossHPAtMostPercent = 13,
	/** The boss's own turns this fight compare to Value. */
	BossTurnsTaken = 14,
	/** The boss's Broken Darkness absorption is at its maximum. */
	BossAbsorptionMaxed = 15,
	/** The crystal on the boss's active weapon is broken. */
	BossCrystalBroken = 16,
	/** The boss's status-bar breaks this fight compare to Value. */
	BossStatusBreaks = 17,
	/** TODO: counted once infusion hits report per target; never holds for now. */
	BossInfusionHits = 18,
	/** How many phases the boss has reached compare to Value (a phase rule reads "Equal" its index). */
	BossPhasesReached = 19,
	// ---- The fight (B-337b): read from FWoRConditionContext's fight fields ----
	/** Enemy is in the opposing team (a reaction: the faced character). */
	FacingEnemy = 20,
	/** Who's current HP compares to Value: an absolute amount, never a percent (the deckbuilder's
	 *  rule, Crown 2026-10-12). False when the context has no such fighter. */
	Hp = 21,
	/** B-365 (D-158): Self (the defender) has just made a Flawless Parry (FWoRConditionContext::bFlawlessParry). */
	FlawlessParry = 22,
	/** B-337d (Q-337c): Who's current HP as a percent of its max compares to Value (a whole percent); a fighter
	 *  with no max reads 0%. A team Who holds when any member does. */
	HpPercent = 23,
	/** B-337d: as HpPercent, for EP. */
	EpPercent = 24,
	/** B-337d: Who is (a team Who: includes) the defender of the defense event being checked. */
	Defender = 25,
	/** B-337d: never holds (a skill condition authored None, "Disabled"). */
	Disabled = 26,
};

/** Whose state a fight subject reads (the deckbuilder's Who). Append only. */
UENUM(BlueprintType)
enum class EWoRConditionWho : uint8
{
	/** The side the condition belongs to: a reaction's wearer, a boss rule's boss. */
	Self = 0,
	/** The one Self faces or attacked: a reaction's faced character. */
	Target = 1,
	/** B-337d: anyone on Self's team (Self too); holds when any member does. */
	SelfTeam = 2,
	/** B-337d: anyone on Target's team (Target too); holds when any member does. */
	TargetTeam = 3,
};

/** How a number compares to Value. */
UENUM(BlueprintType)
enum class EWoRConditionOperator : uint8
{
	AtLeast = 0,
	AtMost = 1,
	Equal = 2,
	/** B-337d: strictly less (a skill's "HP below 30%"). */
	Below = 3,
	/** B-337d: strictly more (a skill's "HP above 30%"). */
	Above = 4,
};

/** The wearer against what an equipped item or skill requires, per pillar (was EMemoryRequirementFit). */
UENUM(BlueprintType)
enum class EWoRRequirementFit : uint8
{
	/** Requirements don't matter. */
	Any = 0,
	/** Every pillar meets them. */
	Met = 1,
	/** Met, and every pillar asked for is RequirementMargin or more over. */
	Above = 2,
	/** Some pillar is RequirementMargin (at least 1) or more short. */
	Under = 3,
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

	/** Hp / HpPercent / EpPercent / Defender: whose. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	EWoRConditionWho Who = EWoRConditionWho::Self;

	/** Numeric subjects (GearEquipped / EvolutionEquipped: tiers above the authored tier; Hp: HP; HpPercent /
	 *  EpPercent: a whole percent). */
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
