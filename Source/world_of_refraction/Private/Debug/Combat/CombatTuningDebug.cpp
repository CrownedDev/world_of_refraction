// CombatTuningDebug.cpp

#include "Debug/Combat/CombatTuningDebug.h"
#include "Combat/Tuning/CombatTuningData.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float TUNING_DEBUG_SCREEN_SECONDS = 8.0f;

	FAutoConsoleCommand GPrintCombatTuningCmd(
		TEXT("wor.PrintCombatTuning"),
		TEXT("DEBUG: print the live combat tuning values (asset or C++ defaults)."),
		FConsoleCommandDelegate::CreateLambda([]() { UCombatTuningDebug::PrintCombatTuning(); }));

	FAutoConsoleCommand GCompareCombatTuningCmd(
		TEXT("wor.CompareCombatTuning"),
		TEXT("DEBUG: list every live combat tuning value that differs from the C++ constants."),
		FConsoleCommandDelegate::CreateLambda([]() { UCombatTuningDebug::CompareCombatTuning(); }));
}

FString UCombatTuningDebug::GetActiveTuningString()
{
	const UCombatTuningData *Active = UCombatTuningData::GetActive();
	return FString::Printf(TEXT("Live: %s\n%s"), Active ? *Active->GetPathName() : TEXT("C++ defaults (no tuning asset)"),
						   *WoRTuning::Get().GetCombatTuningString());
}

int32 UCombatTuningDebug::CompareCombatTuning()
{
	const FString Differences = WoRTuning::Get().GetDifferencesFromCode();
	TArray<FString> Lines;
	Differences.ParseIntoArrayLines(Lines);
	const FString Summary = FString::Printf(TEXT("[CompareCombatTuning] %d value(s) differ from the C++ constants"), Lines.Num());
	UE_LOG(LogWoRCombat, Log, TEXT("%s\n%s"), *Summary, *Differences);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, TUNING_DEBUG_SCREEN_SECONDS, Lines.Num() == 0 ? FColor::Green : FColor::Yellow, Summary);
	}
	return Lines.Num();
}

void UCombatTuningDebug::PrintCombatTuning()
{
	const FString Text = GetActiveTuningString();
	UE_LOG(LogWoRCombat, Log, TEXT("%s"), *Text);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, TUNING_DEBUG_SCREEN_SECONDS, FColor::Cyan, Text);
	}
}
