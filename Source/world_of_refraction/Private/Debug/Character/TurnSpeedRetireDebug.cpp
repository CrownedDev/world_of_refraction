// TurnSpeedRetireDebug.cpp

#include "Debug/Character/TurnSpeedRetireDebug.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Character/CharacterData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/EquipmentDataBase.h"
#include "HAL/IConsoleManager.h"
#include "Reaction/ReactionTrigger.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Effects/EffectDefinition.h"
#include "WoRLog.h"

namespace
{
	void AddTurnSpeedEffectUses(const TArray<FSkillEffect> &Effects, TArray<FString> &OutUses)
	{
		for (int32 i = 0; i < Effects.Num(); ++i)
		{
			for (const FSkillEffectPayload &Payload : Effects[i].Payloads)
			{
				if (SkillEffectClassification::IsTurnSpeedFamily(Payload.EffectType))
				{
					OutUses.Add(FString::Printf(TEXT("effect %d: %s"), i, *UEnum::GetValueAsString(Payload.EffectType)));
				}
			}
		}
	}

	void AddTurnSpeedPointUses(const TCHAR *FieldName, int32 Points, TArray<FString> &OutUses)
	{
		if (Points != 0)
		{
			OutUses.Add(FString::Printf(TEXT("%s = %d"), FieldName, Points));
		}
	}

	FAutoConsoleCommand GListTurnSpeedCmd(
		TEXT("wor.Data.ListTurnSpeed"),
		TEXT("DEBUG (B-170g): list every asset that still authors the retired Turn Speed family (effects, skill scaling, "
			 "character points, gear and evolution bonuses). Loads the assets."),
		FConsoleCommandDelegate::CreateLambda([]() { UE_LOG(LogWoR, Display, TEXT("%s"), *TurnSpeedRetireDebug::GetTurnSpeedAssetsString()); }));
}

FString TurnSpeedRetireDebug::GetTurnSpeedUsesString(const UObject *Asset)
{
	TArray<FString> Uses;
	if (const UEffectDefinition *Definition = Cast<UEffectDefinition>(Asset))
	{
		AddTurnSpeedEffectUses(Definition->Effects, Uses);
	}
	else if (const USkillDataBase *Skill = Cast<USkillDataBase>(Asset))
	{
		for (const FStatScaling &Scaling : Skill->StatScaling)
		{
			if (Scaling.Stat == ESubStat::TurnSpeed)
			{
				Uses.Add(TEXT("StatScaling on Turn Speed"));
			}
		}
	}
	else if (const UCharacterData *Character = Cast<UCharacterData>(Asset))
	{
		AddTurnSpeedPointUses(TEXT("TurnSpeed"), Character->TurnSpeed, Uses);
	}
	else if (const UEquipmentDataBase *Gear = Cast<UEquipmentDataBase>(Asset))
	{
		AddTurnSpeedPointUses(TEXT("BaseStatBonus.BonusTurnSpeed"), Gear->BaseStatBonus.BonusTurnSpeed, Uses);
	}
	else if (const UEvolutionItemData *Evolution = Cast<UEvolutionItemData>(Asset))
	{
		AddTurnSpeedPointUses(TEXT("BaseStatBonus.BonusTurnSpeed"), Evolution->BaseStatBonus.BonusTurnSpeed, Uses);
	}
	else if (const UReactionTriggerData *Reactions = Cast<UReactionTriggerData>(Asset))
	{
		for (int32 TriggerIndex = 0; TriggerIndex < Reactions->Triggers.Num(); ++TriggerIndex)
		{
			TArray<FString> TriggerUses;
			AddTurnSpeedEffectUses(Reactions->Triggers[TriggerIndex].Effects, TriggerUses);
			for (const FString &Use : TriggerUses)
			{
				Uses.Add(FString::Printf(TEXT("trigger %d (%s) %s"), TriggerIndex, *Reactions->Triggers[TriggerIndex].TriggerId.ToString(), *Use));
			}
		}
	}
	return FString::Join(Uses, TEXT("; "));
}

FString TurnSpeedRetireDebug::GetTurnSpeedAssetsString()
{
	const IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	const UClass *const ScannedClasses[] = {UEffectDefinition::StaticClass(), USkillDataBase::StaticClass(), UCharacterData::StaticClass(),
											UEquipmentDataBase::StaticClass(), UEvolutionItemData::StaticClass(),
											UReactionTriggerData::StaticClass()};

	TArray<FString> Lines;
	for (const UClass *Class : ScannedClasses)
	{
		TArray<FAssetData> Assets;
		Registry.GetAssetsByClass(Class->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
		for (const FAssetData &Asset : Assets)
		{
			const FString Uses = GetTurnSpeedUsesString(Asset.GetAsset());
			if (!Uses.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("  %s: %s"), *Asset.GetObjectPathString(), *Uses));
			}
		}
	}

	FString Report = Lines.IsEmpty() ? FString(TEXT("=== Turn Speed (retired, B-170g): no asset authors it ==="))
									 : FString::Printf(TEXT("=== Turn Speed (retired, B-170g): %d asset(s) still author it ===\n%s"),
													   Lines.Num(), *FString::Join(Lines, TEXT("\n")));
	if (Registry.IsLoadingAssets())
	{
		Report += TEXT("\n  (asset scan still running: the list may be partial)");
	}
	return Report;
}
