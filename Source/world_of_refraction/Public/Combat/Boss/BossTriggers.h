// BossTriggers.h
// B-211 (D-100 part 1, Q-211): a boss's own "if these conditions, then this" (UBossData::Triggers),
// checked once at fight start from what's true in the fight: who is in the party, what they wear
// and its level. Not memory-based. Weaken / strengthen from every fired trigger add up to one
// capped change (ReactionTriggers::ApplyBossSteps); skip / add phase go to UBossPhaseComponent;
// a dialogue line is logged until the dialogue system (B-194). B-212: only fight-start triggers
// fire here; the rest wait in UFightTriggers for their When. B-355b: those are the one list's AtMoment rules,
// fired through FireMomentRule.
// Debug: wor.Boss.Print lists each boss's triggers.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UBossData;
struct FBossTrigger;
struct FBossRule;
struct FBossTriggerCondition;
struct FMemoryEvent;

/** B-212: one boss's trigger results so far this fight: steps add up to one net change; skip and
 *  add phase happen at most once each (Q-211). */
struct FBossFightState
{
	int32 NetSteps = 0;
	bool bSkipped = false;
	bool bAdded = false;
};

namespace BossTriggers
{
	/** OBSOLETE (B-355, B-355b: no production caller; the specs still read them). ApplyTo and the moments read the
	 *  rule list (BossRules). Retire with FBossTrigger after Crown's PIE.
	 *  Condition holds for the party in Event (its members and their worn gear). */
	WORLD_OF_REFRACTION_API bool IsMet(const FBossTriggerCondition &Condition, const FMemoryEvent &Event);

	/** Every condition holds (none: never fires). */
	WORLD_OF_REFRACTION_API bool Fires(const FBossTrigger &Trigger, const FMemoryEvent &Event);

	/** Trigger's phase and dialogue results on Boss, and its steps into State.NetSteps (the caller
	 *  applies the net). True when it has a weaken or strengthen. */
	WORLD_OF_REFRACTION_API bool ApplyResults(AActor *Boss, const FBossTrigger &Trigger, FBossFightState &State);

	/** B-355: every fight-start rule of Data's one list (BossRules::GetRules) that fires for Party,
	 *  on Boss (the steps as one net).
	 *  Returns how many fired; OutState gets what they did. */
	WORLD_OF_REFRACTION_API int32 ApplyTo(AActor *Boss, const UBossData &Data, const TArray<AActor *> &Party,
										  FBossFightState *OutState = nullptr);

	/** B-355b: one AtMoment rule of the one list, now its moment came: when its conditions hold for Party (the boss as
	 *  Self, its phase state as Boss), its results go on Boss and its steps into State.NetSteps. Returns whether it
	 *  fired; bOutSteps: it moved steps (the caller applies the net). */
	WORLD_OF_REFRACTION_API bool FireMomentRule(AActor *Boss, const FBossRule &Rule, const TArray<AActor *> &Party, FBossFightState &State,
												bool &bOutSteps);

	/** ApplyTo for each opposing combatant with boss data. Returns how many triggers fired. */
	WORLD_OF_REFRACTION_API int32 Apply(const TArray<AActor *> &Party, const TArray<AActor *> &Opposing);
}
