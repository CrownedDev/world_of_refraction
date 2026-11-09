// WoRSkillAbilities.cpp

#include "GAS/Abilities/WoRSkillAbilities.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/Actions/EAbilityExecutionType.h"
#include "GAS/Abilities/WoRGameplayAbility.h"
#include "GAS/WoRCombatEvents.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/ESpellDeliveryType.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Tags/WoRGameplayTags.h"
#include "WoRLog.h"

namespace
{
	/** A skill ability has no levels: the skill's own tier and charge set its numbers. */
	constexpr int32 SKILL_ABILITY_LEVEL = 1;

	int32 GWoRAbilitiesUseGAS = 0;
	FAutoConsoleVariableRef CVarWoRAbilitiesUseGAS(
		TEXT("wor.Abilities.UseGAS"), GWoRAbilitiesUseGAS,
		TEXT("1: a spell or ability action starts through its skill's granted GameplayAbility (B-043b), which runs the same executor call. 0: the old call (default)."),
		ECVF_Default);

	FGameplayTag ExecutionShapeTag(EAbilityExecutionType Type)
	{
		switch (Type)
		{
		case EAbilityExecutionType::Melee:
			return WoRTags::Ability_Shape_Melee;
		case EAbilityExecutionType::Ranged:
			return WoRTags::Ability_Shape_Ranged;
		case EAbilityExecutionType::SelfCast:
			return WoRTags::Ability_Shape_SelfCast;
		case EAbilityExecutionType::TargetCast:
			return WoRTags::Ability_Shape_TargetCast;
		}
		return FGameplayTag();
	}

	FGameplayTag DeliveryShapeTag(ESpellDeliveryType Type)
	{
		switch (Type)
		{
		case ESpellDeliveryType::Projectile:
			return WoRTags::Ability_Shape_Projectile;
		case ESpellDeliveryType::AOE:
			return WoRTags::Ability_Shape_AOE;
		case ESpellDeliveryType::Instant:
			return WoRTags::Ability_Shape_Instant;
		}
		return FGameplayTag();
	}

	bool IsSkillSpec(const FGameplayAbilitySpec &Spec)
	{
		return Spec.Ability && Spec.Ability->IsA<UWoRGameplayAbility>();
	}
}

bool WoRSkillAbilities::UsesAbilities()
{
	return GWoRAbilitiesUseGAS != 0;
}

void WoRSkillAbilities::SetUsesAbilities(bool bEnabled)
{
	GWoRAbilitiesUseGAS = bEnabled ? 1 : 0;
}

FGameplayTagContainer WoRSkillAbilities::GetShapeTags(const USkillDataBase &Skill)
{
	FGameplayTagContainer Tags;
	Tags.AddTag(ExecutionShapeTag(Skill.ExecutionType));
	if (Skill.CastArray.Num() > 0)
	{
		for (const FSkillCastEntry &Entry : Skill.CastArray)
		{
			Tags.AddTag(DeliveryShapeTag(Entry.DeliveryType));
		}
	}
	else if (Skill.IsA<USpellData>())
	{
		Tags.AddTag(DeliveryShapeTag(Skill.DeliveryType)); // the empty-CastArray fallback the orchestrator reads
	}
	return Tags;
}

USkillDataBase *WoRSkillAbilities::GetActionSkill(const FAction &Action)
{
	switch (Action.ActionType)
	{
	case EActionType::Spell:
		return Action.SpellData;
	case EActionType::Ability:
		return Action.SkillData;
	default:
		return nullptr;
	}
}

FAction WoRSkillAbilities::MakePlainAction(USkillDataBase *Skill)
{
	FAction Action;
	if (USpellData *Spell = Cast<USpellData>(Skill))
	{
		Action.ActionType = EActionType::Spell;
		Action.SpellData = Spell;
	}
	else if (Skill)
	{
		Action.ActionType = EActionType::Ability;
		Action.SkillData = Skill;
	}
	return Action;
}

UAbilitySystemComponent *WoRSkillAbilities::GetGrantingASC(AActor *Actor)
{
	if (!Actor || !Actor->HasAuthority())
	{
		return nullptr;
	}
	UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	// GiveAbility hands the spec its actor info, so the ASC must have been initialised (UWoRAttributeSyncComponent).
	return ASC && ASC->AbilityActorInfo.IsValid() && ASC->GetAvatarActor() ? ASC : nullptr;
}

