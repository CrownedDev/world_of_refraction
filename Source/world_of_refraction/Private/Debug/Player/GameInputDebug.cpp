// GameInputDebug.cpp

#include "Debug/Player/GameInputDebug.h"

#if !UE_BUILD_SHIPPING

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SViewport.h"
#include "WoRLog.h"

namespace
{
	constexpr float INPUT_STATE_SCREEN_SECONDS = 10.0f;

	const TCHAR *GetFocusName(const UGameViewportClient &Viewport)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return TEXT("unknown (no Slate)");
		}
		const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
		const TSharedPtr<SWidget> Game = Viewport.GetGameViewportWidget();
		if (!Focused.IsValid())
		{
			return TEXT("on nothing");
		}
		if (Focused == Game)
		{
			return TEXT("on the game");
		}
		for (TSharedPtr<SWidget> Widget = Focused; Widget.IsValid(); Widget = Widget->GetParentWidget())
		{
			if (Widget == Game)
			{
				return TEXT("on a widget in the game");
			}
		}
		return TEXT("outside the game");
	}

	FAutoConsoleCommandWithWorld GPrintInputStateCmd(
		TEXT("wor.Input.PrintState"),
		TEXT("DEBUG (B-578): whether the game viewport ignores game input, the mouse capture and lock, the cursor, keyboard focus."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				const FString State = WoRGameInputDebug::GetStateString(World);
				UE_LOG(LogWoR, Log, TEXT("%s"), *State);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, INPUT_STATE_SCREEN_SECONDS, FColor::Cyan, State);
				}
			}));
}

FString WoRGameInputDebug::GetStateString(const UWorld *World)
{
	const UGameViewportClient *Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		return TEXT("[GameInput] no game viewport here");
	}
	const APlayerController *Controller = World->GetFirstPlayerController();
	return FString::Printf(TEXT("[GameInput] game input: %s; mouse capture: %s; mouse lock: %s; cursor: %s; keyboard focus: %s"),
						   const_cast<UGameViewportClient *>(Viewport)->IgnoreInput() ? TEXT("IGNORED (a menu-only mode is on)") : TEXT("on"),
						   *UEnum::GetValueAsString(Viewport->GetMouseCaptureMode()), *UEnum::GetValueAsString(Viewport->GetMouseLockMode()),
						   Controller && Controller->ShouldShowMouseCursor() ? TEXT("shown") : TEXT("hidden"), GetFocusName(*Viewport));
}

#endif // !UE_BUILD_SHIPPING
