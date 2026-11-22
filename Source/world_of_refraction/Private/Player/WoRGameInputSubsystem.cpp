// WoRGameInputSubsystem.cpp

#include "Player/WoRGameInputSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "WoRLog.h"

void UWoRGameInputSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	// OpenLevel announces itself with PreLoadMap, seamless ServerTravel with OnSeamlessTravelStart.
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &UWoRGameInputSubsystem::HandlePreLoadMap);
	FWorldDelegates::OnSeamlessTravelStart.AddUObject(this, &UWoRGameInputSubsystem::HandleSeamlessTravelStart);
}

void UWoRGameInputSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	FWorldDelegates::OnSeamlessTravelStart.RemoveAll(this);
	Super::Deinitialize();
}

void UWoRGameInputSubsystem::ApplyGameOnly(UGameViewportClient &Viewport)
{
	// The viewport half of FInputModeGameOnly::ApplyInputMode.
	Viewport.SetMouseLockMode(EMouseLockMode::LockOnCapture);
	Viewport.SetIgnoreInput(false);
	Viewport.SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently);
}

bool UWoRGameInputSubsystem::RestoreGameInput(const FWorldContext &WorldContext)
{
	UGameViewportClient *Viewport = WorldContext.GameViewport;
	if (WorldContext.OwningGameInstance != GetGameInstance() || !Viewport)
	{
		return false;
	}
	const bool bWasIgnoring = Viewport->IgnoreInput();
	// Focus and the mouse go back to the game through each player's own controller, as when a menu closes.
	for (const ULocalPlayer *LocalPlayer : GetGameInstance()->GetLocalPlayers())
	{
		APlayerController *Controller = LocalPlayer ? LocalPlayer->GetPlayerController(WorldContext.World()) : nullptr;
		if (Controller)
		{
			Controller->SetInputMode(FInputModeGameOnly());
		}
	}
	ApplyGameOnly(*Viewport); // also with no player or viewport widget to apply a mode through
	if (bWasIgnoring)
	{
		UE_LOG(LogWoR, Log, TEXT("[GameInput] A menu-only input mode was still on at a level change: game input given back"));
	}
	return bWasIgnoring;
}

void UWoRGameInputSubsystem::HandlePreLoadMap(const FWorldContext &WorldContext, const FString &MapName)
{
	RestoreGameInput(WorldContext);
}

void UWoRGameInputSubsystem::HandleSeamlessTravelStart(UWorld *World, const FString &MapName)
{
	const FWorldContext *WorldContext = World && GEngine ? GEngine->GetWorldContextFromWorld(World) : nullptr;
	if (WorldContext)
	{
		RestoreGameInput(*WorldContext);
	}
}
