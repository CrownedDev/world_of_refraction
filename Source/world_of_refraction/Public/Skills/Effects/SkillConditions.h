// SkillConditions.h
// B-337d (D-130, Q-337c): a skill effect's conditions check through the one evaluator (WoRConditions).
// The authored FSkillCondition group (trigger, threshold, AND / OR, Self / SelfTeam / Target / TargetTeam)
// reads across here: an event trigger becomes the condition's Trigger, an HP / EP threshold an HpPercent /
// EpPercent subject (Below / Above, a whole percent), the side its Who, and the AND / OR group a list of sets
// any one of which holds (every AND with each OR). FSkillCondition stays the authored form until the effect
// assets are re-authored (B-337e).
// Three call sites check, and count different events as happening (ESkillCheck):
//  - State: USkillEffectManager's trigger and threshold checks; every non-defense event counts, as do the
//    plain defense outcomes (Take Damage, Dodge, Block, Guard); a perfect outcome or a Flawless Parry never.
//  - Defense: the impact path (FireDefenseTriggerEffects); a defense trigger holds when its event fired and its
//    Who defended; the other events count as in State.
//  - ActionResult: UActionExecutor's gate after an action; only Hit (damage dealt), Crit and Kill count, and
//    nothing reads HP / EP.
// B-337e: the effect assets carry the group in the one vocabulary (FSkillEffect::ConditionGroup, filled on load from
// the old field); every check reads it through GroupOf. The old-group functions stay for the runtime copy
// (FActiveSkillEffect::Conditions) and the parity spec until the sweep.
// Debug: GetGroupString; wor.Conditions.ListOld (GetUnmigratedEffectsString).

#pragma once

#include "CoreMinimal.h"
#include "Conditions/WoRConditions.h"
#include "Skills/Effects/FSkillCondition.h"
#include "Skills/Effects/SkillConditionGroup.h"

struct FSkillEffect;

enum class ESkillCheck : uint8
{
	State,
	Defense,
	ActionResult,
};

namespace SkillConditions
{
	/** A skill event as a condition trigger (Always, None and the thresholds: Any). */
	WORLD_OF_REFRACTION_API EWoRConditionTrigger TriggerFor(ESkillTrigger Trigger);

	WORLD_OF_REFRACTION_API EWoRConditionWho WhoFor(ECondSubject Subject);

	/** One authored condition in the one vocabulary. A Defense check reads a defense trigger as "Who defended". */
	WORLD_OF_REFRACTION_API FWoRCondition ToCondition(const FSkillCondition &Condition, ESkillCheck Check);

	/** Whether Condition takes part in Check: a Target-side one only with a target, never after an action. */
	WORLD_OF_REFRACTION_API bool Participates(const FSkillCondition &Condition, ESkillCheck Check, bool bHasTarget);

	/** The participating group as sets, any one of which holds: the ANDs, with each OR when there are any. */
	WORLD_OF_REFRACTION_API TArray<FWoRConditionSet> ToSets(const TArray<FSkillCondition> &Conditions, ESkillCheck Check, bool bHasTarget);

	/** The two legacy trigger fields (FActiveSkillEffect::TriggerCondition / SecondaryTriggerCondition) as a Self group. */
	WORLD_OF_REFRACTION_API TArray<FSkillCondition> FromLegacy(ESkillTrigger Primary, float PrimaryThreshold, ESkillTrigger Secondary,
															   float SecondaryThreshold, bool bRequireBoth);

	/** The events Check counts as happening; Fired: the action's or defense's own (ActionResult / Defense). */
	WORLD_OF_REFRACTION_API TArray<EWoRConditionTrigger> GetFiring(ESkillCheck Check, const TArray<ESkillTrigger> &Fired);

	/** Sets Context's trigger (Any) and AlsoFiring for Check. Its fighters are the caller's. */
	WORLD_OF_REFRACTION_API void PrepareContext(FWoRConditionContext &Context, ESkillCheck Check, const TArray<ESkillTrigger> &Fired);

	/** The group holds in Context (prepared for Check): empty or nothing participating holds. bHasTarget: the check has a
	 *  target actor (its Target-side conditions take part even when it has no fighter state, and then fail). */
	WORLD_OF_REFRACTION_API bool IsGroupMet(const TArray<FSkillCondition> &Conditions, ESkillCheck Check, const FWoRConditionContext &Context,
											bool bHasTarget);

	/** Debug: the group's sets as "a + b | c" in the one vocabulary. */
	WORLD_OF_REFRACTION_API FString GetGroupString(const TArray<FSkillCondition> &Conditions, ESkillCheck Check, bool bHasTarget);

	// ==================== B-337e: the authored group ====================

	/** The old group in the one vocabulary, check-neutral: each condition as ToCondition reads it for a State check,
	 *  the ANDs into AllOf and the ORs into AnyOf. */
	WORLD_OF_REFRACTION_API FSkillConditionGroup ToAuthored(const TArray<FSkillCondition> &Conditions);

	/** The group an effect checks: its authored ConditionGroup, or (an effect built in code with only the old field)
	 *  the old Conditions read across. */
	WORLD_OF_REFRACTION_API FSkillConditionGroup GroupOf(const FSkillEffect &Effect);

	/** A defense outcome event in the one vocabulary (as SkillTriggerUtils::IsDefenseOutcomeTrigger). */
	WORLD_OF_REFRACTION_API bool IsDefenseOutcome(EWoRConditionTrigger Trigger);

	/** Whether Condition takes part in Check: an owner-side one always, a target-side one only with a target, never
	 *  after an action (as Participates). */
	WORLD_OF_REFRACTION_API bool Participates(const FWoRCondition &Condition, ESkillCheck Check, bool bHasTarget);

	/** The group as sets for Check, any one of which holds (as ToSets on the old group): the impact path reads a
	 *  defense event as "Who defended". */
	WORLD_OF_REFRACTION_API TArray<FWoRConditionSet> ToSets(const FSkillConditionGroup &Group, ESkillCheck Check, bool bHasTarget);

	WORLD_OF_REFRACTION_API bool IsGroupMet(const FSkillConditionGroup &Group, ESkillCheck Check, const FWoRConditionContext &Context,
											bool bHasTarget);

	WORLD_OF_REFRACTION_API FString GetGroupString(const FSkillConditionGroup &Group, ESkillCheck Check, bool bHasTarget);

	/** Debug (wor.Conditions.ListOld): every effect asset whose effects this load filled from the old conditions,
	 *  i.e. not resaved since B-337e. First line: how many assets were scanned. Editor only. */
	WORLD_OF_REFRACTION_API FString GetUnmigratedEffectsString();
}
