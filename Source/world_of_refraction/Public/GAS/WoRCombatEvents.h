// WoRCombatEvents.h
// GAS stage 3 (B-042d, Q-044j A): the effect manager keeps its trigger dispatcher and also sends
// these GameplayEvents to the actor they happen to, so stage 4 (B-043) can move triggers onto
// passive abilities (WaitGameplayEvent) without touching the senders:
// - Event.Combat.Hit: to the target of damage; Instigator = attacker, EventMagnitude = damage.
// - Event.Combat.DefenseResolved: to the defender; Instigator = attacker, EventMagnitude = the
//   attack's base energy cost.
// - Event.Combat.TurnStart / TurnEnd: to the actor whose turn it is.
// An actor with no ability system component gets nothing (no error).
// Debug: GetEventDebugString; `wor.Events.Log 1` logs every event sent.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AActor;
struct FGameplayEventData;

namespace WoRCombatEvents
{
	/** Sends Event to Target's ability system component. False when Target has none. */
	WORLD_OF_REFRACTION_API bool Send(AActor *Target, const FGameplayTag &Event, AActor *Instigator = nullptr,
									  float Magnitude = 0.0f);

	/** "Event.Combat.Hit to X from Y (magnitude 12.00)". */
	WORLD_OF_REFRACTION_API FString GetEventDebugString(const FGameplayEventData &Payload);
}
