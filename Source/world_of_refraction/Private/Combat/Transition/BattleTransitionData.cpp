// BattleTransitionData.cpp

#include "Combat/Transition/BattleTransitionData.h"

#include "Debug/Combat/BattleTransitionDebug.h"
#include "UObject/UObjectGlobals.h"
#include "WoRLog.h"

const UBattleTransitionData &UBattleTransitionData::Get()
{
	const UBattleTransitionData *Asset = Cast<UBattleTransitionData>(
		StaticLoadObject(StaticClass(), nullptr, BattleTransitionAsset::PATH, nullptr, LOAD_NoWarn));
	return Asset ? *Asset : *GetDefault<UBattleTransitionData>();
}

#if WITH_EDITOR
void UBattleTransitionData::LogValues() const
{
	UE_LOG(LogWoR, Display, TEXT("[BattleTransition] %s: %s"), *GetName(), *WoRBattleTransitionDebug::GetDataString(*this));
}
#endif
