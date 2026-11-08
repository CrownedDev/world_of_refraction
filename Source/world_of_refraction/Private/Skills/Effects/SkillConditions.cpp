// SkillConditions.cpp

#include "Skills/Effects/SkillConditions.h"

#include "Skills/Effects/SkillTriggerUtils.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Skills/Effects/EffectDefinition.h"
#include "Reaction/ReactionTrigger.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#endif

namespace
{
	/** Every skill event the old actor-state check read as met whenever it was asked. */
	const ESkillTrigger SKILL_CONDITION_STATE_EVENTS[] = {
		ESkillTrigger::OnCrit, ESkillTrigger::OnHit, ESkillTrigger::OnTakeDamage, ESkillTrigger::OnKill,
		ESkillTrigger::OnDodge, ESkillTrigger::OnBlock, ESkillTrigger::OnGuard, ESkillTrigger::OnTurnStart,
		ESkillTrigger::OnTurnEnd, ESkillTrigger::OnSpellCast, ESkillTrigger::OnAbilityUsed, ESkillTrigger::OnBattleStart,
		ESkillTrigger::OnStatusApplied, ESkillTrigger::OnStatusReceived,
	};

	/** The events an action's result can report (UActionExecutor). */
	bool IsSkillActionResultEvent(ESkillTrigger Trigger)
	{
		return Trigger == ESkillTrigger::OnHit || Trigger == ESkillTrigger::OnCrit || Trigger == ESkillTrigger::OnKill;
	}

	FWoRCondition MakeSkillPercentCondition(EWoRConditionSubject Subject, EWoRConditionOperator Operator, float Threshold)
	{
		FWoRCondition Out;
		Out.Subject = Subject;
		Out.Operator = Operator;
		// Q-337d: a whole percent (every authored default is whole; a fraction rounds to the nearest).
		Out.Value = FMath::RoundToInt(Threshold);
		return Out;
	}
}

EWoRConditionTrigger SkillConditions::TriggerFor(ESkillTrigger Trigger)
{
	switch (Trigger)
	{
	case ESkillTrigger::OnCrit:           return EWoRConditionTrigger::Crit;
	case ESkillTrigger::OnHit:            return EWoRConditionTrigger::Hit;
	case ESkillTrigger::OnTakeDamage:     return EWoRConditionTrigger::TakeDamage;
	case ESkillTrigger::OnBattleStart:    return EWoRConditionTrigger::FightStart;
	case ESkillTrigger::OnTurnStart:      return EWoRConditionTrigger::TurnStart;
	case ESkillTrigger::OnTurnEnd:        return EWoRConditionTrigger::TurnEnd;
	case ESkillTrigger::OnKill:           return EWoRConditionTrigger::Kill;
	case ESkillTrigger::OnStatusApplied:  return EWoRConditionTrigger::StatusApplied;
	case ESkillTrigger::OnStatusReceived: return EWoRConditionTrigger::StatusReceived;
	case ESkillTrigger::OnSpellCast:      return EWoRConditionTrigger::SpellCast;
	case ESkillTrigger::OnAbilityUsed:    return EWoRConditionTrigger::AbilityUsed;
	case ESkillTrigger::OnDodge:          return EWoRConditionTrigger::Dodge;
	case ESkillTrigger::OnBlock:          return EWoRConditionTrigger::Block;
	case ESkillTrigger::OnParry:          return EWoRConditionTrigger::Parry;
	case ESkillTrigger::OnPerfectParry:   return EWoRConditionTrigger::PerfectParry;
	case ESkillTrigger::OnPerfectBlock:   return EWoRConditionTrigger::PerfectBlock;
	case ESkillTrigger::OnPerfectDodge:   return EWoRConditionTrigger::PerfectDodge;
	case ESkillTrigger::OnGuard:          return EWoRConditionTrigger::Guard;
	case ESkillTrigger::OnPerfectGuard:   return EWoRConditionTrigger::PerfectGuard;
	case ESkillTrigger::OnFlawlessParry:  return EWoRConditionTrigger::FlawlessParry;
	default:                              return EWoRConditionTrigger::Any;
	}
}

EWoRConditionWho SkillConditions::WhoFor(ECondSubject Subject)
{
	switch (Subject)
	{
	case ECondSubject::SelfTeam:   return EWoRConditionWho::SelfTeam;
	case ECondSubject::Target:     return EWoRConditionWho::Target;
	case ECondSubject::TargetTeam: return EWoRConditionWho::TargetTeam;
	case ECondSubject::Self:
	default:                       return EWoRConditionWho::Self;
	}
}

