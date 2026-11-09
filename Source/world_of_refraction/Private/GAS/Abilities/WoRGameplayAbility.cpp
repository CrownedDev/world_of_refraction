// WoRGameplayAbility.cpp

#include "GAS/Abilities/WoRGameplayAbility.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/TargetType.h"
#include "Combat/WoRFightSubsystem.h"
#include "GAS/Abilities/WoRSkillAbilities.h"
#include "Skills/Definitions/ESpellDeliveryType.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Tags/WoRGameplayTags.h"
#include "WoRLog.h"

FWoRSkillActivationScope *FWoRSkillActivationScope::Innermost = nullptr;

FWoRSkillActivationScope::FWoRSkillActivationScope(FGameplayAbilitySpecHandle InHandle, const FAction &InAction,
												   FOnActionComplete InOnComplete)
	: Handle(InHandle), Action(InAction), OnComplete(MoveTemp(InOnComplete)), Outer(Innermost)
{
	Innermost = this;
}

FWoRSkillActivationScope::~FWoRSkillActivationScope()
{
	Innermost = Outer;
}

FWoRSkillActivationScope *FWoRSkillActivationScope::Find(FGameplayAbilitySpecHandle Handle)
{
	for (FWoRSkillActivationScope *Scope = Innermost; Scope; Scope = Scope->Outer)
	{
		// A scope whose action an ability already took is spent: a later activation of the same handle (from the
		// forwarded completion) must not replay it.
		if (Scope->Handle == Handle && !Scope->bConsumed)
		{
			return Scope;
		}
	}
	return nullptr;
}

UWoRGameplayAbility::UWoRGameplayAbility()
{
	// Q-043b: one instance per granted spec, reused each turn (Lyra's and GASDocumentation's default); the
	// orchestrator and executor run on the server, so the ability does too.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

USkillDataBase *UWoRGameplayAbility::GetSkill(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo) const
{
	return Cast<USkillDataBase>(GetSourceObject(Handle, ActorInfo));
}

bool UWoRGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo,
											  const FGameplayTagContainer *SourceTags, const FGameplayTagContainer *TargetTags,
											  FGameplayTagContainer *OptionalRelevantTags) const
{
	if (bActionRunning)
	{
		UE_LOG(LogWoR, Log, TEXT("[Abilities] %s: an earlier action still runs; not activating"), *GetNameSafe(GetSkill(Handle, ActorInfo)));
		return false;
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

bool UWoRGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo,
									FGameplayTagContainer *OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}
	AActor *Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UActionExecutor *Executor = Avatar ? WoRFight::Get<UActionExecutor>(Avatar) : nullptr;
	// As UActionExecutor::ValidateAction: no character data, no EP check.
	if (!Executor || !Avatar->FindComponentByClass<UCharacterDataComponent>())
	{
		return true;
	}
	const FWoRSkillActivationScope *Scope = FWoRSkillActivationScope::Find(Handle);
	USkillDataBase *Skill = GetSkill(Handle, ActorInfo);
	if (!Scope && !Skill)
	{
		return true;
	}
	// The action being activated; during CommitAbility (the scope is consumed by then) the one now running.
	const FAction Action = Scope ? Scope->Action : bActionRunning ? RunningAction : WoRSkillAbilities::MakePlainAction(Skill);
	const int32 Cost = Executor->CalculateActionEnergyCost(Avatar, Action);
	const int32 Spendable = Executor->GetSpendableEP(Avatar);
	if (Spendable >= Cost)
	{
		return true;
	}
	if (OptionalRelevantTags && UAbilitySystemGlobals::Get().ActivateFailCostTag.IsValid())
	{
		OptionalRelevantTags->AddTag(UAbilitySystemGlobals::Get().ActivateFailCostTag);
	}
	UE_LOG(LogWoR, Verbose, TEXT("[Abilities] %s can't afford %s: costs %d EP, %d spendable"), *Avatar->GetName(),
		   *GetNameSafe(Skill), Cost, Spendable);
	return false;
}

void UWoRGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo,
									const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// B-043f: only the commit CommitActionCost makes pays; no CostGameplayEffectClass is set, so Super pays nothing.
	if (PendingCommitCost == INDEX_NONE)
	{
		return;
	}
	AActor *Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UActionExecutor *Executor = Avatar ? WoRFight::Get<UActionExecutor>(Avatar) : nullptr;
	bCommitCostPaid = Executor && Executor->SpendEnergy(Avatar, PendingCommitCost);
}

bool UWoRGameplayAbility::CommitActionCost(int32 Cost)
{
	// Ended (cancelled) mid-action: the executor pays.
	if (!bActionRunning || !IsActive())
	{
		return false;
	}
	// B-043g (Q-043g): a ritual's fire was paid by its arm's commit; the executor's zero-cost pay is a no-op.
	if (RunningAction.bIsDeferredFire)
	{
		return false;
	}
	PendingCommitCost = Cost;
	bCommitCostPaid = false;
	const bool bCommitted = CommitAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
	const bool bPaid = bCommitted && bCommitCostPaid;
	PendingCommitCost = INDEX_NONE;
	bCommitCostPaid = false;
	if (bPaid)
	{
		++CostsCommitted;
	}
	UE_LOG(LogWoR, Verbose, TEXT("[Abilities] %s: %s commits %d EP (%s)"), *GetNameSafe(GetAvatarActorFromActorInfo()),
		   *RunningAction.GetActionName(), Cost, bPaid ? TEXT("paid") : TEXT("not paid"));
	return bPaid;
}

void UWoRGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo,
										  const FGameplayAbilityActivationInfo ActivationInfo,
										  const FGameplayEventData *TriggerEventData)
{
	FWoRSkillActivationScope *Scope = FWoRSkillActivationScope::Find(Handle);
	if (!Scope)
	{
		// Activated some other way (a debug TryActivateAbility): there is no action to run.
		UE_LOG(LogWoR, Log, TEXT("[Abilities] %s activated with no action; ending"), *GetNameSafe(GetSkill(Handle, ActorInfo)));
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
		return;
	}
	Scope->bConsumed = true;
	RunningAction = Scope->Action;
	ForwardOnComplete = Scope->OnComplete;
	bActionRunning = true;
	ImpactTask = nullptr;
	ProjectileArrivedTask = nullptr;
	ProjectileDodgedTask = nullptr;
	BurstDelayTask = nullptr;
	SpellResolveTask = nullptr;

	// No commit here (B-043f, Q-043f): TryActivateAbility already checked the cost (CheckCost), and the executor
	// commits it through CommitActionCost once its own gates have passed, so a refused action pays nothing.
	AActor *Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UActionExecutor *Executor = Avatar ? WoRFight::Get<UActionExecutor>(Avatar) : nullptr;
	if (!Executor)
	{
		FailActivation(TEXT("No action executor"));
		return;
	}

	if (RunningAction.bIsDeferredFire)
	{
		++RitualFiresRun;
	}
	UE_LOG(LogWoR, Verbose, TEXT("[Abilities] %s: %s runs through its ability%s"), *Avatar->GetName(),
		   *RunningAction.GetActionName(), RunningAction.bIsDeferredFire ? TEXT(" (ritual fire)") : TEXT(""));
	// Before the executor call: a montage-less action (wor.Combat.InstantImpacts) lands its impacts inside it.
	if (ShouldListenForImpacts(RunningAction))
	{
		StartImpactListener();
	}
	// B-043d: before the executor call too: a montage-less spell releases its cast inside it.
	if (ShouldListenForProjectiles(RunningAction))
	{
		StartProjectileListeners();
	}
	// B-043e: an AOE / Instant / support release resolves inside the call too (a montage-less spell).
	if (ShouldListenForSpellResolves(RunningAction))
	{
		StartSpellResolveListener();
	}
	// A copy: the completion can run inside this call and a bonus turn can start this ability again.
	const FAction Action = RunningAction;
	Executor->ExecuteActionAsync(Avatar, Action, FOnActionComplete::CreateUObject(this, &UWoRGameplayAbility::HandleActionComplete));
}

