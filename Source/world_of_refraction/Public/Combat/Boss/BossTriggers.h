// BossTriggers.h
// B-211 (D-100 part 1, Q-211): a boss's own "if these conditions, then this" (UBossData::Triggers),
// checked once at fight start from what's true in the fight: who is in the party, what they wear
// and its level. Not memory-based. Weaken / strengthen from every fired trigger add up to one
// capped change (ReactionTriggers::ApplyBossSteps); skip / add phase go to UBossPhaseComponent;
// a dialogue line is logged until the dialogue system (B-194). B-212: only fight-start triggers
// fire here; the rest wait in UFightTriggers for their When.
// Debug: wor.Boss.Print lists each boss's triggers.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UBossData;
struct FBossTrigger;
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
	/** Condition holds for the party in Event (its members and their worn gear). */
	WORLD_OF_REFRACTION_API bool IsMet(const FBossTriggerCondition &Condition, const FMemoryEvent &Event);

	/** Every condition holds (none: never fires). */
	WORLD_OF_REFRACTION_API bool Fires(const FBossTrigger &Trigger, const FMemoryEvent &Event);

	/** Trigger's phase and dialogue results on Boss, and its steps into State.NetSteps (the caller
	 *  applies the net). True when it has a weaken or strengthen. */
	WORLD_OF_REFRACTION_API bool ApplyResults(AActor *Boss, const FBossTrigger &Trigger, FBossFightState &State);

	/** Apply every fight-start trigger of Data that fires for Party to Boss (the steps as one net).
	 *  Returns how many fired; OutState gets what they did. */
	WORLD_OF_REFRACTION_API int32 ApplyTo(AActor *Boss, const UBossData &Data, const TArray<AActor *> &Party,
										  FBossFightState *OutState = nullptr);

	/** ApplyTo for each opposing combatant with boss data. Returns how many triggers fired. */
	WORLD_OF_REFRACTION_API int32 Apply(const TArray<AActor *> &Party, const TArray<AActor *> &Opposing);
}
