// AgentPlayToolset.cpp

#include "AgentPlay/AgentPlayToolset.h"
#include "AgentPlay/AgentPlayDebug.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CoreDelegates.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "Misc/StringOutputDevice.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

DEFINE_LOG_CATEGORY_STATIC(LogWoRAgentPlay, Log, All);

namespace
{
	const TCHAR *DEFAULT_SCREENSHOT_DIR = TEXT("Intermediate/AgentPlay");
	constexpr int32 MIN_WINDOW_SIDE = 240;
	constexpr int32 MAX_WINDOW_SIDE = 7680;
	/** A console command's output is cut here so one print can't flood the caller. */
	constexpr int32 MAX_CONSOLE_OUTPUT_CHARS = 20000;
	constexpr int32 KEYBOARD_USER = 0;
	constexpr int32 MOUSE_POINTER = 0;

	struct FAgentPlayRun
	{
		TArray<FWoRAgentPlayStep> Steps;
		TArray<FWoRAgentPlayEvent> Events;
		int32 NextEvent = 0;
		double StartSeconds = 0.0;
		int32 Frame = INDEX_NONE;
		FString ScreenshotDir;
		/** Indices into Report.Fired of the screenshots this frame takes once it is drawn. */
		TArray<int32> ShotsThisFrame;
		FWoRAgentPlayReport Report;
		TStrongObjectPtr<UWoRAgentPlayResult> Result;
		FDelegateHandle BeginFrameHandle;
		FDelegateHandle EndFrameHandle;
	};

	TUniquePtr<FAgentPlayRun> GRun;
	/** Mouse buttons a sequence left down, so a later move or release carries them. */
	TSet<FKey> GHeldMouseButtons;

	FString GetActionName(EWoRAgentPlayAction Action)
	{
		return StaticEnum<EWoRAgentPlayAction>()->GetNameStringByValue(static_cast<int64>(Action));
	}

	FIntPoint GetPictureSize(const FWoRAgentPlayGame &Game)
	{
		const FVector2D Size(Game.Viewport->GetTickSpaceGeometry().GetAbsoluteSize());
		return FIntPoint(FMath::RoundToInt32(Size.X), FMath::RoundToInt32(Size.Y));
	}

	bool CapturePicture(const FWoRAgentPlayGame &Game, TArray<FColor> &OutColors, FIntPoint &OutSize)
	{
		FIntVector Size;
		if (!FSlateApplication::Get().TakeScreenshot(Game.Viewport.ToSharedRef(), OutColors, Size))
		{
			return false;
		}
		for (FColor &Color : OutColors)
		{
			Color.A = MAX_uint8; // the window's alpha is not the picture's
		}
		OutSize = FIntPoint(Size.X, Size.Y);
		return true;
	}

	/** Keys go to whatever in the game has focus; when focus is outside the game (the editor, the console), the game takes it. */
	void GiveGameKeyboardFocus(const FWoRAgentPlayGame &Game)
	{
		FSlateApplication &Slate = FSlateApplication::Get();
		for (TSharedPtr<SWidget> Widget = Slate.GetUserFocusedWidget(KEYBOARD_USER); Widget.IsValid(); Widget = Widget->GetParentWidget())
		{
			if (Widget == Game.Viewport)
			{
				return;
			}
		}
		Slate.SetKeyboardFocus(Game.Viewport, EFocusCause::SetDirectly);
		UE_LOG(LogWoRAgentPlay, Log, TEXT("[AgentPlay] keyboard focus was outside the game: given to the game"));
	}