void UWoRGameplayAbility::HandleActionComplete(const FActionResult &Result)
{
	if (!bActionRunning)
	{
		return;
	}
	bActionRunning = false;
	RunningAction = FAction(); // drop its targets and skill until the next activation
	EndActionTasks();
	FOnActionComplete Forward = ForwardOnComplete;
	ForwardOnComplete.Unbind();
	// End first, so whoever started the action can start this ability again from the forwarded call
	// (a bonus turn). A cancelled ability (EndAbility already ran) still hands its action's result on.
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
				   /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
	}
	Forward.ExecuteIfBound(Result);
}

void UWoRGameplayAbility::FailActivation(const FString &Reason)
{
	UE_LOG(LogWoR, Warning, TEXT("[Abilities] %s: %s did not start (%s)"), *GetNameSafe(GetAvatarActorFromActorInfo()),
		   *RunningAction.GetActionName(), *Reason);
	FActionResult Fail;
	Fail.bSuccess = false;
	Fail.ActionType = RunningAction.ActionType;
	Fail.Executor = GetAvatarActorFromActorInfo();
	Fail.ErrorMessage = Reason;
	HandleActionComplete(Fail);
}

bool UWoRGameplayAbility::ShouldListenForImpacts(const FAction &Action)
{
	// A spell's hits arrive with its casts (projectiles, AOE, instant: B-043d/e); its Impact notifies, if any, keep
	// the old path. Every other skill action (melee, ranged and cast abilities alike) lands its impacts here.
	const USkillDataBase *Skill = Action.ActionType == EActionType::Ability ? Action.SkillData : nullptr;
	return Skill && Skill->HitCount > 0;
}

bool UWoRGameplayAbility::IsListeningForImpacts() const
{
	return bActionRunning && IsActive() && ImpactTask && ImpactTask->IsActive();
}

void UWoRGameplayAbility::StartImpactListener()
{
	// Q-043c: every impact of the action (not OnlyTriggerOnce), the exact tag (Lyra's and GASDocumentation's
	// montage-plus-gameplay-event pattern).
	ImpactTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, WoRTags::Event_Combat_Impact,
																   /*OptionalExternalTarget*/ nullptr,
																   /*OnlyTriggerOnce*/ false, /*OnlyMatchExact*/ true);
	if (!ImpactTask)
	{
		return; // the executor resolves the impacts itself
	}
	ImpactTask->EventReceived.AddDynamic(this, &UWoRGameplayAbility::HandleImpactEvent);
	ImpactTask->ReadyForActivation();
}

void UWoRGameplayAbility::HandleImpactEvent(FGameplayEventData Payload)
{
	if (!bActionRunning)
	{
		return;
	}
	AActor *Avatar = GetAvatarActorFromActorInfo();
	// The event reaches every impact task on this ASC; only the listener the sender chose resolves it, so two
	// listening abilities on one actor could never land one impact twice.
	if (WoRSkillAbilities::FindImpactListener(Avatar) != this)
	{
		return;
	}
	UActionExecutor *Executor = Avatar ? WoRFight::Get<UActionExecutor>(Avatar) : nullptr;
	if (!Executor || Executor->GetCurrentSkillData() != RunningAction.SkillData)
	{
		return; // not this ability's action, or no executor: not counted, the sender resolves it (SendImpactEvent)
	}
	const int32 Index = FMath::RoundToInt(Payload.EventMagnitude);
	// Counted before resolving: the impact can finish the action and end this ability.
	++ImpactEventsResolved;
	UE_LOG(LogWoR, Verbose, TEXT("[Abilities] %s: impact %d through Event.Combat.Impact"), *Avatar->GetName(), Index);
	Executor->ResolveImpactFrame(Index);
}

