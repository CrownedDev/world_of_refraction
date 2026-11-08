// ApparelData.cpp

#include "Equipment/Apparel/ApparelData.h"

#include "WoRLog.h"

FEquipmentStatBonus UApparelData::GetStatLines() const
{
	FEquipmentStatBonus Lines;
	Lines.BonusMaxHP = BaseStatBonus.BonusMaxHP;
	Lines.BonusDefense = BaseStatBonus.BonusDefense;
	Lines.Amp = BaseStatBonus.Amp; // B-386: the deckbuilder's Apparel tiles are HP, Amp and Def
	return Lines;
}

bool UApparelData::HasUnusedStatLines() const
{
	const FEquipmentStatBonus &B = BaseStatBonus;
	return B.BonusRawDamage != 0 || B.BonusSpellDamage != 0 || B.BonusEfficiency != 0 || B.BonusStatusMultiplier != 0 ||
		   !FMath::IsNearlyZero(B.BonusCritDamage) || B.BonusSpellSpeed != 0 || B.BonusActionSpeed != 0 || B.BonusReflex != 0 ||
		   B.BonusMaxEnergy != 0 || B.BonusResistance != 0 || B.BonusTurnSpeed != 0 || B.BonusLuck != 0 ||
		   !FMath::IsNearlyZero(B.BonusMindModifierPercent) || !FMath::IsNearlyZero(B.BonusBodyModifierPercent) ||
		   !FMath::IsNearlyZero(B.BonusSpiritModifierPercent);
}

FString UApparelData::GetApparelString() const
{
	const FEquipmentStatBonus Lines = GetStatLines();
	return FString::Printf(TEXT("%s (%s): HP %+d, Defence %+d, %s%s"), *GetName(), *UEnum::GetDisplayValueAsText(Tier).ToString(), Lines.BonusMaxHP,
						   Lines.BonusDefense, *Lines.Amp.ToString(), HasUnusedStatLines() ? TEXT(" [has stat lines apparel ignores]") : TEXT(""));
}

void UApparelData::PrintApparel() const
{
	UE_LOG(LogWoR, Display, TEXT("[Apparel] %s"), *GetApparelString());
}
