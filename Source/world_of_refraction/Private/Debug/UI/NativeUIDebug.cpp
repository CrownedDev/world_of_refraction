// NativeUIDebug.cpp

#include "Debug/UI/NativeUIDebug.h"
#include "Combat/CombatOrchestrator.h"
#include "Hub/ExplorationPlayerController.h"
#include "Player/WoRHubController.h"
#include "UI/Combat/CombatHUD.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace NativeUIDebugConstants
{
    constexpr float ScreenMessageSeconds = 8.0f;
}

FString NativeUIDebug::GetNativeUIStateString(UWorld *World)
{
    if (!World)
    {
        return TEXT("[NativeUI] no world");
    }

    FString Out;
    int32 OrchestratorCount = 0;
    for (TActorIterator<ACombatOrchestrator> It(World); It; ++It)
    {
        ++OrchestratorCount;
        const UCombatHUD *HUD = It->GetCombatHUD();
        Out += FString::Printf(TEXT("%s: %s\n"), *It->GetName(),
                               HUD ? *HUD->GetHUDStateString() : TEXT("[CombatHUD] not created (BP override active, or combat not started)"));
    }
    if (OrchestratorCount == 0)
    {
        Out += TEXT("[CombatHUD] no orchestrator in world\n");
    }

    APlayerController *PC0 = World->GetFirstPlayerController();
    if (const AWoRHubController *Hub = Cast<AWoRHubController>(PC0))
    {
        Out += Hub->GetInteractionStateString();
    }
    else if (const AExplorationPlayerController *ExplorationPC = Cast<AExplorationPlayerController>(PC0))
    {
        Out += ExplorationPC->GetInteractionStateString(); // OBSOLETE (B-158): retired controller
    }
    else
    {
        Out += TEXT("[WoRHubController] PC0 is not an AWoRHubController");
    }
    return Out;
}

void NativeUIDebug::PrintNativeUIState(UWorld *World)
{
    const FString State = GetNativeUIStateString(World);
    UE_LOG(LogTemp, Log, TEXT("%s"), *State);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(INDEX_NONE, NativeUIDebugConstants::ScreenMessageSeconds, FColor::Cyan, State);
    }
}

static FAutoConsoleCommandWithWorld GNativeUIStateCommand(
    TEXT("wor.UIState"),
    TEXT("Print native UI state: combat HUD widgets per orchestrator + exploration-controller interact candidates."),
    FConsoleCommandWithWorldDelegate::CreateStatic(&NativeUIDebug::PrintNativeUIState));
