// WoRGameplayAbility.h
// B-043b (GAS stage 4 part 2, Q-043b): the one GameplayAbility class a combatant's skills are granted as.
// One spec per skill, the skill asset (USkillDataBase) as the spec's source object, its Ability.Shape.* tags
// on the spec (WoRSkillAbilities::GetShapeTags). Activating it runs the FAction handed over by
// FWoRSkillActivationScope through UActionExecutor::ExecuteActionAsync, and the ability ends when that action
// completes. The executor still pays the costs at commit (SpendEnergy, B-043a's cost GEs), so CheckCost is
// the executor's spendable-EP check and ApplyCost pays nothing: exactly one payment.
// Instanced per actor, server only (Q-043b). Later parts (B-043d..g) split it per delivery shape.
// B-043c (Q-043c): while it runs a non-spell action that lands impacts, a WaitGameplayEvent(Event.Combat.Impact)
// task takes the action's impacts: the executor sends each Impact notify (or montage-less impact) to the avatar as
// that event (index in EventMagnitude) and the task resolves it through UActionExecutor::ResolveImpactFrame. The
// montage chain itself stays in the executor (PlayMontageAndWait waits for a later part).
// B-043d (Q-043d): while it runs an action whose skill fires projectiles at enemies, WaitGameplayEvent tasks take
// Event.Combat.Projectile.Arrived / .Dodged (Target = the target, cast entry index in EventMagnitude) and resolve
// them through the executor's same arrival / dodge code; a burst's stagger runs as a WaitDelay chain here.
// Spawning the projectile stays in the executor (presentation).
// Granting and the orchestrator hand-off: GAS/Abilities/WoRSkillAbilities.h. Debug: wor.Abilities.Print.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Combat/Actions/ActionExecutor.h"
#include "WoRGameplayAbility.generated.h"

class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;
class USkillDataBase;

/** While alive, an activation of Handle runs Action and reports to OnComplete. Lives on the caller's stack
 *  around one TryActivateAbility (WoRSkillAbilities::TryExecuteThroughAbility): TryActivateAbility carries
 *  no payload, and CheckCost (inside CanActivateAbility) never sees gameplay-event data (Q-043b). */
struct WORLD_OF_REFRACTION_API FWoRSkillActivationScope
{
	FWoRSkillActivationScope(FGameplayAbilitySpecHandle InHandle, const FAction &InAction, FOnActionComplete InOnComplete);
	~FWoRSkillActivationScope();

	FWoRSkillActivationScope(const FWoRSkillActivationScope &) = delete;
	FWoRSkillActivationScope &operator=(const FWoRSkillActivationScope &) = delete;

	/** The innermost scope for Handle, or null. */
	static FWoRSkillActivationScope *Find(FGameplayAbilitySpecHandle Handle);

	FGameplayAbilitySpecHandle Handle;
	const FAction &Action;
	FOnActionComplete OnComplete;
	/** Set by the ability once it took the action: the caller must not run it again. */
	bool bConsumed = false;

private:
	FWoRSkillActivationScope *Outer = nullptr;
	static FWoRSkillActivationScope *Innermost;
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UWoRGameplayAbility();

	/** The skill this spec was granted for (its source object), or null. */
	USkillDataBase *GetSkill(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo) const;

	/** The action this instance is running, or null when idle. */
	const FAction *GetRunningAction() const { return bActionRunning ? &RunningAction : nullptr; }

	/** B-043c: true while this instance runs an action and its WaitGameplayEvent(Event.Combat.Impact) task waits. */
	bool IsListeningForImpacts() const;

	/** B-043c: impacts this instance took as Event.Combat.Impact and resolved, over its life (debug, spec). */
	int32 GetImpactEventsResolved() const { return ImpactEventsResolved; }

	/** B-043c: whether Action's impacts route through its ability: a non-spell action whose skill lands impacts. */
	static bool ShouldListenForImpacts(const FAction &Action);

	/** B-043d: whether Action's projectile arrivals route through its ability: its skill strikes an enemy and has
	 *  a Projectile delivery (Ability.Shape.Projectile). */
	static bool ShouldListenForProjectiles(const FAction &Action);