FWoRCondition SkillConditions::ToCondition(const FSkillCondition &Condition, ESkillCheck Check)
{
	FWoRCondition Out;
	switch (Condition.Trigger)
	{
	case ESkillTrigger::None:
		Out.Subject = EWoRConditionSubject::Disabled;
		break;
	case ESkillTrigger::Always:
		break;
	case ESkillTrigger::OnHPBelowThreshold:
		Out = MakeSkillPercentCondition(EWoRConditionSubject::HpPercent, EWoRConditionOperator::Below, Condition.Threshold);
		break;
	case ESkillTrigger::OnHPAboveThreshold:
		Out = MakeSkillPercentCondition(EWoRConditionSubject::HpPercent, EWoRConditionOperator::Above, Condition.Threshold);
		break;
	case ESkillTrigger::OnEnergyBelowThreshold:
		Out = MakeSkillPercentCondition(EWoRConditionSubject::EpPercent, EWoRConditionOperator::Below, Condition.Threshold);
		break;
	case ESkillTrigger::OnEnergyAboveThreshold:
		Out = MakeSkillPercentCondition(EWoRConditionSubject::EpPercent, EWoRConditionOperator::Above, Condition.Threshold);
		break;
	default:
		Out.Trigger = TriggerFor(Condition.Trigger);
		// The impact path asks whose defense it was; elsewhere an event holds whenever it counts.
		if (Check == ESkillCheck::Defense && SkillTriggerUtils::IsDefenseOutcomeTrigger(Condition.Trigger))
		{
			Out.Subject = EWoRConditionSubject::Defender;
		}
		break;
	}
	Out.Who = WhoFor(Condition.Subject);
	return Out;
}

bool SkillConditions::Participates(const FSkillCondition &Condition, ESkillCheck Check, bool bHasTarget)
{
	if (IsOwnerSide(Condition.Subject))
	{
		return true;
	}
	return Check != ESkillCheck::ActionResult && bHasTarget;
}

TArray<FWoRConditionSet> SkillConditions::ToSets(const TArray<FSkillCondition> &Conditions, ESkillCheck Check, bool bHasTarget)
{
	FWoRConditionSet Ands;
	TArray<FWoRCondition> Ors;
	for (const FSkillCondition &Condition : Conditions)
	{
		if (!Participates(Condition, Check, bHasTarget))
		{
			continue;
		}
		(Condition.Combine == ECondCombine::And ? Ands.Conditions : Ors).Add(ToCondition(Condition, Check));
	}
	TArray<FWoRConditionSet> Sets;
	if (Ors.IsEmpty())
	{
		if (!Ands.Conditions.IsEmpty())
		{
			Sets.Add(MoveTemp(Ands));
		}
		return Sets; // no sets: nothing to meet
	}
	for (const FWoRCondition &Or : Ors)
	{
		FWoRConditionSet &Set = Sets.Add_GetRef(Ands);
		Set.Conditions.Add(Or);
	}
	return Sets;
}

TArray<FSkillCondition> SkillConditions::FromLegacy(ESkillTrigger Primary, float PrimaryThreshold, ESkillTrigger Secondary,
													float SecondaryThreshold, bool bRequireBoth)
{
	TArray<FSkillCondition> Out;
	const ECondCombine Combine = (Secondary == ESkillTrigger::None || bRequireBoth) ? ECondCombine::And : ECondCombine::Or;
	FSkillCondition &First = Out.AddDefaulted_GetRef();
	First.Trigger = Primary;
	First.Threshold = PrimaryThreshold;
	First.Combine = Combine;
	if (Secondary != ESkillTrigger::None)
	{
		FSkillCondition &Second = Out.AddDefaulted_GetRef();
		Second.Trigger = Secondary;
		Second.Threshold = SecondaryThreshold;
		Second.Combine = Combine;
	}
	return Out;
}

TArray<EWoRConditionTrigger> SkillConditions::GetFiring(ESkillCheck Check, const TArray<ESkillTrigger> &Fired)
{
	TArray<EWoRConditionTrigger> Out;
	if (Check != ESkillCheck::ActionResult)
	{
		for (const ESkillTrigger Event : SKILL_CONDITION_STATE_EVENTS)
		{
			// The impact path counts a defense outcome only when it is the one that happened.
			if (Check == ESkillCheck::State || !SkillTriggerUtils::IsDefenseOutcomeTrigger(Event))
			{
				Out.AddUnique(TriggerFor(Event));
			}
		}
	}
	for (const ESkillTrigger Event : Fired)
	{
		const bool bCounts = Check == ESkillCheck::Defense ? SkillTriggerUtils::IsDefenseOutcomeTrigger(Event)
							 : Check == ESkillCheck::ActionResult ? IsSkillActionResultEvent(Event)
																  : false;
		if (bCounts)
		{
			Out.AddUnique(TriggerFor(Event));
		}
	}
	return Out;
}

