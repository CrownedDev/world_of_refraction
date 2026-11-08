// CurseData.cpp

#include "Memory/CurseData.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "WoRLog.h"

TArray<UCurseData *> UCurseData::GatherAll()
{
	const IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UCurseData::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
	Assets.Sort([](const FAssetData &A, const FAssetData &B) { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });
	TArray<UCurseData *> Curses;
	for (const FAssetData &Asset : Assets)
	{
		if (UCurseData *Curse = Cast<UCurseData>(Asset.GetAsset()))
		{
			Curses.Add(Curse);
		}
	}
	return Curses;
}

FString UCurseData::GetCurseString() const
{
	const FEquipmentStatBonus &P = StatPenalty;
	return FString::Printf(TEXT("%s (%s): HP %+d, Defence %+d, raw damage %+d, spell damage %+d, luck %+d; Fire resist %+.0f"),
						   *CurseId.ToString(), *Title.ToString(), P.BonusMaxHP, P.BonusDefense, P.BonusRawDamage, P.BonusSpellDamage,
						   P.BonusLuck, ResistancePenalty.Fire);
}

void UCurseData::PrintCurse() const
{
	UE_LOG(LogWoR, Display, TEXT("[Curse] %s"), *GetCurseString());
}
