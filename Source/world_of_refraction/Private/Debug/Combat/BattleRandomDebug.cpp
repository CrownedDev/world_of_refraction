// BattleRandomDebug.cpp

#include "Debug/Combat/BattleRandomDebug.h"
#include "Combat/Mechanics/BattleRandomSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float BATTLE_RANDOM_SCREEN_SECONDS = 8.0f;

	FAutoConsoleCommandWithWorld GPrintBattleRandomCmd(
		TEXT("wor.Battle.PrintRandom"),
		TEXT("DEBUG: print the current battle's random seed and draw count."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				UBattleRandomDebug::PrintBattleRandomState(World);
			}));
}

FString UBattleRandomDebug::GetBattleRandomString(const UObject *WorldContextObject)
{
	const UBattleRandomSubsystem *Battle = UBattleRandomSubsystem::Get(WorldContextObject);
	if (!Battle)
	{
		return TEXT("No battle stream (no world)");
	}
	return FString::Printf(TEXT("Battle seed %d, %d draws (replay: wor.Battle.Seed %d)"),
						   Battle->GetSeed(), Battle->GetDrawCount(), Battle->GetSeed());
}

void UBattleRandomDebug::PrintBattleRandomState(const UObject *WorldContextObject)
{
	const FString State = GetBattleRandomString(WorldContextObject);
	UE_LOG(LogWoRCombat, Display, TEXT("[BattleRandom] %s"), *State);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, BATTLE_RANDOM_SCREEN_SECONDS, FColor::Cyan, State);
	}
}