void SkillConditions::PrepareContext(FWoRConditionContext &Context, ESkillCheck Check, const TArray<ESkillTrigger> &Fired)
{
	Context.Trigger = EWoRConditionTrigger::Any;
	Context.AlsoFiring = GetFiring(Check, Fired);
}

bool SkillConditions::IsGroupMet(const TArray<FSkillCondition> &Conditions, ESkillCheck Check, const FWoRConditionContext &Context,
								  bool bHasTarget)
{
	return WoRConditions::IsAnyMet(ToSets(Conditions, Check, bHasTarget), Context);
}

FString SkillConditions::GetGroupString(const TArray<FSkillCondition> &Conditions, ESkillCheck Check, bool bHasTarget)
{
	const TArray<FWoRConditionSet> Sets = ToSets(Conditions, Check, bHasTarget);
	if (Sets.IsEmpty())
	{
		return TEXT("(always)");
	}
	TArray<FString> SetStrings;
	for (const FWoRConditionSet &Set : Sets)
	{
		TArray<FString> Parts;
		for (const FWoRCondition &Condition : Set.Conditions)
		{
			Parts.Add(WoRConditions::GetConditionString(Condition));
		}
		SetStrings.Add(FString::Join(Parts, TEXT(" + ")));
	}
	return FString::Join(SetStrings, TEXT(" | "));
}

// ==================== B-337e: the authored group ====================

namespace
{
	bool IsOwnerSideWho(EWoRConditionWho Who)
	{
		return Who == EWoRConditionWho::Self || Who == EWoRConditionWho::SelfTeam;
	}

	/** One authored condition as Check reads it: the impact path asks whose defense a defense event was. */
	FWoRCondition ForCheck(const FWoRCondition &Authored, ESkillCheck Check)
	{
		FWoRCondition Out = Authored;
		if (Check == ESkillCheck::Defense && Out.Subject == EWoRConditionSubject::None && SkillConditions::IsDefenseOutcome(Out.Trigger))
		{
			Out.Subject = EWoRConditionSubject::Defender;
		}
		return Out;
	}

	FString SetsToString(const TArray<FWoRConditionSet> &Sets)
	{
		if (Sets.IsEmpty())
		{
			return TEXT("(always)");
		}
		TArray<FString> SetStrings;
		for (const FWoRConditionSet &Set : Sets)
		{
			TArray<FString> Parts;
			for (const FWoRCondition &Condition : Set.Conditions)
			{
				Parts.Add(WoRConditions::GetConditionString(Condition));
			}
			SetStrings.Add(FString::Join(Parts, TEXT(" + ")));
		}
		return FString::Join(SetStrings, TEXT(" | "));
	}
}

void FSkillEffect::MigrateConditionGroup(bool bRebuild)
{
	if (bRebuild)
	{
		ConditionGroup = SkillConditions::ToAuthored(Conditions); // an edit of the old rows: the group follows
		return;
	}
	if (!ConditionGroup.IsEmpty() || Conditions.IsEmpty())
	{
		return;
	}
	ConditionGroup = SkillConditions::ToAuthored(Conditions);
	bConditionGroupMigratedOnLoad = true;
}

FSkillConditionGroup SkillConditions::ToAuthored(const TArray<FSkillCondition> &Conditions)
{
	FSkillConditionGroup Group;
	for (const FSkillCondition &Condition : Conditions)
	{
		(Condition.Combine == ECondCombine::And ? Group.AllOf : Group.AnyOf).Conditions.Add(ToCondition(Condition, ESkillCheck::State));
	}
	return Group;
}

FSkillConditionGroup SkillConditions::GroupOf(const FSkillEffect &Effect)
{
	return Effect.ConditionGroup.IsEmpty() ? ToAuthored(Effect.Conditions) : Effect.ConditionGroup;
}

