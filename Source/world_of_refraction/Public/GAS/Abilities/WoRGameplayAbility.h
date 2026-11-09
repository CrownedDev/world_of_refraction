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
// B-043d (Q-043d): while it runs an action whose skill fires projectiles, a WaitGameplayEvent(Event.Combat.Projectile)
// task (Arrived and Dodged, one event per projectile) takes each projectile's end from the executor
// (UActionExecutor::TakeRoutedProjectile) and resolves it through UActionExecutor::ResolveProjectileArrival. Spawning,
// flight and the burst timer stay in the executor.
// B-043e (Q-043e): while it runs an action whose skill resolves an AOE or Instant cast entry, or a support cast, a
// WaitGameplayEvent(Event.Combat.SpellResolve) task (AOE, Instant, Support; one event per AOE / Instant defender, one
// per support cast) takes each resolution from the executor (UActionExecutor::TakeRoutedSpellResolve) and resolves
// it through UActionExecutor::ResolveRoutedSpell. The effects stay spawned in the executor. SelfCast / TargetCast
// abilities have no resolution point of their own: their hits route as impacts (B-043c), their casts as these.
// Granting and the orchestrator hand-off: GAS/Abilities/WoRSkillAbilities.h. Debug: wor.Abilities.Print.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Combat/Actions/ActionExecutor.h"
#include "WoRGameplayAbility.generated.h"

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

	/** B-043d: true while this instance runs an action and its WaitGameplayEvent(Event.Combat.Projectile) task waits. */
	bool IsListeningForProjectiles() const;

	/** B-043d: projectile ends (arrivals and dodges) this instance took and resolved, over its life (debug, spec). */
	int32 GetProjectileEventsResolved() const { return ProjectileArrivalsResolved + ProjectileDodgesResolved; }
	int32 GetProjectileArrivalsResolved() const { return ProjectileArrivalsResolved; }
	int32 GetProjectileDodgesResolved() const { return ProjectileDodgesResolved; }

	/** B-043d: whether Action's projectiles route through its ability: its skill (spell or not) has a Projectile
	 *  cast entry, or is a spell with no cast entries whose loose delivery is Projectile. AOE / Instant: B-043e. */
	static bool ShouldListenForProjectiles(const FAction &Action);

	/** B-043e: true while this instance runs an action and its WaitGameplayEvent(Event.Combat.SpellResolve) task waits. */
	bool IsListeningForSpellResolves() const;

	/** B-043e: AOE / Instant / Support resolutions this instance took and resolved, over its life (debug, spec). */
	int32 GetSpellResolvesResolved() const { return AOEResolves + InstantResolves + SupportResolves; }
	int32 GetAOEResolves() const { return AOEResolves; }
	int32 GetInstantResolves() const { return InstantResolves; }
	int32 GetSupportResolves() const { return SupportResolves; }

	/** B-043e: whether Action's flightless resolutions route through its ability: its skill (spell or not) has cast
	 *  entries and is a support skill (not enemy-targeted) or has an AOE / Instant entry; or it is a spell with no
	 *  cast entries that is a support spell or whose loose delivery is AOE / Instant. */
	static bool ShouldListenForSpellResolves(const FAction &Action);

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

	/** B-043d: starts the WaitGameplayEvent(Event.Combat.Projectile) task for the running action. */
	void StartProjectileListener();

	/** B-043d: one projectile's end (Arrived or Dodged, Target = the struck actor), taken from the executor and
	 *  resolved as OnProjectileImpact / OnProjectileDodged would. */
	UFUNCTION()
	void HandleProjectileEvent(FGameplayEventData Payload);

	/** B-043e: starts the WaitGameplayEvent(Event.Combat.SpellResolve) task for the running action. */
	void StartSpellResolveListener();

	/** B-043e: one AOE / Instant / Support resolution (kind from the tag, Target = the event target), taken from the
	 *  executor and resolved as SpawnAOEEffect / ResolveInstantSpell / SpawnSupportSpellEffect would. */
	UFUNCTION()
	void HandleSpellResolveEvent(FGameplayEventData Payload);

	/** B-043c/d/e: ends and forgets the running action's event tasks. */
	void EndEventTasks();

	/** B-043c/d: whether the executor's current action is this instance's running one (own-action check). */
	bool IsExecutorOnRunningAction(const UActionExecutor &Executor) const;

	/** A UPROPERTY so its targets and skill asset stay referenced while the action runs. */
	UPROPERTY(Transient)
	FAction RunningAction;

	FOnActionComplete ForwardOnComplete;
	bool bActionRunning = false;

	/** The running action's impact task (B-043c), or null. */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ImpactTask;

	int32 ImpactEventsResolved = 0;

	/** The running action's projectile task (B-043d), or null. */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ProjectileTask;

	int32 ProjectileArrivalsResolved = 0;
	int32 ProjectileDodgesResolved = 0;

	/** The running action's spell-resolve task (B-043e), or null. */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> SpellResolveTask;

	int32 AOEResolves = 0;
	int32 InstantResolves = 0;
	int32 SupportResolves = 0;
};
