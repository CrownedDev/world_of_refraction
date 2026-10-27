// BossTriggers.h
// B-211 (D-100 part 1, Q-211): a boss's own "if these conditions, then this" (UBossData::Triggers),
// checked once at fight start from what's true in the fight: who is in the party, what they wear
// and its level. Not memory-based. Weaken / strengthen from every fired trigger add up to one
// capped change (ReactionTriggers::ApplyBossSteps); skip / add phase go to UBossPhaseComponent;
// a dialogue line is logged until the dialogue system (B-194).
// Debug: wor.Boss.Print lists each boss's triggers.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UBossData;
struct FBossTrigger;
struct FBossTriggerCondition;
struct FMemoryEvent;

namespace BossTriggers
{
	/** Condition holds for the party in Event (its members and their worn gear). */
	WORLD_OF_REFRACTION_API bool IsMet(const FBossTriggerCondition &Condition, const FMemoryEvent &Event);

	/** Every condition holds (none: never fires). */
	WORLD_OF_REFRACTION_API bool Fires(const FBossTrigger &Trigger, const FMemoryEvent &Event);

	/** Apply every trigger of Data that fires for Party to Boss. Returns how many fired. */
	WORLD_OF_REFRACTION_API int32 ApplyTo(AActor *Boss, const UBossData &Data, const TArray<AActor *> &Party);

	/** ApplyTo for each opposing combatant with boss data. Returns how many triggers fired. */
	WORLD_OF_REFRACTION_API int32 Apply(const TArray<AActor *> &Party, const TArray<AActor *> &Opposing);
}