bool SkillConditions::IsDefenseOutcome(EWoRConditionTrigger Trigger)
{
	switch (Trigger)
	{
	case EWoRConditionTrigger::Parry:
	case EWoRConditionTrigger::PerfectParry:
	case EWoRConditionTrigger::Block:
	case EWoRConditionTrigger::PerfectBlock:
	case EWoRConditionTrigger::Dodge:
	case EWoRConditionTrigger::PerfectDodge:
	case EWoRConditionTrigger::Guard:
	case EWoRConditionTrigger::PerfectGuard:
	case EWoRConditionTrigger::TakeDamage:
	case EWoRConditionTrigger::FlawlessParry:
		return true;
	default:
		return false;
	}
}

bool SkillConditions::Participates(const FWoRCondition &Condition, ESkillCheck Check, bool bHasTarget)
{
	return IsOwnerSideWho(Condition.Who) || (Check != ESkillCheck::ActionResult && bHasTarget);
}

TArray<FWoRConditionSet> SkillConditions::ToSets(const FSkillConditionGroup &Group, ESkillCheck Check, bool bHasTarget)
{
	FWoRConditionSet Ands;
	TArray<FWoRCondition> Ors;
	for (const FWoRCondition &Condition : Group.AllOf.Conditions)
	{
		if (Participates(Condition, Check, bHasTarget))
		{
			Ands.Conditions.Add(ForCheck(Condition, Check));
		}
	}
	for (const FWoRCondition &Condition : Group.AnyOf.Conditions)
	{
		if (Participates(Condition, Check, bHasTarget))
		{
			Ors.Add(ForCheck(Condition, Check));
		}
	}
	TArray<FWoRConditionSet> Sets;
	if (Ors.IsEmpty())
	{
		if (!Ands.Conditions.IsEmpty())
		{
			Sets.Add(MoveTemp(Ands));
		}
		return Sets; // no sets: nothing to meet
	}
	for (const FWoRCondition &Or : Ors)
	{
		FWoRConditionSet &Set = Sets.Add_GetRef(Ands);
		Set.Conditions.Add(Or);
	}
	return Sets;
}

bool SkillConditions::IsGroupMet(const FSkillConditionGroup &Group, ESkillCheck Check, const FWoRConditionContext &Context, bool bHasTarget)
{
	return WoRConditions::IsAnyMet(ToSets(Group, Check, bHasTarget), Context);
}

FString SkillConditions::GetGroupString(const FSkillConditionGroup &Group, ESkillCheck Check, bool bHasTarget)
{
	return SetsToString(ToSets(Group, Check, bHasTarget));
}

FString SkillConditions::GetUnmigratedEffectsString()
{
#if !WITH_EDITOR
	return TEXT("(editor only)");
#else
	IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.WaitForCompletion(); // a half-scanned registry would read as "all resaved"
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UEffectDefinition::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);

	FString Out = FString::Printf(TEXT("%d effect assets scanned\n"), Assets.Num());
	for (const FAssetData &Asset : Assets)
	{
		const UEffectDefinition *Definition = Cast<UEffectDefinition>(Asset.GetAsset());
		if (!Definition)
		{
			continue;
		}
		const int32 OnOld = Definition->Effects.FilterByPredicate([](const FSkillEffect &Effect) { return Effect.bConditionGroupMigratedOnLoad; }).Num();
		if (OnOld > 0)
		{
			Out += FString::Printf(TEXT("%s: %d effect(s) on the old conditions (resave)\n"), *Asset.GetObjectPathString(), OnOld);
		}
	}

	// Review fix: reaction assets carry effects too (FReactionTrigger::Effects).
	TArray<FAssetData> ReactionAssets;
	Registry.GetAssetsByClass(UReactionTriggerData::StaticClass()->GetClassPathName(), ReactionAssets, /*bSearchSubClasses=*/true);
	Out += FString::Printf(TEXT("%d reaction assets scanned\n"), ReactionAssets.Num());
	for (const FAssetData &Asset : ReactionAssets)
	{
		const UReactionTriggerData *Reactions = Cast<UReactionTriggerData>(Asset.GetAsset());
		if (!Reactions)
		{
			continue;
		}
		int32 OnOld = 0;
		for (const FReactionTrigger &Trigger : Reactions->Triggers)
		{
			OnOld += Trigger.Effects.FilterByPredicate([](const FSkillEffect &Effect) { return Effect.bConditionGroupMigratedOnLoad; }).Num();
		}
		if (OnOld > 0)
		{
			Out += FString::Printf(TEXT("%s: %d effect(s) on the old conditions (resave)\n"), *Asset.GetObjectPathString(), OnOld);
		}
	}
	return Out;
#endif
}