	FString SendEvent(const FWoRAgentPlayGame &Game, const FWoRAgentPlayEvent &Event, const FWoRAgentPlayStep &Step)
	{
		FSlateApplication &Slate = FSlateApplication::Get();
		const FModifierKeysState NoModifiers;
		const FVector2D Screen = FVector2D(Game.Viewport->GetTickSpaceGeometry().GetAbsolutePosition()) + FVector2D(Step.X, Step.Y);
		const FString Name = GetActionName(Event.Action);
		switch (Event.Action)
		{
		case EWoRAgentPlayAction::KeyDown:
			GiveGameKeyboardFocus(Game);
			Slate.ProcessKeyDownEvent(FKeyEvent(Event.Key, NoModifiers, KEYBOARD_USER, false, 0, 0));
			return FString::Printf(TEXT("%s %s"), *Name, *Event.Key.ToString());
		case EWoRAgentPlayAction::KeyUp:
			Slate.ProcessKeyUpEvent(FKeyEvent(Event.Key, NoModifiers, KEYBOARD_USER, false, 0, 0));
			return FString::Printf(TEXT("%s %s"), *Name, *Event.Key.ToString());
		case EWoRAgentPlayAction::MouseMove:
			// Slate reads the real pointer every frame, so the real pointer is what moves.
			Slate.SetCursorPos(Screen);
			Slate.ProcessMouseMoveEvent(FPointerEvent(MOUSE_POINTER, Screen, Screen, GHeldMouseButtons, EKeys::Invalid, 0.0f, NoModifiers));
			return FString::Printf(TEXT("%s %d,%d"), *Name, Step.X, Step.Y);
		case EWoRAgentPlayAction::MouseDown:
			GHeldMouseButtons.Add(Event.Key);
			Slate.ProcessMouseButtonDownEvent(Game.Window->GetNativeWindow(), FPointerEvent(MOUSE_POINTER, Screen, Screen, GHeldMouseButtons, Event.Key, 0.0f, NoModifiers));
			return FString::Printf(TEXT("%s %s %d,%d"), *Name, *Event.Key.ToString(), Step.X, Step.Y);
		case EWoRAgentPlayAction::MouseUp:
			GHeldMouseButtons.Remove(Event.Key);
			Slate.ProcessMouseButtonUpEvent(FPointerEvent(MOUSE_POINTER, Screen, Screen, GHeldMouseButtons, Event.Key, 0.0f, NoModifiers));
			return FString::Printf(TEXT("%s %s %d,%d"), *Name, *Event.Key.ToString(), Step.X, Step.Y);
		case EWoRAgentPlayAction::MouseWheel:
			Slate.ProcessMouseWheelOrGestureEvent(FPointerEvent(MOUSE_POINTER, Screen, Screen, GHeldMouseButtons, EKeys::MouseWheelAxis, Step.Wheel, NoModifiers), nullptr);
			return FString::Printf(TEXT("%s %.1f at %d,%d"), *Name, Step.Wheel, Step.X, Step.Y);
		default:
			return Name;
		}
	}

	void FinishRun(const FString &Error)
	{
		const TUniquePtr<FAgentPlayRun> Run = MoveTemp(GRun);
		FCoreDelegates::OnBeginFrame.Remove(Run->BeginFrameHandle);
		FCoreDelegates::OnEndFrame.Remove(Run->EndFrameHandle);
		if (Error.IsEmpty())
		{
			UE_LOG(LogWoRAgentPlay, Log, TEXT("[AgentPlay] sequence done: %d sent over %d frame(s)"), Run->Report.Fired.Num(), Run->Frame + 1);
			Run->Result->SetValue(Run->Report);
		}
		else
		{
			UE_LOG(LogWoRAgentPlay, Warning, TEXT("[AgentPlay] sequence stopped after %d of %d: %s"), Run->NextEvent, Run->Events.Num(), *Error);
			Run->Result->SetError(FString::Printf(TEXT("%s (%d of %d sent)"), *Error, Run->NextEvent, Run->Events.Num()));
		}
	}

	/** The frame's start, before the game reads input: what is due is sent now, so this frame's game tick sees it. */
	void OnBeginFrame()
	{
		if (!GRun)
		{
			return;
		}
		FWoRAgentPlayGame Game;
		FString Why;
		if (!UWoRAgentPlayToolset::FindGame(Game, Why))
		{
			FinishRun(Why);
			return;
		}
		FAgentPlayRun &Run = *GRun;
		const double Now = FPlatformTime::Seconds();
		if (++Run.Frame == 0)
		{
			Run.StartSeconds = Now;
		}
		const int32 ElapsedMs = FMath::RoundToInt32((Now - Run.StartSeconds) * 1000.0);
		const int32 Due = WoRAgentPlaySchedule::CountDue(Run.Events, Run.NextEvent, ElapsedMs);
		for (int32 Index = Run.NextEvent; Index < Run.NextEvent + Due; ++Index)
		{
			const FWoRAgentPlayEvent &Event = Run.Events[Index];
			FWoRAgentPlayFired Fired;
			Fired.Step = Event.StepIndex;
			Fired.AtMs = ElapsedMs;
			Fired.Frame = Run.Frame;
			Fired.What = SendEvent(Game, Event, Run.Steps[Event.StepIndex]);
			const int32 FiredIndex = Run.Report.Fired.Add(Fired);
			if (Event.Action == EWoRAgentPlayAction::Screenshot)
			{
				Run.ShotsThisFrame.Add(FiredIndex);
			}
		}
		Run.NextEvent += Due;
	}

