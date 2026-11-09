// WoRGameplayAbility.cpp

#include "GAS/Abilities/WoRGameplayAbility.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/WoRFightSubsystem.h"
#include "GAS/Abilities/WoRSkillAbilities.h"
#include "Skills/Definitions/SkillDataBase.h"
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
	// Intentionally empty (Q-043b): ExecuteActionAsync pays the EP through SpendEnergy (B-043a's cost GE when
	// wor.Costs.UseGAS is on) and the infusion backlash after the action, so paying here would pay twice.
	// No CostGameplayEffectClass is set, so Super would pay nothing either.
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

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FailActivation(TEXT("Not enough energy"));
		return;
	}
	AActor *Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UActionExecutor *Executor = Avatar ? WoRFight::Get<UActionExecutor>(Avatar) : nullptr;
	if (!Executor)
	{
		FailActivation(TEXT("No action executor"));
		return;
	}

	UE_LOG(LogWoR, Verbose, TEXT("[Abilities] %s: %s runs through its ability"), *Avatar->GetName(), *RunningAction.GetActionName());
	// Before the executor call: a montage-less action (wor.Combat.InstantImpacts) lands its impacts inside it.
	if (ShouldListenForImpacts(RunningAction))
	{
		StartImpactListener();
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
	if (UAbilityTask_WaitGameplayEvent *Task = ImpactTask)
	{
		// Safe from inside the task's own broadcast: the ASC and the delegate both broadcast from copies.
		ImpactTask = nullptr;
		Task->EndTask();
	}
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
