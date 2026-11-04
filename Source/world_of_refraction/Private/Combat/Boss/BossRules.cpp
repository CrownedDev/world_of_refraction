// BossRules.cpp

#include "Combat/Boss/BossRules.h"

#include "Combat/Boss/BossData.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "Combat/Boss/BossTriggers.h"
#include "Conditions/WoRConditions.h"
#include "WoRLog.h"

namespace
{
	FWoRCondition MakeCondition(EWoRConditionSubject Subject, int32 Value = 0, EWoRConditionOperator Operator = EWoRConditionOperator::AtLeast)
	{
		FWoRCondition Out;
		Out.Subject = Subject;
		Out.Value = Value;
		Out.Operator = Operator;
		return Out;
	}

	/** An old phase condition (OBSOLETE) in the one vocabulary. Its float Value becomes a whole number:
	 *  an HP percent rounds; a turn or break count rounds up ("at least 2.5" = at least 3, as before). */
	FWoRCondition FromPhaseCondition(const FBossPhaseCondition &Condition)
	{
		switch (Condition.Kind)
		{
		case EBossPhaseConditionKind::HPBelowPercent:
			return MakeCondition(EWoRConditionSubject::BossHPAtMostPercent, FMath::RoundToInt(Condition.Value));
		case EBossPhaseConditionKind::TurnCount:
			return MakeCondition(EWoRConditionSubject::BossTurnsTaken, FMath::CeilToInt(Condition.Value));
		case EBossPhaseConditionKind::AbsorptionMaxed:
			return MakeCondition(EWoRConditionSubject::BossAbsorptionMaxed);
		case EBossPhaseConditionKind::CrystalBroken:
			return MakeCondition(EWoRConditionSubject::BossCrystalBroken);
		case EBossPhaseConditionKind::StatusBreaks:
			return MakeCondition(EWoRConditionSubject::BossStatusBreaks, FMath::CeilToInt(Condition.Value));
		case EBossPhaseConditionKind::InfusionHits:
		default:
			return MakeCondition(EWoRConditionSubject::BossInfusionHits);
		}
	}

	/** An old trigger condition (OBSOLETE) in the one vocabulary. */
	FWoRCondition FromTriggerCondition(const FBossTriggerCondition &Condition)
	{
		FWoRCondition Out;
		Out.Character = Condition.Character;
		Out.Item = Condition.Item;
		Out.Value = Condition.UpgradeLevel;
		switch (Condition.Kind)
		{
		case EBossTriggerConditionKind::PartyMember:
			Out.Subject = EWoRConditionSubject::CharacterInParty;
			break;
		case EBossTriggerConditionKind::GearWorn:
			Out.Subject = EWoRConditionSubject::GearEquipped;
			break;
		case EBossTriggerConditionKind::Hidden:
		default:
			Out.Subject = EWoRConditionSubject::Hidden;
			break;
		}
		return Out;
	}

	EBossRuleResult FromTriggerResult(EBossTriggerResult Result)
	{
		switch (Result)
		{
		case EBossTriggerResult::Weaken:     return EBossRuleResult::Weaken;
		case EBossTriggerResult::Strengthen: return EBossRuleResult::Strengthen;
		case EBossTriggerResult::SkipPhase:  return EBossRuleResult::SkipPhase;
		case EBossTriggerResult::AddPhase:   return EBossRuleResult::AddPhase;
		case EBossTriggerResult::Dialogue:
		default:                             return EBossRuleResult::Dialogue;
		}
	}

	FString EnumName(const UEnum *Enum, int64 Value)
	{
		return Enum ? Enum->GetNameStringByValue(Value) : FString::FromInt(static_cast<int32>(Value));
	}
}

bool BossRules::MakePhaseRule(const FBossPhase &Phase, int32 PhaseIndex, FBossRule &OutRule)
{
	if (Phase.Conditions.IsEmpty())
	{
		return false; // as before: a phase with no conditions is never reached by itself
	}
	OutRule = FBossRule();
	OutRule.RuleId = Phase.PhaseId.IsNone() ? FName(*FString::Printf(TEXT("Phase%d"), PhaseIndex)) : Phase.PhaseId;
	OutRule.When = EBossRuleWhen::StateChange;
	for (const FBossPhaseCondition &Condition : Phase.Conditions)
	{
		OutRule.Conditions.Conditions.Add(FromPhaseCondition(Condition));
	}
	// Phases are reached in order: this rule holds only while the boss is at this phase.
	OutRule.Conditions.Conditions.Add(MakeCondition(EWoRConditionSubject::BossPhasesReached, PhaseIndex, EWoRConditionOperator::Equal));
	OutRule.Results.Add(EBossRuleResult::NextPhase);
	return true;
}

