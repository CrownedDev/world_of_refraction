// CurseData.cpp

#include "Memory/CurseData.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Currency/EconomyYield.h"
#include "WoRLog.h"

ECurrencyType UCurseData::GetClearCurrency() const
{
	return ClearEssence == ECurseClearEssence::Elemental ? ECurrencyType::ElementalEssence : ECurrencyType::PillarEssence;
}

int32 UCurseData::GetClearPrice() const
{
	// D-169: the item essence price ladder's step for this tier (starting values, tune in PIE).
	return EconomyYield::GetTypedEssencePurchaseCostForTier(Tier);
}

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
	return FString::Printf(TEXT("%s (%s, %s): HP %+d, Defence %+d, raw damage %+d, spell damage %+d, luck %+d; Fire resist %+.0f; clear costs %d %s"),
						   *CurseId.ToString(), *Title.ToString(), *TierHelpers::GetTierDisplayString(Tier), P.BonusMaxHP, P.BonusDefense,
						   P.BonusRawDamage, P.BonusSpellDamage, P.BonusLuck, ResistancePenalty.Fire, GetClearPrice(),
						   GetClearCurrency() == ECurrencyType::ElementalEssence ? TEXT("Elemental Essence") : TEXT("Pillar Essence"));
}

void UCurseData::PrintCurse() const
{
	UE_LOG(LogWoR, Display, TEXT("[Curse] %s"), *GetCurseString());
}