FGameplayAbilitySpec *WoRSkillAbilities::FindSkillSpec(AActor *Actor, const USkillDataBase *Skill)
{
	UAbilitySystemComponent *ASC = Skill ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor) : nullptr;
	if (!ASC)
	{
		return nullptr;
	}
	for (FGameplayAbilitySpec &Spec : ASC->GetActivatableAbilities())
	{
		if (IsSkillSpec(Spec) && Spec.SourceObject.Get() == Skill)
		{
			return &Spec;
		}
	}
	return nullptr;
}

FGameplayAbilitySpecHandle WoRSkillAbilities::GrantSkill(AActor *Actor, USkillDataBase *Skill)
{
	UAbilitySystemComponent *ASC = Skill ? GetGrantingASC(Actor) : nullptr;
	if (!ASC)
	{
		return FGameplayAbilitySpecHandle();
	}
	if (const FGameplayAbilitySpec *Existing = FindSkillSpec(Actor, Skill))
	{
		return Existing->Handle;
	}
	FGameplayAbilitySpec Spec(UWoRGameplayAbility::StaticClass(), SKILL_ABILITY_LEVEL, INDEX_NONE, Skill);
	Spec.GetDynamicSpecSourceTags().AppendTags(GetShapeTags(*Skill));
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
	UE_LOG(LogWoR, Verbose, TEXT("[Abilities] %s granted %s"), *Actor->GetName(), *Skill->GetName());
	return Handle;
}

int32 WoRSkillAbilities::GrantSkills(AActor *Actor, const TArray<USkillDataBase *> &Skills)
{
	int32 Granted = 0;
	for (USkillDataBase *Skill : Skills)
	{
		if (Skill && !FindSkillSpec(Actor, Skill) && GrantSkill(Actor, Skill).IsValid())
		{
			++Granted;
		}
	}
	return Granted;
}

TArray<USkillDataBase *> WoRSkillAbilities::GetLoadoutSkills(const AActor *Actor)
{
	TArray<USkillDataBase *> Skills;
	const ULoadoutComponent *Loadout = Actor ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr;
	const UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr;
	// The loadout's getters warn without an inventory or an active saved loadout; such a combatant has no
	// loadout skills (its actions still get theirs granted on first use).
	if (!Loadout || !Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
	{
		return Skills;
	}
	Skills.Append(Loadout->GetAllWeaponAttacks());
	for (UAbilityData *Ability : Loadout->GetAvailableAbilities())
	{
		Skills.Add(Ability);
	}
	for (USpellData *Spell : Loadout->GetAvailableSpells())
	{
		Skills.Add(Spell);
	}
	return Skills;
}

int32 WoRSkillAbilities::GrantLoadoutSkills(AActor *Actor)
{
	if (!GetGrantingASC(Actor))
	{
		return 0;
	}
	const int32 Granted = GrantSkills(Actor, GetLoadoutSkills(Actor));
	UE_LOG(LogWoR, Verbose, TEXT("[Abilities] %s: %d skill ability(ies) granted"), *Actor->GetName(), Granted);
	return Granted;
}

int32 WoRSkillAbilities::ClearSkills(AActor *Actor)
{
	UAbilitySystemComponent *ASC = Actor && Actor->HasAuthority() ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor) : nullptr;
	if (!ASC)
	{
		return 0;
	}
	TArray<FGameplayAbilitySpecHandle> Handles;
	for (const FGameplayAbilitySpec &Spec : ASC->GetActivatableAbilities())
	{
		if (IsSkillSpec(Spec))
		{
			Handles.Add(Spec.Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle &Handle : Handles)
	{
		ASC->CancelAbilityHandle(Handle);
		ASC->ClearAbility(Handle);
	}
	return Handles.Num();
}

TArray<FGameplayAbilitySpec> WoRSkillAbilities::GetSkillSpecs(AActor *Actor)
{
	TArray<FGameplayAbilitySpec> Specs;
	if (UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor))
	{
		for (const FGameplayAbilitySpec &Spec : ASC->GetActivatableAbilities())
		{
			if (IsSkillSpec(Spec))
			{
				Specs.Add(Spec);
			}
		}
	}
	return Specs;
}

bool WoRSkillAbilities::TryExecuteThroughAbility(AActor *Actor, const FAction &Action, const FOnActionComplete &OnComplete)
{
	// B-043g (Q-043g): a ritual's fire (bIsDeferredFire) runs through the same skill spec as its arm; the executor's
	// bIsDeferredFire gates keep it cost-free and skip the ritual leg, and CommitCostThroughAbility commits nothing.
	if (!UsesAbilities())
	{
		return false;
	}
	USkillDataBase *Skill = GetActionSkill(Action);
	UAbilitySystemComponent *ASC = Skill ? GetGrantingASC(Actor) : nullptr;
	if (!ASC)
	{
		return false;
	}
	// A skill the loadout gained mid-fight (weapon switch, a Broken Darkness pool) is granted on first use. If the
	// ASC's ability list is locked (this runs inside another ability's synchronous activation), GiveAbility is
	// deferred: TryActivateAbility then fails and the old call runs the action, which is safe; the deferred grant
	// lands when the lock ends (FindSkillSpec finds it next time, so no duplicate spec is given after that).
	const FGameplayAbilitySpecHandle Handle = GrantSkill(Actor, Skill);
	if (!Handle.IsValid())
	{
		return false;
	}
	FWoRSkillActivationScope Scope(Handle, Action, OnComplete);
	const bool bActivated = ASC->TryActivateAbility(Handle, /*bAllowRemoteActivation*/ false);
	if (Scope.bConsumed)
	{
		return true;
	}
	UE_LOG(LogWoR, Log, TEXT("[Abilities] %s: %s's ability did not run the action (activated: %s); the old call runs it"),
		   *Actor->GetName(), *Skill->GetName(), bActivated ? TEXT("yes") : TEXT("no"));
	return false;
}

namespace
{
	/** The first active skill ability on Actor that IsListening accepts, or null. */
	template <typename PredicateType>
	UWoRGameplayAbility *FindListeningSkillAbility(AActor *Actor, PredicateType IsListening)
	{
		UAbilitySystemComponent *ASC = Actor ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor) : nullptr;
		if (!ASC)
		{
			return nullptr;
		}
		for (const FGameplayAbilitySpec &Spec : ASC->GetActivatableAbilities())
		{
			if (!IsSkillSpec(Spec) || !Spec.IsActive())
			{
				continue;
			}
			for (UGameplayAbility *Instance : Spec.GetAbilityInstances())
			{
				UWoRGameplayAbility *Ability = Cast<UWoRGameplayAbility>(Instance);
				if (Ability && IsListening(*Ability))
				{
					return Ability;
				}
			}
		}
		return nullptr;
	}
}