void UWoRGameplayAbility::EndActionTasks()
{
	// Each pointer is cleared before EndTask: safe from inside a task's own broadcast (the ASC and the delegates
	// broadcast from copies).
	const auto EndTask = [](auto &TaskPtr)
	{
		if (UAbilityTask *Task = TaskPtr)
		{
			TaskPtr = nullptr;
			Task->EndTask();
		}
	};
	EndTask(ImpactTask);
	EndTask(ProjectileArrivedTask);
	EndTask(ProjectileDodgedTask);
	EndTask(BurstDelayTask);
	EndTask(SpellResolveTask);
}

bool UWoRGameplayAbility::ShouldListenForProjectiles(const FAction &Action)
{
	// Support casts (self / ally) spawn no projectile (SpawnSupportSpellEffect); a skill with no Projectile delivery
	// has nothing to wait for.
	const USkillDataBase *Skill = WoRSkillAbilities::GetActionSkill(Action);
	return Skill && Skill->TargetType == ETargetType::Enemy &&
		   WoRSkillAbilities::GetShapeTags(*Skill).HasTagExact(WoRTags::Ability_Shape_Projectile);
}

bool UWoRGameplayAbility::IsListeningForProjectiles() const
{
	return bActionRunning && IsActive() && ProjectileArrivedTask && ProjectileArrivedTask->IsActive();
}

void UWoRGameplayAbility::StartProjectileListeners()
{
	// Q-043d: every arrival of the action (a burst lands several), exact tags, as the impact task (Q-043c).
	ProjectileArrivedTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, WoRTags::Event_Combat_Projectile_Arrived,
																			  /*OptionalExternalTarget*/ nullptr,
																			  /*OnlyTriggerOnce*/ false, /*OnlyMatchExact*/ true);
	ProjectileDodgedTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, WoRTags::Event_Combat_Projectile_Dodged,
																			 /*OptionalExternalTarget*/ nullptr,
																			 /*OnlyTriggerOnce*/ false, /*OnlyMatchExact*/ true);
	if (!ProjectileArrivedTask || !ProjectileDodgedTask)
	{
		// Neither listens, so the executor resolves the arrivals itself.
		ProjectileArrivedTask = nullptr;
		ProjectileDodgedTask = nullptr;
		return;
	}
	ProjectileArrivedTask->EventReceived.AddDynamic(this, &UWoRGameplayAbility::HandleProjectileArrivedEvent);
	ProjectileDodgedTask->EventReceived.AddDynamic(this, &UWoRGameplayAbility::HandleProjectileDodgedEvent);
	ProjectileArrivedTask->ReadyForActivation();
	ProjectileDodgedTask->ReadyForActivation();
}

bool UWoRGameplayAbility::AcceptsProjectileEvent(const FGameplayEventData &Payload) const
{
	if (!bActionRunning || !Payload.Target)
	{
		return false;
	}
	AActor *Avatar = GetAvatarActorFromActorInfo();
	// As the impact event: only the listener the sender chose resolves it, and only for its own action.
	if (WoRSkillAbilities::FindProjectileListener(Avatar) != this)
	{
		return false;
	}
	const UActionExecutor *Executor = Avatar ? WoRFight::Get<UActionExecutor>(Avatar) : nullptr;
	return Executor && Executor->GetCurrentSkillData() == WoRSkillAbilities::GetActionSkill(RunningAction);
}

