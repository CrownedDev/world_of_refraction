// BossData.cpp

#include "Combat/Boss/BossData.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Character/CharacterData.h"
#include "Equipment/EquipmentDataBase.h"
#include "WoRLog.h"

namespace BossPresetConstants
{
	// TODO: tune every preset value.
	constexpr float FIRST_HP_POINT = 50.0f;
	constexpr float PLANNING_FIRST_HP_POINT = 60.0f;
	constexpr float LAST_HP_POINT = 25.0f;
	constexpr float MEMORY_LAST_STAND_HP_POINT = 15.0f;
	constexpr int32 PLANNING_STATUS_BREAKS = 2;
	constexpr int32 TIMING_TURN_COUNT = 6;
	constexpr float FIRST_PHASE_WEATHER = 0.5f;
	constexpr float SECOND_PHASE_WEATHER = 0.8f;
	constexpr float LAST_STAND_WEATHER = 1.0f;
	constexpr int32 PHASE_AGGRESSION_STEP = 1;
}

namespace
{
	FBossPhase MakePresetPhase(FName Id, EBossPhaseConditionKind Kind, float Value, float Weather)
	{
		FBossPhase Phase;
		Phase.PhaseId = Id;
		Phase.Conditions.Add({Kind, Value});
		Phase.AggressionSteps = BossPresetConstants::PHASE_AGGRESSION_STEP;
		Phase.WeatherIntensity = Weather;
		return Phase;
	}
}

TArray<FBossPhase> UBossData::GetPhasesToUse() const
{
	return Phases.IsEmpty() ? GetPresetPhases(Style) : Phases;
}

TArray<FBossPhase> UBossData::GetPresetPhases(EBossStyle Style)
{
	using namespace BossPresetConstants;
	switch (Style)
	{
	case EBossStyle::Planning:
		// Planning: statuses matter, so the second phase is a hidden status-break count.
		return {MakePresetPhase(TEXT("Planning1"), EBossPhaseConditionKind::HPBelowPercent, PLANNING_FIRST_HP_POINT, FIRST_PHASE_WEATHER),
				MakePresetPhase(TEXT("Planning2"), EBossPhaseConditionKind::StatusBreaks, PLANNING_STATUS_BREAKS, SECOND_PHASE_WEATHER)};
	case EBossStyle::Timing:
		// Timing: the fight speeds up after a while (hidden turn count, never an enrage).
		return {MakePresetPhase(TEXT("Timing1"), EBossPhaseConditionKind::HPBelowPercent, FIRST_HP_POINT, FIRST_PHASE_WEATHER),
				MakePresetPhase(TEXT("Timing2"), EBossPhaseConditionKind::TurnCount, TIMING_TURN_COUNT, SECOND_PHASE_WEATHER)};
	case EBossStyle::Both:
		return {MakePresetPhase(TEXT("Both1"), EBossPhaseConditionKind::HPBelowPercent, FIRST_HP_POINT, FIRST_PHASE_WEATHER),
				MakePresetPhase(TEXT("Both2"), EBossPhaseConditionKind::HPBelowPercent, LAST_HP_POINT, SECOND_PHASE_WEATHER)};
	case EBossStyle::None:
	default:
		return {};
	}
}

FBossPhase UBossData::GetMemoryAddedPhase()
{
	return MakePresetPhase(TEXT("MemoryLastStand"), EBossPhaseConditionKind::HPBelowPercent,
						   BossPresetConstants::MEMORY_LAST_STAND_HP_POINT, BossPresetConstants::LAST_STAND_WEATHER);
}

UBossData *UBossData::FindFor(const UCharacterData *Character)
{
	if (!Character)
	{
		return nullptr;
	}
	const IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UBossData::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
	for (const FAssetData &Asset : Assets)
	{
		UBossData *Data = Cast<UBossData>(Asset.GetAsset());
		if (Data && Data->Boss == Character)
		{
			return Data;
		}
	}
	return nullptr;
}

FString UBossData::GetBossDataString() const
{
	const TArray<FBossPhase> InUse = GetPhasesToUse();
	FString Out = FString::Printf(TEXT("Boss %s (%s, style %s): %d phase(s)%s"), *GetName(),
								  Boss ? *Boss->Name : TEXT("<no character>"), *UEnum::GetValueAsString(Style), InUse.Num(),
								  Phases.IsEmpty() && !InUse.IsEmpty() ? TEXT(" from the style preset") : TEXT(""));
	for (const FBossPhase &Phase : InUse)
	{
		TArray<FString> Conditions;
		for (const FBossPhaseCondition &Condition : Phase.Conditions)
		{
			Conditions.Add(FString::Printf(TEXT("%s %.0f"), *UEnum::GetDisplayValueAsText(Condition.Kind).ToString(), Condition.Value));
		}
		Out += FString::Printf(TEXT("\n  %s: when [%s] -> %d effect(s), AI +%d, weather %.2f%s"), *Phase.PhaseId.ToString(),
							   *FString::Join(Conditions, TEXT(" and ")), Phase.PhaseEffects.Num(), Phase.AggressionSteps,
							   Phase.WeatherIntensity, Phase.DialogueId.IsNone() ? TEXT("") : *FString::Printf(TEXT(", dialogue %s"), *Phase.DialogueId.ToString()));
	}
	for (const FBossTrigger &Trigger : Triggers)
	{
		Out += TEXT("\n  ") + GetTriggerString(Trigger);
	}
	return Out;
}

FString UBossData::GetTriggerString(const FBossTrigger &Trigger)
{
	const auto NameOf = [](const UObject *Object) { return Object ? Object->GetName() : FString(TEXT("anyone")); };
	TArray<FString> Conditions;
	for (const FBossTriggerCondition &Condition : Trigger.Conditions)
	{
		switch (Condition.Kind)
		{
		case EBossTriggerConditionKind::PartyMember:
			Conditions.Add(FString::Printf(TEXT("%s in the party"), *NameOf(Condition.Character)));
			break;
		case EBossTriggerConditionKind::GearWorn:
			Conditions.Add(FString::Printf(TEXT("%s worn by %s at +%d"), Condition.Item ? *Condition.Item->GetName() : TEXT("<none>"),
										   *NameOf(Condition.Character), Condition.UpgradeLevel));
			break;
		case EBossTriggerConditionKind::Hidden:
			Conditions.Add(TEXT("hidden (not built)"));
			break;
		}
	}
	TArray<FString> Results;
	for (const EBossTriggerResult Result : Trigger.Results)
	{
		Results.Add(UEnum::GetDisplayValueAsText(Result).ToString());
	}
	return FString::Printf(TEXT("Trigger %s: when [%s], at %s -> [%s]%s"), *Trigger.TriggerId.ToString(),
						   *FString::Join(Conditions, TEXT(" and ")), *Trigger.When.ToString(),
						   *FString::Join(Results, TEXT(", ")),
						   Trigger.DialogueId.IsNone() ? TEXT("") : *FString::Printf(TEXT(", dialogue %s"), *Trigger.DialogueId.ToString()));
}

void UBossData::PrintBossData() const
{
	UE_LOG(LogWoR, Log, TEXT("%s"), *GetBossDataString());
}
