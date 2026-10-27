// FightTriggers.h
// B-212 (Q-191 changed by Crown, Q-212): reactions and boss triggers whose When is mid-fight. Who
// matches is decided at fight start (as before); the trigger then waits here for its moment (a
// turn start, a hit, damage taken, an HP line crossed, a boss phase) and fires once. Per fight
// (D-064): owned by ABattleGameMode, made at fight start, unbound when the fight ends.
// Debug: GetFightTriggersString (logged at fight start).

#pragma once

#include "CoreMinimal.h"
#include "Combat/Boss/BossTriggers.h"
#include "Reaction/ReactionTrigger.h"
#include "UObject/Object.h"
#include "FightTriggers.generated.h"

class UBossData;
class UGameInstance;

UCLASS(Transient)
class WORLD_OF_REFRACTION_API UFightTriggers : public UObject
{
	GENERATED_BODY()

public:
	/** Find the reactions that match Party against Opposing. Returns the fight-start ones to apply
	 *  now (they point into this object's copy of Reactions); the rest wait. */
	TArray<FFiredReaction> StartReactions(const TArray<FReactionTrigger> &InReactions, const TArray<AActor *> &Party,
										  const TArray<AActor *> &Opposing);

	/** BossTriggers::ApplyTo for Boss (fight-start triggers now); the mid-fight ones whose
	 *  conditions hold wait. Returns how many fired now. */
	int32 StartBoss(AActor *Boss, const UBossData &Data, const TArray<AActor *> &Party);

	/** StartBoss for each opposing combatant with boss data. */
	int32 StartBosses(const TArray<AActor *> &Party, const TArray<AActor *> &Opposing);

	/** Listen (or stop) for the waiting triggers' moments: turns, damage, HP and boss phases. Call
	 *  after StartReactions / StartBosses: HP lines count from the HP then. */
	void Watch(bool bWatch);

	UFUNCTION()
	void HandleTurnStarted(AActor *Actor, int32 TurnNumber);

	UFUNCTION()
	void HandleDamageDealt(AActor *Attacker, AActor *Target, int32 Damage, bool bCritical);

	/** Any watched combatant's HP changed: check every HP line. */
	UFUNCTION()
	void HandleHPChanged(int32 CurrentHP, int32 MaxHP);

	UFUNCTION()
	void HandleBossPhaseReached(AActor *Boss, int32 PhaseIndex);

	int32 GetWaitingCount() const;
	int32 GetFiredMidFightCount() const { return FiredMidFight; }

	/** Debug: each waiting trigger and its moment. */
	FString GetFightTriggersString() const;

private:
	struct FWaitingTrigger
	{
		FFightMoment When;
		TArray<TWeakObjectPtr<AActor>> Ours;
		TArray<TWeakObjectPtr<AActor>> Theirs;
		/** HP lines: each subject's side of the line last seen (fires when it becomes met). */
		TMap<TWeakObjectPtr<AActor>, bool> WasPastLine;
		/** A reaction (Trigger set), or a boss trigger (BossTrigger set). */
		FFiredReaction Reaction;
		TWeakObjectPtr<AActor> Boss;
		const FBossTrigger *BossTrigger = nullptr;
		FName TriggerId;
		bool bFired = false;
	};

	const TArray<TWeakObjectPtr<AActor>> &GetSide(const FWaitingTrigger &Waiting) const;
	void FireMatching(EFightMoment Moment, const AActor *Subject);
	void CheckHPLines();
	void Fire(FWaitingTrigger &Waiting);
	void BindCombatant(AActor *Actor, bool bWatch);

	UPROPERTY()
	TArray<FReactionTrigger> Reactions;

	/** Keeps the boss data the waiting triggers point into. */
	UPROPERTY()
	TArray<TObjectPtr<const UBossData>> BossDatas;

	TArray<FWaitingTrigger> Waiting;
	TMap<TWeakObjectPtr<AActor>, FBossFightState> BossStates;
	TWeakObjectPtr<UGameInstance> GameInstance;
	bool bWatching = false;
	int32 FiredMidFight = 0;
};
