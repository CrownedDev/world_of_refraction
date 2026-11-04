// SkillConditions.cpp

#include "Skills/Effects/SkillConditions.h"

#include "Skills/Effects/SkillTriggerUtils.h"

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
