// WoRGameState.cpp

#include "Player/WoRGameState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	FAutoConsoleCommandWithWorld GPrintGameStateCmd(
		TEXT("wor.GameState.Print"),
		TEXT("DEBUG (B-227): the level's GameState class, net mode and players."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			const AWoRGameState *GameState = World ? World->GetGameState<AWoRGameState>() : nullptr;
			UE_LOG(LogTemp, Log, TEXT("[GameState] %s"),
				   GameState ? *GameState->GetDebugString()
							 : *FString::Printf(TEXT("no AWoRGameState (GameState %s)"), World ? *GetNameSafe(World->GetGameState()) : TEXT("-")));
		}));
}

FString AWoRGameState::GetDebugString() const
{
	const UWorld *World = GetWorld();
	const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
	FString Out = FString::Printf(TEXT("%s on %s, net mode %d, %d player(s)"), *GetName(), World ? *World->GetMapName() : TEXT("-"),
								  static_cast<int32>(NetMode), PlayerArray.Num());
	for (const APlayerState *Player : PlayerArray)
	{
		Out += FString::Printf(TEXT("\n  %s (%s)"), Player ? *Player->GetPlayerName() : TEXT("null"), *GetNameSafe(Player));
	}
	return Out;
}
