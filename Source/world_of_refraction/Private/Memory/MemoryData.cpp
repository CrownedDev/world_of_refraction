// MemoryData.cpp

#include "Memory/MemoryData.h"

#include "Character/CharacterData.h"
#include "Equipment/EquipmentDataBase.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "WoRLog.h"

FName UMemoryData::GetMemoryId() const
{
	return MemoryId.IsNone() ? GetFName() : MemoryId;
}

EMemoryDepth UMemoryData::GetDepthFor(const UCharacterData *Character) const
{
	for (const TPair<TObjectPtr<UCharacterData>, EMemoryDepth> &Entry : CharacterDepth)
	{
		if (Entry.Key == Character)
		{
			return Entry.Value;
		}
	}
	return EMemoryDepth::None;
}

FString UMemoryData::GetMemoryString() const
{
	TArray<FString> Depths;
	for (const TPair<TObjectPtr<UCharacterData>, EMemoryDepth> &Entry : CharacterDepth)
	{
		Depths.Add(FString::Printf(TEXT("%s for %s"), *UEnum::GetDisplayValueAsText(Entry.Value).ToString(),
								   Entry.Key ? *Entry.Key->Name : TEXT("<none>")));
	}
	TArray<FString> Modifiers;
	for (const FMemoryBossModifier &Modifier : BossModifiers)
	{
		Modifiers.Add(FString::Printf(TEXT("%s %s"), *UEnum::GetDisplayValueAsText(Modifier.Kind).ToString(),
									  Modifier.Boss ? *Modifier.Boss->Name : TEXT("<no boss>")));
	}
	TArray<FString> Routes;
	for (const TArray<FMemoryCondition> &Set : GetConditionSets())
	{
		Routes.Add(FString::JoinBy(Set, TEXT(" + "), [](const FMemoryCondition &Condition) { return GetConditionString(Condition); }));
	}
	const FString RewardList = FString::JoinBy(Rewards, TEXT(", "), [](const FMemoryReward &Reward) { return GetRewardString(Reward); });
	return FString::Printf(TEXT("Memory %s '%s' | depth: %s | boss (obsolete, B-211): %s | unlock: %s | rewards: %s"), *GetMemoryId().ToString(),
						   *Title.ToString(), Depths.IsEmpty() ? TEXT("none") : *FString::Join(Depths, TEXT(", ")),
						   Modifiers.IsEmpty() ? TEXT("none") : *FString::Join(Modifiers, TEXT(", ")),
						   Routes.IsEmpty() ? TEXT("by its source only") : *FString::Join(Routes, TEXT(" OR ")),
						   RewardList.IsEmpty() ? TEXT("none") : *RewardList);
}

TArray<TArray<FMemoryCondition>> UMemoryData::GetConditionSets() const
{
	TArray<TArray<FMemoryCondition>> Sets;
	if (!UnlockConditions.IsEmpty())
	{
		Sets.Add(UnlockConditions);
	}
	for (const FMemoryConditionSet &Alternative : AlternativeConditionSets)
	{
		if (!Alternative.Conditions.IsEmpty())
		{
			Sets.Add(Alternative.Conditions);
		}
	}
	return Sets;
}

FString UMemoryData::GetRewardString(const FMemoryReward &Reward)
{
	const FString Kind = UEnum::GetDisplayValueAsText(Reward.Kind).ToString();
	switch (Reward.Kind)
	{
	case EMemoryRewardKind::Item:
		return FString::Printf(TEXT("%s %s"), *Kind, Reward.Item ? *Reward.Item->GetName() : TEXT("<none>"));
	case EMemoryRewardKind::Skill:
		return FString::Printf(TEXT("%s %s"), *Kind, Reward.Skill ? *Reward.Skill->GetName() : TEXT("<none>"));
	case EMemoryRewardKind::Title:
	case EMemoryRewardKind::Curse:
		return FString::Printf(TEXT("%s %s"), *Kind, *Reward.RewardId.ToString());
	}
	return Kind;
}

FString UMemoryData::GetConditionString(const FMemoryCondition &Condition)
{
	const auto NameOf = [](const UObject *Object) { return Object ? Object->GetName() : FString(TEXT("<none>")); };
	switch (Condition.Kind)
	{
	case EMemoryConditionKind::Character:
		return FString::Printf(TEXT("%s in the party"), *NameOf(Condition.Character));
	case EMemoryConditionKind::GearEquipped:
		return FString::Printf(TEXT("%s worn by %s at +%d"), *NameOf(Condition.Item),
							   Condition.Character ? *NameOf(Condition.Character) : TEXT("anyone"), Condition.UpgradeLevel);
	case EMemoryConditionKind::Location:
		return FString::Printf(TEXT("at %s"), *Condition.LocationId.ToString());
	case EMemoryConditionKind::EnemyBeaten:
		return FString::Printf(TEXT("%s beaten"), *NameOf(Condition.Enemy));
	case EMemoryConditionKind::NoDamageTaken:
		return TEXT("won with no damage taken");
	case EMemoryConditionKind::PriorMemory:
		return FString::Printf(TEXT("after %s"), *NameOf(Condition.PriorMemory));
	case EMemoryConditionKind::Hidden:
		return TEXT("hidden (not built)");
	}
	return TEXT("<unknown>");
}

void UMemoryData::PrintMemory() const
{
	UE_LOG(LogWoR, Log, TEXT("%s"), *GetMemoryString());
}