	/** The frame's end, once the game has ticked and drawn: this frame's screenshots, then done when nothing is left. */
	void OnEndFrame()
	{
		if (!GRun)
		{
			return;
		}
		FAgentPlayRun &Run = *GRun;
		if (!Run.ShotsThisFrame.IsEmpty())
		{
			FWoRAgentPlayGame Game;
			FString Why;
			TArray<FColor> Colors;
			FIntPoint Size;
			if (!UWoRAgentPlayToolset::FindGame(Game, Why) || !CapturePicture(Game, Colors, Size))
			{
				FinishRun(Why.IsEmpty() ? TEXT("the screenshot could not be taken") : Why);
				return;
			}
			for (const int32 FiredIndex : Run.ShotsThisFrame)
			{
				FWoRAgentPlayFired &Fired = Run.Report.Fired[FiredIndex];
				Fired.File = FPaths::Combine(Run.ScreenshotDir, FString::Printf(TEXT("shot_step%02d_%05dms.png"), Fired.Step, Run.Steps[Fired.Step].AtMs));
				if (!FImageUtils::SaveImageByExtension(*Fired.File, FImageView(Colors.GetData(), Size.X, Size.Y)))
				{
					FinishRun(FString::Printf(TEXT("could not write %s"), *Fired.File));
					return;
				}
			}
			Run.ShotsThisFrame.Reset();
		}
		if (Run.NextEvent >= Run.Events.Num())
		{
			FinishRun(FString());
		}
	}

	FWoRAgentPlayStep MakeStep(EWoRAgentPlayAction Action, const FString &Key = FString(), int32 X = 0, int32 Y = 0)
	{
		FWoRAgentPlayStep Step;
		Step.Action = Action;
		Step.Key = Key;
		Step.X = X;
		Step.Y = Y;
		return Step;
	}
}

bool UWoRAgentPlayToolset::FindGame(FWoRAgentPlayGame &OutGame, FString &OutWhy)
{
	const FWorldContext *Context = GEditor ? GEditor->GetPIEWorldContext() : nullptr;
	if (!Context || !Context->World() || !Context->GameViewport || !FSlateApplication::IsInitialized())
	{
		OutWhy = TEXT("PIE is not running");
		return false;
	}
	OutGame.World = Context->World();
	OutGame.Viewport = Context->GameViewport->GetGameViewportWidget();
	OutGame.Window = Context->GameViewport->GetWindow();
	if (!OutGame.Viewport.IsValid() || !OutGame.Window.IsValid())
	{
		OutWhy = TEXT("the PIE game has no window");
		return false;
	}
	return true;
}

int32 UWoRAgentPlayToolset::GetPendingEventCount()
{
	return GRun ? GRun->Events.Num() - GRun->NextEvent : 0;
}

UWoRAgentPlayResult *UWoRAgentPlayToolset::Play(const TArray<FWoRAgentPlayStep> &Steps, const FString &ScreenshotDir)
{
	UWoRAgentPlayResult *Result = NewObject<UWoRAgentPlayResult>();
	FWoRAgentPlayGame Game;
	FString Why;
	if (GRun)
	{
		Why = FString::Printf(TEXT("a sequence is still playing (%d left)"), GetPendingEventCount());
	}
	else if (FindGame(Game, Why))
	{
		Why = WoRAgentPlaySchedule::Validate(Steps, GetPictureSize(Game));
	}
	if (!Why.IsEmpty())
	{
		Result->SetError(Why);
		return Result;
	}

	GRun = MakeUnique<FAgentPlayRun>();
	GRun->Steps = Steps;
	GRun->Events = WoRAgentPlaySchedule::BuildEvents(Steps);
	GRun->ScreenshotDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ScreenshotDir.IsEmpty() ? DEFAULT_SCREENSHOT_DIR : *ScreenshotDir);
	const FIntPoint Size = GetPictureSize(Game);
	GRun->Report.Width = Size.X;
	GRun->Report.Height = Size.Y;
	GRun->Result.Reset(Result);
	GRun->BeginFrameHandle = FCoreDelegates::OnBeginFrame.AddStatic(&OnBeginFrame);
	GRun->EndFrameHandle = FCoreDelegates::OnEndFrame.AddStatic(&OnEndFrame);
	UE_LOG(LogWoRAgentPlay, Log, TEXT("[AgentPlay] sequence started: %d step(s), %d input(s) and screenshot(s)"), Steps.Num(), GRun->Events.Num());
	return Result;
}

UWoRAgentPlayResult *UWoRAgentPlayToolset::PlaySequence(const TArray<FWoRAgentPlayStep> &Steps, const FString &ScreenshotDir)
{
	return Play(Steps, ScreenshotDir);
}

UWoRAgentPlayResult *UWoRAgentPlayToolset::TapKey(const FString &Key, int32 HoldMs)
{
	FWoRAgentPlayStep Step = MakeStep(EWoRAgentPlayAction::KeyTap, Key);
	Step.HoldMs = HoldMs;
	return Play({Step}, FString());
}

