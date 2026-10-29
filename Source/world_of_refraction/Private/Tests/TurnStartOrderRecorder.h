// TurnStartOrderRecorder.h
// Test-only listener for the turn-start steps (ET-17, B-284). Dynamic delegates need a UFUNCTION
// target, so a spec binds this to each step's own signal and reads back the order they came in.
// One actor is watched; signals about anyone else are ignored.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "TurnStartOrderRecorder.generated.h"

/** A turn-start step, by the signal it gives. */
enum class ETurnStartStep : uint8
{
	OwnTurnStartEffect, // ET-17 step 2: a start-of-own-turn effect ran (OnEffectTriggered)
	AlwaysOnEffect,     // step 3: an always-on effect ran (OnEffectTriggered)
	TurnStartTrigger,   // step 4: an "on turn start" trigger paid out (its EP gain, OnEPChanged)
	BarDecay,           // step 5: the status bar decayed (OnStatusBuildupChanged)
	OverloadTick,       // step 6: the Broken Darkness overload ticked (OnOverloadDamage)
	TurnStartedBroadcast,
	ActionRequested
};

inline const TCHAR *GetTurnStartStepName(ETurnStartStep Step)
{
	switch (Step)
	{
	case ETurnStartStep::OwnTurnStartEffect: return TEXT("start-of-own-turn effect");
	case ETurnStartStep::AlwaysOnEffect: return TEXT("always-on effect");
	case ETurnStartStep::TurnStartTrigger: return TEXT("on-turn-start trigger");
	case ETurnStartStep::BarDecay: return TEXT("status-bar decay");
	case ETurnStartStep::OverloadTick: return TEXT("Broken Darkness overload tick");
	case ETurnStartStep::TurnStartedBroadcast: return TEXT("turn-started broadcast");
	case ETurnStartStep::ActionRequested: return TEXT("action request");
	default: return TEXT("unknown step");
	}
}

UCLASS(Transient)
class UTurnStartOrderRecorder : public UObject
{
	GENERATED_BODY()

public:
	/** Every signal from the watched actor, in arrival order. */
	TArray<ETurnStartStep> Steps;

	/** Whether the watched actor's free weapon / ring switch was back when its first
	 *  start-of-own-turn effect ran (step 1, the per-turn flags, came first). */
	bool bSwitchFreeAtOwnTurnStartEffect = false;

	/** Binds every step's signal. Energy is recorded as the trigger's payout only when
	 *  bEnergyIsTrigger (nothing else at turn start may change it then); BrokenDarkness may be null. */
	void Listen(AActor *Actor, int32 InOwnTurnStartEffectId, int32 InAlwaysOnEffectId, bool bEnergyIsTrigger,
				ACombatOrchestrator *Orchestrator, USkillEffectManager *Effects, UStatusBuildupManager *Buildup,
				UBrokenDarknessManager *BrokenDarkness)
	{
		Watched = Actor;
		OwnTurnStartEffectId = InOwnTurnStartEffectId;
		AlwaysOnEffectId = InAlwaysOnEffectId;
		WatchedLoadout = Actor ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr;

		Effects->OnEffectTriggered.AddDynamic(this, &UTurnStartOrderRecorder::HandleEffectTriggered);
		Buildup->OnStatusBuildupChanged.AddDynamic(this, &UTurnStartOrderRecorder::HandleBuildupChanged);
		Orchestrator->OnActorTurnStarted.AddDynamic(this, &UTurnStartOrderRecorder::HandleActorTurnStarted);
		Orchestrator->OnActionRequested.AddDynamic(this, &UTurnStartOrderRecorder::HandleActionRequested);
		UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (CharComp && bEnergyIsTrigger)
		{
			CharComp->OnEPChanged.AddDynamic(this, &UTurnStartOrderRecorder::HandleEnergyChanged);
		}
		if (BrokenDarkness)
		{
			BrokenDarkness->OnOverloadDamage.AddDynamic(this, &UTurnStartOrderRecorder::HandleOverloadDamage);
		}
	}

	/** Index of the step's first signal, INDEX_NONE when it never came. */
	int32 FirstIndexOf(ETurnStartStep Step) const { return Steps.IndexOfByKey(Step); }

	UFUNCTION()
	void HandleEffectTriggered(AActor *Target, const FActiveSkillEffect &Effect)
	{
		if (Target != Watched.Get())
		{
			return;
		}
		if (Effect.EffectID == OwnTurnStartEffectId)
		{
			if (!Steps.Contains(ETurnStartStep::OwnTurnStartEffect))
			{
				const ULoadoutComponent *Loadout = WatchedLoadout.Get();
				bSwitchFreeAtOwnTurnStartEffect = Loadout && Loadout->GetTurnSwitchRefusedReason().IsEmpty();
			}
			Steps.Add(ETurnStartStep::OwnTurnStartEffect);
		}
		else if (Effect.EffectID == AlwaysOnEffectId)
		{
			Steps.Add(ETurnStartStep::AlwaysOnEffect);
		}
	}

	UFUNCTION()
	void HandleEnergyChanged(int32 CurrentEP, int32 MaxEP) { Steps.Add(ETurnStartStep::TurnStartTrigger); }

	UFUNCTION()
	void HandleBuildupChanged(AActor *Target, float CurrentBuildup, float MaxBuildup, ESpellElement PendingElement, AActor *Source)
	{
		if (Target == Watched.Get())
		{
			Steps.Add(ETurnStartStep::BarDecay);
		}
	}

	UFUNCTION()
	void HandleOverloadDamage(AActor *Source, AActor *Target, float Damage)
	{
		if (Source == Watched.Get())
		{
			Steps.Add(ETurnStartStep::OverloadTick);
		}
	}

	UFUNCTION()
	void HandleActorTurnStarted(AActor *Actor, int32 TurnNumber)
	{
		if (Actor == Watched.Get())
		{
			Steps.Add(ETurnStartStep::TurnStartedBroadcast);
		}
	}

	UFUNCTION()
	void HandleActionRequested(AActor *Actor)
	{
		if (Actor == Watched.Get())
		{
			Steps.Add(ETurnStartStep::ActionRequested);
		}
	}

private:
	TWeakObjectPtr<AActor> Watched;
	TWeakObjectPtr<ULoadoutComponent> WatchedLoadout;
	int32 OwnTurnStartEffectId = INDEX_NONE;
	int32 AlwaysOnEffectId = INDEX_NONE;
};
