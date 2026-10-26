// StoneRowDebug.cpp

#include "Debug/Equipment/StoneRowDebug.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Crystals/StoneRowTable.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	FAutoConsoleCommand GPrintStoneRowsCmd(
		TEXT("wor.PrintStoneRows"),
		TEXT("DEBUG: print every stat stone's row (attached stat, consumable effect, buff/debuff, base percent)."),
		FConsoleCommandDelegate::CreateStatic(&UStoneRowDebug::PrintStoneRows));

	template <typename TEnum>
	FString StoneDebugName(TEnum Value)
	{
		return StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(Value));
	}
}

FString UStoneRowDebug::GetStoneRowsString()
{
	TArray<FString> Lines;
	for (const FStoneRow &Row : StoneRowTable::ROWS)
	{
		Lines.Add(FString::Printf(TEXT("%s: stat %s, item %s, %s / %s, %.0f-%.0f%%%s"),
								  *StoneDebugName(Row.Type), *StoneDebugName(Row.Stat), *StoneDebugName(Row.ItemEffect),
								  *StoneDebugName(Row.Buff), *StoneDebugName(Row.Debuff),
								  CrystalEffectTable::GetStoneBasePercent(Row.Type, EItemTier::F_Tier),
								  CrystalEffectTable::GetStoneBasePercent(Row.Type, EItemTier::S_Tier),
								  Row.bStandardConsumable ? TEXT("") : TEXT(" (own consumable handler)")));
	}
	return FString::Join(Lines, TEXT("\n"));
}

void UStoneRowDebug::PrintStoneRows()
{
	UE_LOG(LogWoR, Display, TEXT("[StoneRows]\n%s"), *GetStoneRowsString());
}
