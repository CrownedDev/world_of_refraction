// AgentPlayDebug.cpp

#include "AgentPlay/AgentPlayDebug.h"
#include "AgentPlay/AgentPlayHold.h"
#include "AgentPlay/AgentPlayToolset.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

DEFINE_LOG_CATEGORY_STATIC(LogWoRAgentPlayDebug, Log, All);

FString WoRAgentPlayDebug::GetStateString()
{
	FWoRAgentPlayGame Game;
	FString Why;
	if (!UWoRAgentPlayToolset::FindGame(Game, Why))
	{
		return FString::Printf(TEXT("PIE: %s"), *Why);
	}
	FSlateApplication &Slate = FSlateApplication::Get();
	const FGeometry &Picture = Game.Viewport->GetTickSpaceGeometry();
	const FVector2D PictureSize(Picture.GetAbsoluteSize());
	const FVector2D WindowSize(Game.Window->GetClientSizeInScreen());
	const FVector2D Mouse = Slate.GetCursorPos() - FVector2D(Picture.GetAbsolutePosition());

	const TSharedPtr<SWidget> Focused = Slate.GetUserFocusedWidget(0);
	bool bFocusInGame = false;
	for (TSharedPtr<SWidget> Widget = Focused; Widget.IsValid() && !bFocusInGame; Widget = Widget->GetParentWidget())
	{
		bFocusInGame = Widget == Game.Viewport;
	}

	const int32 Pending = UWoRAgentPlayToolset::GetPendingEventCount();
	TArray<FString> Lines;
	Lines.Add(TEXT("PIE: running"));
	Lines.Add(FString::Printf(TEXT("Picture: %.0f x %.0f px"), PictureSize.X, PictureSize.Y));
	Lines.Add(FString::Printf(TEXT("Window: %.0f x %.0f px, %s"), WindowSize.X, WindowSize.Y,
							  Game.Window == FGlobalTabmanager::Get()->GetRootWindow() ? TEXT("the level editor's (can't be resized)") : TEXT("PIE's own")));
	Lines.Add(FString::Printf(TEXT("Keyboard focus: %s (%s the game)"), Focused.IsValid() ? *Focused->GetTypeAsString() : TEXT("none"), bFocusInGame ? TEXT("inside") : TEXT("outside")));
	const bool bHeld = WoRAgentPlayHold::IsTaken();
	Lines.Add(bHeld ? TEXT("Hold: taken (the agent has its own mouse pointer; the background throttle is off)")
					: TEXT("Hold: released (the mouse is the real pointer; the next play tool takes the hold)"));
	Lines.Add(bHeld && WoRAgentPlayHold::IsPointerParked() ? FString(TEXT("Mouse: the agent's, not on the game yet"))
														   : FString::Printf(TEXT("Mouse: %s, %.0f,%.0f in picture px"), bHeld ? TEXT("the agent's") : TEXT("the real pointer"), Mouse.X, Mouse.Y));
	Lines.Add(Pending > 0 ? FString::Printf(TEXT("Sequence: playing, %d left"), Pending) : FString(TEXT("Sequence: idle")));
	return FString::Join(Lines, TEXT("\n"));
}

void WoRAgentPlayDebug::PrintState()
{
	UE_LOG(LogWoRAgentPlayDebug, Log, TEXT("[AgentPlay]\n%s"), *GetStateString());
}

namespace
{
	FAutoConsoleCommand GAgentPlayPrintStateCmd(
		TEXT("wor.Agent.PrintState"),
		TEXT("DEBUG (B-573): the agent play tool's view of the PIE game: picture and window size, keyboard focus, mouse, sequence."),
		FConsoleCommandDelegate::CreateStatic(&WoRAgentPlayDebug::PrintState));
}