UWoRGameplayAbility *WoRSkillAbilities::FindImpactListener(AActor *Actor)
{
	return FindListeningSkillAbility(Actor, [](const UWoRGameplayAbility &Ability) { return Ability.IsListeningForImpacts(); });
}

UWoRGameplayAbility *WoRSkillAbilities::FindProjectileListener(AActor *Actor)
{
	return FindListeningSkillAbility(Actor, [](const UWoRGameplayAbility &Ability) { return Ability.IsListeningForProjectiles(); });
}

bool WoRSkillAbilities::SendProjectileEvent(AActor *Actor, const FGameplayTag &Event, AActor *Target, int32 CastEntryIndex)
{
	UWoRGameplayAbility *Listener = FindProjectileListener(Actor);
	UAbilitySystemComponent *ASC = Listener ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor) : nullptr;
	if (!ASC || !Target)
	{
		return false;
	}
	FGameplayEventData Payload;
	Payload.EventTag = Event;
	Payload.Instigator = Actor;
	Payload.Target = Target;
	Payload.EventMagnitude = static_cast<float>(CastEntryIndex);
	const int32 ResolvedBefore = Listener->GetProjectileEventsResolved();
	ASC->HandleGameplayEvent(Event, &Payload);
	if (Listener->GetProjectileEventsResolved() != ResolvedBefore)
	{
		return true;
	}
	// Never drop an arrival: the old path resolves it.
	UE_LOG(LogWoR, Warning, TEXT("[Abilities] %s: %s on %s did not reach %s's projectile task; resolving it directly"),
		   *Actor->GetName(), *Event.ToString(), *Target->GetName(), *GetNameSafe(Listener));
	return false;
}

bool WoRSkillAbilities::StartBurstThroughAbility(AActor *Actor, float Interval)
{
	UWoRGameplayAbility *Listener = FindProjectileListener(Actor);
	return Listener && Listener->StartBurstChain(Interval);
}

