// WoRCombatEvents.cpp

#include "GAS/WoRCombatEvents.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayPrediction.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	int32 GWoRLogCombatEvents = 0;
	FAutoConsoleVariableRef CVarWoRLogCombatEvents(
		TEXT("wor.Events.Log"), GWoRLogCombatEvents,
		TEXT("1: log every Event.Combat.* GameplayEvent the combat code sends (B-042d)."),
		ECVF_Default);
}

namespace WoRCombatEvents
{
	bool Send(AActor *Target, const FGameplayTag &Event, AActor *Instigator, float Magnitude)
	{
		UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		if (!ASC || !Event.IsValid())
		{
			return false;
		}
		FGameplayEventData Payload;
		Payload.EventTag = Event;
		Payload.Instigator = Instigator;
		Payload.Target = Target;
		Payload.EventMagnitude = Magnitude;

		if (GWoRLogCombatEvents != 0)
		{
			UE_LOG(LogWoREffects, Display, TEXT("[CombatEvents] %s"), *GetEventDebugString(Payload));
		}
		// As UAbilitySystemBlueprintLibrary::SendGameplayEventToActor, without its error for actors with no ASC.
		FScopedPredictionWindow PredictionWindow(ASC, true);
		ASC->HandleGameplayEvent(Event, &Payload);
		return true;
	}

	FString GetEventDebugString(const FGameplayEventData &Payload)
	{
		return FString::Printf(TEXT("%s to %s from %s (magnitude %.2f)"), *Payload.EventTag.ToString(),
							   *GetNameSafe(Payload.Target.Get()), *GetNameSafe(Payload.Instigator.Get()),
							   Payload.EventMagnitude);
	}
}
