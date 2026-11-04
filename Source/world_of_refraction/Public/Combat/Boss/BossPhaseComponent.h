// BossPhaseComponent.h
// B-193 (D-098, Q-193): a boss's phases during one fight (per-fight state on the boss, D-064).
// Checked whenever the boss's HP changes, a turn starts or its status bar breaks. A phase is
// reached in order once all its conditions hold, applies its changes and stays reached even if
// the boss heals back above the line; one big hit can pass several. Each status break raises the
// boss's status resistance for the rest of the fight. Added by ABattleGameMode for any opposing
// combatant with a UBossData. Debug: wor.Boss.Print.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Boss/BossData.h"
#include "Combat/Boss/BossRules.h"
#include "Combat/Boss/BossTriggers.h"
#include "Components/ActorComponent.h"
#include "BossPhaseComponent.generated.h"

struct FWoRBossState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossPhaseReached, AActor *, Boss, int32, PhaseIndex);

UCLASS(ClassGroup = (Combat))
class WORLD_OF_REFRACTION_API UBossPhaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossPhaseComponent();

	/** Take Data's phases (authored or preset) and start listening. Call once, before the fight. */
	void Setup(UBossData *Data);

	/** B-355 (D-144): fire the boss's state-change rules that hold now (each authored rule once per
	 *  fight; a phase's "next phase" rule while the boss is at that phase), repeating while one fires,
	 *  so one big hit can pass several phases. */
	void EvaluatePhases();

	/** B-355: the NextPhase result: enter the next phase not reached yet (none left: nothing). */
	void EnterNextPhase();

	/** B-355: the state the rules' boss subjects read (HP, own turns, breaks, phases reached, ...). */
	FWoRBossState MakeBossState() const;

	/** B-355: the boss's one per-fight rule state (net steps; skip / add once each, Q-211), shared by the
	 *  fight-start rules (BossTriggers::ApplyTo) and the state-change rules here, so a fight-start Strengthen
	 *  and a later Weaken net out and a skip happens once. Reset by Setup. */
	FBossFightState &GetFightState() { return RuleState; }

	/** A memory skips the next phase not reached yet (Q-191 SkipPhase). */
	void SkipNextPhase();

	/** A memory adds a phase at the end (Q-191 AddPhase). */
	void AddPhase(const FBossPhase &Phase);

	/** The boss's status bar broke: raises its status resistance, may reach a phase. */
	void RecordStatusBreak();

	int32 GetReachedPhaseCount() const { return ReachedPhaseCount; }
	int32 GetPhaseCount() const { return Phases.Num(); }
	int32 GetStatusBreakCount() const { return StatusBreakCount; }

	/** Added to the boss's status resistance: one step per break this fight. */
	float GetStatusResistanceBonus() const;

	/** AI steps from every reached phase. */
	int32 GetAggressionSteps() const;

	/** The weather's pull toward the boss: the highest of the reached phases, 0 before any. */
	float GetWeatherIntensity() const;

	FString GetBossPhaseString() const;

	UPROPERTY(BlueprintAssignable, Category = "Boss")
	FOnBossPhaseReached OnBossPhaseReached;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** OBSOLETE (B-355): the phase conditions are read as rules (BossRules::MakePhaseRule); not called. */
	bool IsConditionMet(const FBossPhaseCondition &Condition) const;

	/** One "next phase" rule per phase with conditions, by its current index (after a skip or add too). */
	void RebuildPhaseRules();
	void EnterPhase(int32 PhaseIndex);
	void UnbindAll();

	UFUNCTION()
	void HandleHPChanged(int32 CurrentHP, int32 MaxHP);

	UFUNCTION()
	void HandleTurnStarted(AActor *Actor, int32 TurnNumber);

	UPROPERTY()
	TObjectPtr<UBossData> BossData;

	UPROPERTY()
	TArray<FBossPhase> Phases;

	/** B-355: the boss's authored state-change rules, and which have fired this fight. */
	TArray<FBossRule> AuthoredRules;
	TArray<bool> AuthoredRuleFired;

	/** B-355: the phases' "next phase" rules (rebuilt from Phases). */
	TArray<FBossRule> PhaseRules;

	/** B-355: what the rules did this fight (net steps; skip / add once, Q-211); see GetFightState. */
	FBossFightState RuleState;

	int32 ReachedPhaseCount = 0;
	int32 OwnTurnsTaken = 0;
	int32 StatusBreakCount = 0;
	bool bEvaluating = false;
};
