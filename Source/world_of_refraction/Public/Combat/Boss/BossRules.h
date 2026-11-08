// BossRules.h
// B-355 (D-144): a boss has one rule list instead of separate phases and triggers. Each rule is
// when -> condition -> result, on D-130's shared condition set (FWoRConditionSet) and the deckbuilder's
// model; "go to the next phase" is one of the results. A phase now only holds what changes when it is
// reached (UBossData::Phases); the rules decide when.
// Old data maps across on read (BossRules::GetRules): each phase's conditions become a "next phase"
// rule that holds only at that phase, and each trigger becomes a rule with its results. Each rule
// fires at most once per fight. Debug: GetRuleString, wor.Boss.Print.

#pragma once

#include "CoreMinimal.h"
#include "Conditions/WoRCondition.h"
#include "Reaction/FightMoment.h"
#include "BossRules.generated.h"

class AActor;
class UBossData;
struct FBossFightState;
struct FBossPhase;
struct FBossTrigger;
struct FWoRConditionContext;

/** When a rule is checked. Append only. */
UENUM(BlueprintType)
enum class EBossRuleWhen : uint8
{
	/** Once, as the fight starts. */
	FightStart = 0,
	/** Whenever the boss's state changes (HP, a turn starts, a status break): where phases change. */
	StateChange = 1,
	/** At Moment (a fight moment, B-212), fired by UFightTriggers from the one list (B-355b); its conditions are read then. */
	AtMoment = 2
};

/** What a rule does. Append only. */
UENUM(BlueprintType)
enum class EBossRuleResult : uint8
{
	/** Enter the next phase not reached yet (its changes from UBossData::Phases). */
	NextPhase = 0,
	Weaken = 1,
	Strengthen = 2,
	/** Skip the next phase not reached yet (at most once per fight, Q-211). */
	SkipPhase = 3,
	/** Add the memory last-stand phase at the end (at most once per fight, Q-211). */
	AddPhase = 4,
	/** Play DialogueId (logged until the dialogue system, B-194). */
	Dialogue = 5
};

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FBossRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	FName RuleId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	EBossRuleWhen When = EBossRuleWhen::StateChange;

	/** AtMoment only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule", meta = (EditCondition = "When == EBossRuleWhen::AtMoment"))
	FFightMoment Moment;

	/** All must hold. Empty: never fires (as before for phases and triggers). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	FWoRConditionSet Conditions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	TArray<EBossRuleResult> Results;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	FName DialogueId;
};

namespace BossRules
{
	/** Data's authored rules, then its phases' conditions and its triggers mapped across (old data). */
	WORLD_OF_REFRACTION_API TArray<FBossRule> GetRules(const UBossData &Data);

	/** The "next phase" rule of a phase at PhaseIndex: its conditions plus "the boss has reached
	 *  exactly PhaseIndex phases" (phases go in order). False for a phase with no conditions. */
	WORLD_OF_REFRACTION_API bool MakePhaseRule(const FBossPhase &Phase, int32 PhaseIndex, FBossRule &OutRule);

	/** An old trigger (OBSOLETE) as a rule: fight start or its moment, its conditions and results. */
	WORLD_OF_REFRACTION_API FBossRule MakeTriggerRule(const FBossTrigger &Trigger);

	/** Its conditions are authored and all hold in Context. */
	WORLD_OF_REFRACTION_API bool Fires(const FBossRule &Rule, const FWoRConditionContext &Context);

	/** Rule's results on Boss: phases through UBossPhaseComponent, dialogue logged, weaken /
	 *  strengthen into State.NetSteps (the caller applies the net). True when it moved steps. */
	WORLD_OF_REFRACTION_API bool ApplyResults(AActor *Boss, const FBossRule &Rule, FBossFightState &State);

	/** Debug: one rule in words. */
	WORLD_OF_REFRACTION_API FString GetRuleString(const FBossRule &Rule);
}