	/** B-043d: true while this instance runs an action and its projectile tasks wait. */
	bool IsListeningForProjectiles() const;

	/** B-043d: projectile arrivals and dodges this instance took as events and resolved, over its life. */
	int32 GetProjectileEventsResolved() const { return ProjectileEventsResolved; }

	/** B-043d: burst projectiles this instance's WaitDelay chain spawned, over its life. */
	int32 GetBurstSpawnsDriven() const { return BurstSpawnsDriven; }

	/** B-043d: while listening for projectiles, runs the executor's burst stagger as a WaitDelay chain (one
	 *  SpawnNextBurstProjectile per Interval while the executor's queue holds any). False when not listening:
	 *  the executor runs its own timer. */
	bool StartBurstChain(float Interval);

	/** EP check only: the executor's spendable EP against the action's cost (the action being activated, else
	 *  the skill's plain cast, WoRSkillAbilities::MakePlainAction). */
	/** Refused while an earlier action still runs (the ability was cancelled before it finished): that action
	 *  keeps its own caller, and the new one runs the old call. */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo,
									const FGameplayTagContainer *SourceTags = nullptr,
									const FGameplayTagContainer *TargetTags = nullptr,
									FGameplayTagContainer *OptionalRelevantTags = nullptr) const override;

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo,
						   FGameplayTagContainer *OptionalRelevantTags = nullptr) const override;

	/** Pays nothing: the executor pays the EP at commit (and the backlash after the action), once. */
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo,
						   const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo,
								 const FGameplayAbilityActivationInfo ActivationInfo,
								 const FGameplayEventData *TriggerEventData) override;

	/** B-043d: ended while its action still runs (cancelled) mid-burst: the executor's timer takes the burst back,
	 *  so the rest still spawns and the action's windows can close. */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo,
							const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility,
							bool bWasCancelled) override;

private:
	/** The executor's completion: ends this ability, then hands the result on to whoever started the action. */
	void HandleActionComplete(const FActionResult &Result);

	/** Ends the ability with a failed result handed on (no executor, no action, the commit failed). */
	void FailActivation(const FString &Reason);

	/** B-043c: starts the WaitGameplayEvent(Event.Combat.Impact) task for the running action. */
	void StartImpactListener();

	/** B-043c: one impact (index in EventMagnitude), resolved by the executor as the Impact notify would. */
	UFUNCTION()
	void HandleImpactEvent(FGameplayEventData Payload);

	/** A UPROPERTY so its targets and skill asset stay referenced while the action runs. */
	UPROPERTY(Transient)
	FAction RunningAction;

	FOnActionComplete ForwardOnComplete;
	bool bActionRunning = false;

	/** The running action's impact task (B-043c), or null. */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ImpactTask;

	int32 ImpactEventsResolved = 0;

	/** B-043d: starts the Projectile.Arrived / Projectile.Dodged tasks for the running action. */
	void StartProjectileListeners();

	/** B-043d: one arrival (Target, cast entry index in EventMagnitude), resolved as the projectile's own
	 *  impact callback would. */
	UFUNCTION()
	void HandleProjectileArrivedEvent(FGameplayEventData Payload);

	/** B-043d: one dodged projectile, counted as the projectile's own dodge callback would. */
	UFUNCTION()
	void HandleProjectileDodgedEvent(FGameplayEventData Payload);

	/** B-043d: true when this instance should resolve Payload (the chosen listener, its own action running). */
	bool AcceptsProjectileEvent(const FGameplayEventData &Payload) const;

	/** B-043d: arms the next burst delay. */
	void ArmBurstDelay();

	/** B-043d: one burst spawn, then the next delay while the executor's queue holds any. */
	UFUNCTION()
	void HandleBurstDelayFinished();

	/** Ends the running action's tasks (impact, projectile, burst). */
	void EndActionTasks();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ProjectileArrivedTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ProjectileDodgedTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> BurstDelayTask;

	float BurstInterval = 0.0f;
	int32 ProjectileEventsResolved = 0;
	int32 BurstSpawnsDriven = 0;
};