FBossRule BossRules::MakeTriggerRule(const FBossTrigger &Trigger)
{
	FBossRule Rule;
	Rule.RuleId = Trigger.TriggerId;
	Rule.When = Trigger.When.IsAtFightStart() ? EBossRuleWhen::FightStart : EBossRuleWhen::AtMoment;
	Rule.Moment = Trigger.When;
	for (const FBossTriggerCondition &Condition : Trigger.Conditions)
	{
		Rule.Conditions.Conditions.Add(FromTriggerCondition(Condition));
	}
	for (const EBossTriggerResult Result : Trigger.Results)
	{
		Rule.Results.Add(FromTriggerResult(Result));
	}
	Rule.DialogueId = Trigger.DialogueId;
	return Rule;
}

TArray<FBossRule> BossRules::GetRules(const UBossData &Data)
{
	TArray<FBossRule> Rules = Data.Rules;
	const TArray<FBossPhase> Phases = Data.GetPhasesToUse();
	for (int32 Index = 0; Index < Phases.Num(); ++Index)
	{
		FBossRule PhaseRule;
		if (MakePhaseRule(Phases[Index], Index, PhaseRule))
		{
			Rules.Add(PhaseRule);
		}
	}
	for (const FBossTrigger &Trigger : Data.Triggers)
	{
		Rules.Add(MakeTriggerRule(Trigger));
	}
	return Rules;
}

bool BossRules::Fires(const FBossRule &Rule, const FWoRConditionContext &Context)
{
	return !Rule.Conditions.Conditions.IsEmpty() && WoRConditions::IsMet(Rule.Conditions, Context);
}

bool BossRules::ApplyResults(AActor *Boss, const FBossRule &Rule, FBossFightState &State)
{
	UBossPhaseComponent *Phases = Boss ? Boss->FindComponentByClass<UBossPhaseComponent>() : nullptr;
	bool bSteps = false;
	for (const EBossRuleResult Result : Rule.Results)
	{
		switch (Result)
		{
		case EBossRuleResult::NextPhase:
			if (Phases)
			{
				Phases->EnterNextPhase();
			}
			break;
		case EBossRuleResult::Weaken:
			--State.NetSteps;
			bSteps = true;
			break;
		case EBossRuleResult::Strengthen:
			++State.NetSteps;
			bSteps = true;
			break;
		case EBossRuleResult::SkipPhase:
		case EBossRuleResult::AddPhase:
			// Q-211: a skip and an added phase each happen at most once per boss per fight.
			if (!Phases)
			{
				UE_LOG(LogWoR, Warning, TEXT("[BossRules] %s: %s on %s, which has no boss phases"), *Rule.RuleId.ToString(),
					   *EnumName(StaticEnum<EBossRuleResult>(), static_cast<int64>(Result)), *GetNameSafe(Boss));
			}
			else if (Result == EBossRuleResult::SkipPhase && !State.bSkipped)
			{
				Phases->SkipNextPhase();
				State.bSkipped = true;
			}
			else if (Result == EBossRuleResult::AddPhase && !State.bAdded)
			{
				Phases->AddPhase(UBossData::GetMemoryAddedPhase());
				State.bAdded = true;
			}
			break;
		case EBossRuleResult::Dialogue:
			if (Rule.DialogueId.IsNone())
			{
				UE_LOG(LogWoR, Warning, TEXT("[BossRules] %s: a Dialogue result with no DialogueId"), *Rule.RuleId.ToString());
			}
			else
			{
				// TODO: played by the dialogue system (B-194).
				UE_LOG(LogWoR, Log, TEXT("[BossRules] %s: dialogue %s"), *Rule.RuleId.ToString(), *Rule.DialogueId.ToString());
			}
			break;
		}
	}
	UE_LOG(LogWoR, Log, TEXT("[BossRules] %s fired on %s"), *Rule.RuleId.ToString(), *GetNameSafe(Boss));
	return bSteps;
}

FString BossRules::GetRuleString(const FBossRule &Rule)
{
	const FString When = Rule.When == EBossRuleWhen::AtMoment ? Rule.Moment.ToString()
															   : EnumName(StaticEnum<EBossRuleWhen>(), static_cast<int64>(Rule.When));
	const FString Conditions = FString::JoinBy(Rule.Conditions.Conditions, TEXT(" + "),
											   [](const FWoRCondition &Condition) { return WoRConditions::GetConditionString(Condition); });
	const FString Results = FString::JoinBy(Rule.Results, TEXT(", "),
											[](EBossRuleResult Result) { return EnumName(StaticEnum<EBossRuleResult>(), static_cast<int64>(Result)); });
	return FString::Printf(TEXT("%s: when %s, if %s, then %s"), *Rule.RuleId.ToString(), *When,
						   Conditions.IsEmpty() ? TEXT("(never)") : *Conditions, *Results);
}