bool WoRSkillAbilities::SendImpactEvent(AActor *Actor, int32 Index)
{
	UWoRGameplayAbility *Listener = FindImpactListener(Actor);
	if (!Listener)
	{
		return false;
	}
	const int32 ResolvedBefore = Listener->GetImpactEventsResolved();
	WoRCombatEvents::Send(Actor, WoRTags::Event_Combat_Impact, Actor, static_cast<float>(Index));
	if (Listener->GetImpactEventsResolved() != ResolvedBefore)
	{
		return true;
	}
	// Never drop a hit: the old path resolves it.
	UE_LOG(LogWoR, Warning, TEXT("[Abilities] %s: impact %d did not reach %s's impact task; resolving it directly"),
		   *Actor->GetName(), Index, *GetNameSafe(Listener));
	return false;
}

FGameplayTag WoRSkillAbilities::GetSpellResolveEventTag(EWoRSpellResolveKind Kind)
{
	switch (Kind)
	{
	case EWoRSpellResolveKind::Instant:
		return WoRTags::Event_Combat_SpellResolve_Instant;
	case EWoRSpellResolveKind::Support:
		return WoRTags::Event_Combat_SpellResolve_Support;
	case EWoRSpellResolveKind::AOE:
	default:
		return WoRTags::Event_Combat_SpellResolve_AOE;
	}
}

bool WoRSkillAbilities::GetSpellResolveKind(const FGameplayTag &EventTag, EWoRSpellResolveKind &OutKind)
{
	for (const EWoRSpellResolveKind Kind : {EWoRSpellResolveKind::AOE, EWoRSpellResolveKind::Instant, EWoRSpellResolveKind::Support})
	{
		if (EventTag.MatchesTagExact(GetSpellResolveEventTag(Kind)))
		{
			OutKind = Kind;
			return true;
		}
	}
	return false;
}

UWoRGameplayAbility *WoRSkillAbilities::FindSpellResolveListener(AActor *Actor)
{
	return FindListeningSkillAbility(Actor, [](const UWoRGameplayAbility &Ability) { return Ability.IsListeningForSpellResolves(); });
}

bool WoRSkillAbilities::SendSpellResolveEvent(AActor *Actor, const FWoRSpellResolve &Resolve)
{
	UWoRGameplayAbility *Listener = FindSpellResolveListener(Actor);
	UAbilitySystemComponent *ASC = Listener ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor) : nullptr;
	if (!ASC)
	{
		return false;
	}
	// Built here, as SendProjectileEvent: this event's Target is the defender (Q-043e), not the receiver.
	FGameplayEventData Payload;
	Payload.EventTag = GetSpellResolveEventTag(Resolve.Kind);
	Payload.Instigator = Actor;
	Payload.Target = Resolve.GetEventTarget();
	Payload.EventMagnitude = static_cast<float>(Resolve.CastEntryIndex);
	const int32 TakenBefore = Listener->GetSpellResolvesResolved();
	ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
	if (Listener->GetSpellResolvesResolved() != TakenBefore)
	{
		return true;
	}
	// Never drop a resolution: the executor resolves it (RouteSpellResolve). These resolve inside their own action,
	// so a listener that did not take one is a fault worth seeing.
	UE_LOG(LogWoR, Warning, TEXT("[Abilities] %s: %s on %s did not reach %s's spell-resolve task; resolving it directly"),
		   *Actor->GetName(), *Payload.EventTag.ToString(), *GetNameSafe(Resolve.GetEventTarget()), *GetNameSafe(Listener));
	return false;
}

UWoRGameplayAbility *WoRSkillAbilities::FindActionRunner(AActor *Actor, const FAction &Action)
{
	const USkillDataBase *Skill = GetActionSkill(Action);
	if (!Skill)
	{
		return nullptr;
	}
	return FindListeningSkillAbility(Actor, [Skill](const UWoRGameplayAbility &Ability)
	{
		const FAction *Running = Ability.GetRunningAction();
		return Running && GetActionSkill(*Running) == Skill;
	});
}

bool WoRSkillAbilities::CommitCostThroughAbility(AActor *Actor, const FAction &Action, int32 Cost)
{
	// B-043g (Q-043g): a ritual paid at its arm; its fire commits nothing (the executor's SpendEnergy of 0 is a no-op).
	if (Action.bIsDeferredFire)
	{
		return false;
	}
	UWoRGameplayAbility *Runner = FindActionRunner(Actor, Action);
	if (!Runner)
	{
		return false;
	}
	if (Runner->CommitActionCost(Cost))
	{
		return true;
	}
	// Never pay twice: nothing was paid, so the executor pays (or refuses, as before).
	UE_LOG(LogWoR, Verbose, TEXT("[Abilities] %s: %s did not pay %d EP; the executor pays it"), *Actor->GetName(),
		   *GetNameSafe(Runner), Cost);
	return false;
}