void UWoRGameplayAbility::HandleProjectileArrivedEvent(FGameplayEventData Payload)
{
	if (!AcceptsProjectileEvent(Payload))
	{
		return; // not counted: the sender resolves it (SendProjectileEvent)
	}
	AActor *Avatar = GetAvatarActorFromActorInfo();
	UActionExecutor *Executor = WoRFight::Get<UActionExecutor>(Avatar);
	// The payload holds the target as const; the executor's defense contexts are keyed by the mutable actor.
	AActor *Target = const_cast<AActor *>(Payload.Target.Get());
	const int32 CastEntryIndex = FMath::RoundToInt(Payload.EventMagnitude);
	// Counted before resolving: the arrival can finish the action and end this ability.
	++ProjectileEventsResolved;
	UE_LOG(LogWoR, Verbose, TEXT("[Abilities] %s: projectile on %s (cast entry %d) through Event.Combat.Projectile.Arrived"),
		   *Avatar->GetName(), *Target->GetName(), CastEntryIndex);
	Executor->ResolveProjectileArrival(Target, CastEntryIndex);
}

void UWoRGameplayAbility::HandleProjectileDodgedEvent(FGameplayEventData Payload)
{
	if (!AcceptsProjectileEvent(Payload))
	{
		return;
	}
	AActor *Avatar = GetAvatarActorFromActorInfo();
	UActionExecutor *Executor = WoRFight::Get<UActionExecutor>(Avatar);
	AActor *Target = const_cast<AActor *>(Payload.Target.Get());
	++ProjectileEventsResolved;
	UE_LOG(LogWoR, Verbose, TEXT("[Abilities] %s: projectile dodged by %s through Event.Combat.Projectile.Dodged"),
		   *Avatar->GetName(), *Target->GetName());
	Executor->ResolveProjectileDodge(Target);
}

void UWoRGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo,
									 const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility,
									 bool bWasCancelled)
{
	// HandleActionComplete clears bActionRunning before it ends the ability, so this runs only for an ability ended
	// while its action goes on.
	if (bActionRunning && BurstDelayTask)
	{
		AActor *Avatar = GetAvatarActorFromActorInfo();
		if (UActionExecutor *Executor = Avatar ? WoRFight::Get<UActionExecutor>(Avatar) : nullptr)
		{
			UE_LOG(LogWoR, Log, TEXT("[Abilities] %s: ended mid-burst; the executor's timer spawns the rest"),
				   *GetNameSafe(GetSkill(Handle, ActorInfo)));
			Executor->ResumeBurstTimer(BurstInterval);
		}
		BurstDelayTask = nullptr; // Super ends the task itself
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UWoRGameplayAbility::StartBurstChain(float Interval)
{
	if (!IsListeningForProjectiles())
	{
		return false;
	}
	// A new burst replaces the old chain, as the executor's SetTimer replaced its timer.
	if (UAbilityTask *Old = BurstDelayTask)
	{
		BurstDelayTask = nullptr;
		Old->EndTask();
	}
	BurstInterval = Interval;
	ArmBurstDelay();
	return BurstDelayTask != nullptr;
}

void UWoRGameplayAbility::ArmBurstDelay()
{
	BurstDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, BurstInterval);
	if (!BurstDelayTask)
	{
		return;
	}
	BurstDelayTask->OnFinish.AddDynamic(this, &UWoRGameplayAbility::HandleBurstDelayFinished);
	BurstDelayTask->ReadyForActivation();
}

void UWoRGameplayAbility::HandleBurstDelayFinished()
{
	BurstDelayTask = nullptr;
	AActor *Avatar = GetAvatarActorFromActorInfo();
	UActionExecutor *Executor = Avatar ? WoRFight::Get<UActionExecutor>(Avatar) : nullptr;
	// The executor empties its queue when the action is cancelled or interrupted: the chain then stops here.
	if (!bActionRunning || !Executor || !Executor->HasBurstQueued())
	{
		return;
	}
	++BurstSpawnsDriven;
	Executor->SpawnNextBurstProjectile();
	if (bActionRunning && !BurstDelayTask && Executor->HasBurstQueued())
	{
		ArmBurstDelay();
	}
}

bool UWoRGameplayAbility::ShouldListenForSpellResolves(const FAction &Action)
{
	const USkillDataBase *Skill = WoRSkillAbilities::GetActionSkill(Action);
	if (!Skill)
	{
		return false;
	}
	// The executor's own test for the support path (SpawnSpellDelivery, DispatchSpellCast).
	const bool bSupport = Skill->TargetType != ETargetType::Enemy;
	const auto IsFlightless = [](ESpellDeliveryType Delivery)
	{
		return Delivery == ESpellDeliveryType::AOE || Delivery == ESpellDeliveryType::Instant;
	};
	if (Skill->CastArray.Num() > 0)
	{
		// DispatchSpellCast's support, AOE and Instant cases (spells and cast-notify abilities alike).
		return bSupport || Skill->CastArray.ContainsByPredicate([&IsFlightless](const FSkillCastEntry &Entry)
		{
			return IsFlightless(Entry.DeliveryType);
		});
	}
	// SpawnSpellDelivery's loose path reads the spell's own TargetType and DeliveryType.
	return Skill->IsA<USpellData>() && (bSupport || IsFlightless(Skill->DeliveryType));
}

bool UWoRGameplayAbility::IsListeningForSpellResolves() const
{
	return bActionRunning && IsActive() && SpellResolveTask && SpellResolveTask->IsActive();
}

void UWoRGameplayAbility::StartSpellResolveListener()
{
	// Q-043e: one task on the parent tag takes all three kinds, every resolution of the action (not OnlyTriggerOnce);
	// GASDocumentation's one-task-many-event-tags pattern, Lyra's WaitGameplayEvent per ability.
	SpellResolveTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, WoRTags::Event_Combat_SpellResolve,
																		 /*OptionalExternalTarget*/ nullptr,
																		 /*OnlyTriggerOnce*/ false, /*OnlyMatchExact*/ false);
	if (!SpellResolveTask)
	{
		return; // the executor resolves them itself
	}
	SpellResolveTask->EventReceived.AddDynamic(this, &UWoRGameplayAbility::HandleSpellResolveEvent);
	SpellResolveTask->ReadyForActivation();
}

