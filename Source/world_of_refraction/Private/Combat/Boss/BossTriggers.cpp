// BossTriggers.cpp

#include "Combat/Boss/BossTriggers.h"

#include "Algo/AllOf.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Boss/BossData.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "Combat/Boss/BossRules.h"
#include "Conditions/WoRConditions.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Reaction/ReactionTrigger.h"
#include "WoRLog.h"

namespace
{
	/** The memory condition check covers the fight-true kinds (D-100: the check, not memories). */
	FMemoryCondition AsMemoryCondition(const FBossTriggerCondition &Condition)
	{
		FMemoryCondition Out;
		Out.Character = Condition.Character;
		Out.Item = Condition.Item;
		Out.UpgradeLevel = Condition.UpgradeLevel;
		switch (Condition.Kind)
		{
		case EBossTriggerConditionKind::PartyMember:
			Out.Kind = EMemoryConditionKind::Character;
			break;
		case EBossTriggerConditionKind::GearWorn:
			Out.Kind = EMemoryConditionKind::GearEquipped;
			break;
		case EBossTriggerConditionKind::Hidden:
			Out.Kind = EMemoryConditionKind::Hidden;
			break;
		}
		return Out;
	}
}

bool BossTriggers::IsMet(const FBossTriggerCondition &Condition, const FMemoryEvent &Event)
{
	// OBSOLETE (B-355): fight-start rules evaluate through WoRConditions (ApplyTo); kept for UFightTriggers' moments.
	return MemoryConditions::IsMet(AsMemoryCondition(Condition), Event, nullptr);
}

bool BossTriggers::Fires(const FBossTrigger &Trigger, const FMemoryEvent &Event)
{
	return !Trigger.Conditions.IsEmpty() &&
		   Algo::AllOf(Trigger.Conditions, [&Event](const FBossTriggerCondition &Condition) { return IsMet(Condition, Event); });
}

bool BossTriggers::ApplyResults(AActor *Boss, const FBossTrigger &Trigger, FBossFightState &State)
{
	UBossPhaseComponent *Phases = Boss ? Boss->FindComponentByClass<UBossPhaseComponent>() : nullptr;
	bool bSteps = false;
	for (const EBossTriggerResult Result : Trigger.Results)
	{
		switch (Result)
		{
		case EBossTriggerResult::Weaken:
			--State.NetSteps;
			bSteps = true;
			break;
		case EBossTriggerResult::Strengthen:
			++State.NetSteps;
			bSteps = true;
			break;
		case EBossTriggerResult::SkipPhase:
		case EBossTriggerResult::AddPhase:
			// Q-211: a skip and an added phase each happen at most once per boss per fight, however
			// many triggers ask (two identical last stands, or skipping every phase, would break it).
			if (!Phases)
			{
				UE_LOG(LogWoR, Warning, TEXT("[BossTriggers] %s: %s on %s, which has no boss phases"), *Trigger.TriggerId.ToString(),
					   *UEnum::GetValueAsString(Result), *GetNameSafe(Boss));
			}
			else if (Result == EBossTriggerResult::SkipPhase && !State.bSkipped)
			{
				Phases->SkipNextPhase();
				State.bSkipped = true;
			}
			else if (Result == EBossTriggerResult::AddPhase && !State.bAdded)
			{
				Phases->AddPhase(UBossData::GetMemoryAddedPhase());
				State.bAdded = true;
			}
			break;
		case EBossTriggerResult::Dialogue:
			if (Trigger.DialogueId.IsNone())
			{
				UE_LOG(LogWoR, Warning, TEXT("[BossTriggers] %s: a Dialogue result with no DialogueId"), *Trigger.TriggerId.ToString());
			}
			else
			{
				// TODO: played by the dialogue system (B-194).
				UE_LOG(LogWoR, Log, TEXT("[BossTriggers] %s: dialogue %s"), *Trigger.TriggerId.ToString(), *Trigger.DialogueId.ToString());
			}
			break;
		}
	}
	UE_LOG(LogWoR, Log, TEXT("[BossTriggers] %s fired on %s (%s)"), *Trigger.TriggerId.ToString(), *GetNameSafe(Boss), *Trigger.When.ToString());
	return bSteps;
}

int32 BossTriggers::ApplyTo(AActor *Boss, const UBossData &Data, const TArray<AActor *> &Party, FBossFightState *OutState)
{
	if (!Boss)
	{
		return 0;
	}
	FMemoryEvent FightStart;
	for (const AActor *Member : Party)
	{
		FightStart.Party.Add(MemoryConditions::MakeMember(Member));
	}
	int32 Fired = 0;
	// B-355 (D-144): the fight-start rules of the boss's one list (authored, then old triggers read
	// across). State-change rules fire in UBossPhaseComponent; moments still wait in UFightTriggers.
	// One per-fight state per boss (Q-211): the phase component's when it has one, so the fight-start
	// results and the state-change results net out and a skip or an added phase happens once.
	UBossPhaseComponent *Phases = Boss->FindComponentByClass<UBossPhaseComponent>();
	FBossFightState StateWithoutPhases;
	FBossFightState &State = Phases ? Phases->GetFightState() : StateWithoutPhases;
	const FWoRBossState BossState = Phases ? Phases->MakeBossState() : FWoRBossState();
	FWoRConditionContext Context = WoRConditions::MakeContext(FightStart, nullptr);
	Context.Boss = Phases ? &BossState : nullptr;
	// B-337c (Q-337b): the boss is the rules' Self (the shared Hp subject), phases or not.
	const UCharacterDataComponent *BossData = Boss->FindComponentByClass<UCharacterDataComponent>();
	const FWoRFighterState BossSelf{BossData ? BossData->CurrentHP : 0, BossData ? BossData->MaxHP : 0};
	Context.Self = &BossSelf;
	for (const FBossRule &Rule : BossRules::GetRules(Data))
	{
		if (Rule.When == EBossRuleWhen::FightStart && BossRules::Fires(Rule, Context))
		{
			BossRules::ApplyResults(Boss, Rule, State);
			++Fired;
		}
	}
	ReactionTriggers::ApplyBossSteps(Boss, State.NetSteps);
	if (OutState)
	{
		*OutState = State;
	}
	return Fired;
}

int32 BossTriggers::Apply(const TArray<AActor *> &Party, const TArray<AActor *> &Opposing)
{
	int32 Fired = 0;
	for (AActor *Enemy : Opposing)
	{
		const UCharacterDataComponent *EnemyData = Enemy ? Enemy->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (const UBossData *BossData = EnemyData ? UBossData::FindFor(EnemyData->CharacterData) : nullptr)
		{
			Fired += ApplyTo(Enemy, *BossData, Party);
		}
	}
	return Fired;
}
