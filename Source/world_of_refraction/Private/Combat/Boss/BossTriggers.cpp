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

int32 BossTriggers::ApplyTo(AActor *Boss, const UBossData &Data, const TArray<AActor *> &Party)
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
	UBossPhaseComponent *Phases = Boss->FindComponentByClass<UBossPhaseComponent>();
	int32 Fired = 0;
	int32 NetSteps = 0;
	// Q-211: a skip and an added phase each happen at most once per boss per fight, however many
	// triggers ask (two identical last stands, or skipping every phase, would break the fight).
	bool bSkipped = false;
	bool bAdded = false;
	for (const FBossTrigger &Trigger : Data.Triggers)
	{
		if (!Fires(Trigger, FightStart))
		{
			continue;
		}
		++Fired;
		for (const EBossTriggerResult Result : Trigger.Results)
		{
			switch (Result)
			{
			case EBossTriggerResult::Weaken:
				--NetSteps;
				break;
			case EBossTriggerResult::Strengthen:
				++NetSteps;
				break;
			case EBossTriggerResult::SkipPhase:
			case EBossTriggerResult::AddPhase:
				if (!Phases)
				{
					UE_LOG(LogWoR, Warning, TEXT("[BossTriggers] %s: %s on %s, which has no boss phases"), *Trigger.TriggerId.ToString(),
						   *UEnum::GetValueAsString(Result), *GetNameSafe(Boss));
				}
				else if (Result == EBossTriggerResult::SkipPhase && !bSkipped)
				{
					Phases->SkipNextPhase();
					bSkipped = true;
				}
				else if (Result == EBossTriggerResult::AddPhase && !bAdded)
				{
					Phases->AddPhase(UBossData::GetMemoryAddedPhase());
					bAdded = true;
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
		UE_LOG(LogWoR, Log, TEXT("[BossTriggers] %s fired on %s"), *Trigger.TriggerId.ToString(), *GetNameSafe(Boss));
	}
	ReactionTriggers::ApplyBossSteps(Boss, NetSteps);
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