UWoRAgentPlayResult *UWoRAgentPlayToolset::KeyDown(const FString &Key)
{
	return Play({MakeStep(EWoRAgentPlayAction::KeyDown, Key)}, FString());
}

UWoRAgentPlayResult *UWoRAgentPlayToolset::KeyUp(const FString &Key)
{
	return Play({MakeStep(EWoRAgentPlayAction::KeyUp, Key)}, FString());
}

UWoRAgentPlayResult *UWoRAgentPlayToolset::MoveMouse(int32 X, int32 Y)
{
	return Play({MakeStep(EWoRAgentPlayAction::MouseMove, FString(), X, Y)}, FString());
}

UWoRAgentPlayResult *UWoRAgentPlayToolset::Click(int32 X, int32 Y, const FString &Button, int32 HoldMs)
{
	FWoRAgentPlayStep Step = MakeStep(EWoRAgentPlayAction::MouseClick, Button, X, Y);
	Step.HoldMs = HoldMs;
	return Play({Step}, FString());
}

UWoRAgentPlayResult *UWoRAgentPlayToolset::Wheel(int32 X, int32 Y, float Notches)
{
	FWoRAgentPlayStep Step = MakeStep(EWoRAgentPlayAction::MouseWheel, FString(), X, Y);
	Step.Wheel = Notches;
	return Play({Step}, FString());
}

FToolsetImage UWoRAgentPlayToolset::Screenshot()
{
	FToolsetImage Image;
	FWoRAgentPlayGame Game;
	FString Why;
	TArray<FColor> Colors;
	FIntPoint Size;
	if (!FindGame(Game, Why))
	{
		UKismetSystemLibrary::RaiseScriptError(Why);
	}
	else if (!CapturePicture(Game, Colors, Size) || !Image.SetFromBitmap(Colors, Size))
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("the screenshot could not be taken"));
	}
	return Image;
}

void UWoRAgentPlayToolset::SetWindowSize(int32 Width, int32 Height)
{
	FWoRAgentPlayGame Game;
	FString Why;
	if (!FindGame(Game, Why))
	{
		UKismetSystemLibrary::RaiseScriptError(Why);
		return;
	}
	if (Width < MIN_WINDOW_SIDE || Height < MIN_WINDOW_SIDE || Width > MAX_WINDOW_SIDE || Height > MAX_WINDOW_SIDE)
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("each side must be between %d and %d pixels"), MIN_WINDOW_SIDE, MAX_WINDOW_SIDE));
		return;
	}
	if (Game.Window == FGlobalTabmanager::Get()->GetRootWindow())
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("PIE runs inside the level editor: start it in its own window (Play > New Editor Window) to resize it"));
		return;
	}
	// The window keeps whatever it draws around the game picture (its title bar).
	const FVector2D Around = FVector2D(Game.Window->GetClientSizeInScreen()) - FVector2D(Game.Viewport->GetTickSpaceGeometry().GetAbsoluteSize());
	Game.Window->Resize(FVector2D(Width, Height) + Around);
	UE_LOG(LogWoRAgentPlay, Log, TEXT("[AgentPlay] PIE window resized for a %d x %d picture"), Width, Height);
}

FString UWoRAgentPlayToolset::RunConsoleCommand(const FString &Command)
{
	FWoRAgentPlayGame Game;
	FString Why;
	if (!FindGame(Game, Why))
	{
		UKismetSystemLibrary::RaiseScriptError(Why);
		return FString();
	}
	FStringOutputDevice Output;
	Output.SetAutoEmitLineTerminator(true);
	GLog->AddOutputDevice(&Output); // most wor. commands answer in the log, not in the command's own output
	Output.Reset();					// anything the log replays to a new listener is not this command's
	ULocalPlayer *Player = Game.World->GetFirstLocalPlayerFromController();
	const bool bKnown = Player ? Player->Exec(Game.World, *Command, Output) : GEngine->Exec(Game.World, *Command, Output);
	GLog->Flush();
	GLog->RemoveOutputDevice(&Output);
	if (!bKnown)
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("'%s' is not a console command here"), *Command));
		return FString();
	}
	if (Output.Len() > MAX_CONSOLE_OUTPUT_CHARS)
	{
		return Output.Left(MAX_CONSOLE_OUTPUT_CHARS) + TEXT("\n(cut: the rest is in the editor's log)");
	}
	return MoveTemp(Output);
}

FString UWoRAgentPlayToolset::GetState()
{
	return WoRAgentPlayDebug::GetStateString();
}
