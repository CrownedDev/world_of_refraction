// BossTriggers.cpp

#include "Combat/Boss/BossTriggers.h"

#include "Algo/AllOf.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Boss/BossData.h"
#include "Combat/Boss/BossPhaseComponent.h"
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
	FBossFightState State;
	int32 Fired = 0;
	for (const FBossTrigger &Trigger : Data.Triggers)
	{
		// B-212: the others wait for their moment in UFightTriggers.
		if (Trigger.When.IsAtFightStart() && Fires(Trigger, FightStart))
		{
			ApplyResults(Boss, Trigger, State);
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