void UWoRGameplayAbility::HandleSpellResolveEvent(FGameplayEventData Payload)
{
	if (!bActionRunning)
	{
		return;
	}
	AActor *Avatar = GetAvatarActorFromActorInfo();
	// As the projectile events: only the listener the sender chose, and only for this ability's own action.
	if (WoRSkillAbilities::FindSpellResolveListener(Avatar) != this)
	{
		return;
	}
	UActionExecutor *Executor = Avatar ? WoRFight::Get<UActionExecutor>(Avatar) : nullptr;
	if (!Executor || Executor->GetCurrentSkillData() != WoRSkillAbilities::GetActionSkill(RunningAction))
	{
		return; // not taken: the executor resolves it (RouteSpellResolve)
	}
	EWoRSpellResolveKind Kind = EWoRSpellResolveKind::AOE;
	if (!WoRSkillAbilities::GetSpellResolveKind(Payload.EventTag, Kind))
	{
		return;
	}
	FWoRSpellResolve Resolve;
	// Taking it clears it on the executor: whatever happens next, nothing resolves it twice.
	if (!Executor->TakeRoutedSpellResolve(Kind, Payload.Target.Get(), Resolve))
	{
		return;
	}
	// Counted before resolving: the last hit can finish the action and end this ability.
	switch (Kind)
	{
	case EWoRSpellResolveKind::AOE:
		++AOEResolves;
		break;
	case EWoRSpellResolveKind::Instant:
		++InstantResolves;
		break;
	case EWoRSpellResolveKind::Support:
		++SupportResolves;
		break;
	}
	UE_LOG(LogWoR, Verbose, TEXT("[Abilities] %s: %s on %s through its event (cast entry %d)"), *Avatar->GetName(),
		   *Payload.EventTag.ToString(), *GetNameSafe(Resolve.GetEventTarget()), Resolve.CastEntryIndex);
	Executor->ResolveRoutedSpell(Resolve);
}
