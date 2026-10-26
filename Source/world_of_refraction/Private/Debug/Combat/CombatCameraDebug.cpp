// CombatCameraDebug.cpp

#include "Debug/Combat/CombatCameraDebug.h"
#include "Combat/Camera/CombatCameraDirector.h"
#include "Combat/CombatOrchestrator.h"
#include "LevelSequence.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float COMBAT_CAMERA_SCREEN_SECONDS = 8.0f;

	FAutoConsoleCommandWithWorld GPrintCombatCameraCmd(
		TEXT("wor.Camera"),
		TEXT("DEBUG: print the combat camera's mode, viewed character, settings and playing sequence."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				UCombatCameraDebug::PrintCameraState(World);
			}));

	FAutoConsoleCommandWithWorldAndArgs GCombatCameraStyleCmd(
		TEXT("wor.Camera.Style"),
		TEXT("DEBUG: turn-view style. 0 = centred-wide (default), 1 = over-the-shoulder, 2 = tactical side-on."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				UCombatCameraDirector *Director = UCombatCameraDebug::FindDirector(World);
				const int32 Style = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
				if (!Director || Style < 0 || Style > static_cast<int32>(ETurnViewStyle::Tactical))
				{
					UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] wor.Camera.Style: no combat orchestrator, or style not 0-2."));
					return;
				}
				Director->SetTurnViewStyle(static_cast<ETurnViewStyle>(Style));
				UCombatCameraDebug::PrintCameraState(World);
			}));

	FAutoConsoleCommandWithWorldAndArgs GCombatCameraMovementOffCmd(
		TEXT("wor.Camera.MovementOff"),
		TEXT("DEBUG: 1 = camera movement off (one static wide view, no sequences), 0 = on."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				UCombatCameraDirector *Director = UCombatCameraDebug::FindDirector(World);
				if (!Director)
				{
					UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] wor.Camera.MovementOff: no combat orchestrator."));
					return;
				}
				Director->SetCameraMovementOff(Args.Num() > 0 && FCString::Atoi(*Args[0]) != 0);
				UCombatCameraDebug::PrintCameraState(World);
			}));
}

FString UCombatCameraDebug::GetCameraStateString(const UCombatCameraDirector *Director)
{
	if (!Director)
	{
		return TEXT("No combat camera (no combat orchestrator in this world)");
	}
	return FString::Printf(TEXT("%s | mode %s | viewing %s | style %s | movement %s | sequence %s | %d cameras"),
						   Director->IsInCombat() ? TEXT("In combat") : TEXT("Not in combat"),
						   *UEnum::GetValueAsString(Director->GetCurrentMode()),
						   *GetNameSafe(Director->GetViewedActor()),
						   *UEnum::GetValueAsString(Director->GetTurnViewStyle()),
						   Director->IsCameraMovementOff() ? TEXT("off") : TEXT("on"),
						   Director->GetPlayingSequence() ? *Director->GetPlayingSequence()->GetName() : TEXT("none"),
						   Director->GetCameraCount());
}

UCombatCameraDirector *UCombatCameraDebug::FindDirector(const UObject *WorldContextObject)
{
	UWorld *World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ACombatOrchestrator> It(World); It; ++It)
	{
		if (UCombatCameraDirector *Director = It->GetCameraDirector())
		{
			return Director;
		}
	}
	return nullptr;
}

void UCombatCameraDebug::PrintCameraState(const UObject *WorldContextObject)
{
	const FString State = GetCameraStateString(FindDirector(WorldContextObject));
	UE_LOG(LogWoRCombat, Display, TEXT("[CombatCamera] %s"), *State);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, COMBAT_CAMERA_SCREEN_SECONDS, FColor::Cyan, State);
	}
}
