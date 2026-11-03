// BalanceSimRecorder.h
// B-292: fills one FBalanceFightRecord from the fight's existing broadcasts (damage, kills, turns, rounds, status
// breaks). Dynamic delegates need a UFUNCTION target, hence a UObject. Bind once per world, Begin() per fight.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Balance/BalanceSimTypes.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/TurnManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "BalanceSimRecorder.generated.h"

UCLASS(Transient)
class UBalanceSimRecorder : public UObject
{
	GENERATED_BODY()

public:
	FBalanceFightRecord Record;
	/** The fight's end state from its result (the orchestrator may already be Idle again when the tick returns). */
	TOptional<ECombatState> FinalState;

	void Listen(UActionExecutor *Executor, ACombatOrchestrator *Orchestrator, UTurnManager *Turns, UStatusBuildupManager *Buildup)
	{
		if (Executor)
		{
			Executor->OnDamageDealt.AddUniqueDynamic(this, &UBalanceSimRecorder::RecordDamage);
			Executor->OnTargetKilled.AddUniqueDynamic(this, &UBalanceSimRecorder::RecordKill);
		}
		if (Orchestrator)
		{
			Orchestrator->OnActorTurnStarted.AddUniqueDynamic(this, &UBalanceSimRecorder::RecordTurn);
			Orchestrator->OnCombatResultReady.AddUniqueDynamic(this, &UBalanceSimRecorder::RecordResult);
		}
		if (Turns)
		{
			Turns->OnRoundAdded.AddUniqueDynamic(this, &UBalanceSimRecorder::RecordRound);
		}
		if (Buildup)
		{
			Buildup->OnStatusTriggered.AddUniqueDynamic(this, &UBalanceSimRecorder::RecordStatusBreak);
		}
	}

	/** A fresh record for the next fight; Party tells the sides apart. */
	void Begin(FName MatchupId, int32 Seed, const TArray<AActor *> &InParty)
	{
		Record = FBalanceFightRecord();
		Record.MatchupId = MatchupId;
		Record.Seed = Seed;
		Party.Reset();
		for (AActor *Member : InParty)
		{
			Party.Add(Member);
		}
		HitsTaken.Reset();
		FinalState.Reset();
	}

	UFUNCTION()
	void RecordDamage(AActor *Attacker, AActor *Target, int32 Damage, bool bCritical)
	{
		if (Damage <= 0)
		{
			return;
		}
		if (Attacker) // no attacker (damage over time, status): counted as a hit, on neither side
		{
			(IsParty(Attacker) ? Record.PartyDamage : Record.EnemyDamage) += Damage;
		}
		++Record.Hits;
		Record.Crits += bCritical ? 1 : 0;
		Record.DamageSequence.Add(Damage);
		++HitsTaken.FindOrAdd(Target);
	}

	UFUNCTION()
	void RecordKill(AActor *Killer, AActor *Victim)
	{
		Record.HitsToKill.Add(HitsTaken.FindRef(Victim));
	}

	UFUNCTION()
	void RecordTurn(AActor *Actor, int32 TurnNumber)
	{
		++Record.Turns;
		Record.PartyTurns += IsParty(Actor) ? 1 : 0;
	}

	UFUNCTION()
	void RecordResult(const FCombatResult &Result) { FinalState = Result.FinalState; }

	UFUNCTION()
	void RecordRound() { ++Record.Rounds; }

	UFUNCTION()
	void RecordStatusBreak(AActor *Target, AActor *Source, ESkillEffectType StatusType, ESpellElement Element) { ++Record.StatusBreaks; }

private:
	bool IsParty(const AActor *Actor) const
	{
		return Party.ContainsByPredicate([Actor](const TWeakObjectPtr<AActor> &Member) { return Member.Get() == Actor; });
	}

	TArray<TWeakObjectPtr<AActor>> Party;
	TMap<TWeakObjectPtr<AActor>, int32> HitsTaken;
};
