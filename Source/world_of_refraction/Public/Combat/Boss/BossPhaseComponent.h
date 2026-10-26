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
#include "Components/ActorComponent.h"
#include "BossPhaseComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossPhaseReached, AActor *, Boss, int32, PhaseIndex);

UCLASS(ClassGroup = (Combat))
class WORLD_OF_REFRACTION_API UBossPhaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossPhaseComponent();

	/** Take Data's phases (authored or preset) and start listening. Call once, before the fight. */
	void Setup(UBossData *Data);

	/** Reach every next phase whose conditions all hold now. */
	void EvaluatePhases();

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
	bool IsConditionMet(const FBossPhaseCondition &Condition) const;
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

	int32 ReachedPhaseCount = 0;
	int32 OwnTurnsTaken = 0;
	int32 StatusBreakCount = 0;
	bool bEvaluating = false;
};
