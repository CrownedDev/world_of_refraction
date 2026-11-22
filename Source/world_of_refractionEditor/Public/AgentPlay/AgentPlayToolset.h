// AgentPlayToolset.h
// B-573 (D-251): the test tool an agent plays the game with, over unreal-mcp. Editor only.
// How to use it: docs/Conventions/HandsOffPlan.md, "How an agent plays". Design: docs/Architecture/AgentPlay.md.

#pragma once

#include "AgentPlay/AgentPlaySchedule.h"
#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolCallAsyncResult.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "ToolsetRegistry/ToolsetImage.h"

#include "AgentPlayToolset.generated.h"

class SViewport;
class SWindow;
class UWorld;

/** One input or screenshot of a sequence, as it happened. */
USTRUCT(BlueprintType)
struct FWoRAgentPlayFired
{
	GENERATED_BODY()

	/** Index of the step in the sequence given. */
	UPROPERTY(BlueprintReadOnly, Category = "WoRAgentPlay")
	int32 Step = 0;

	/** What was sent, e.g. "KeyDown Tab", "MouseMove 640,360", "Screenshot". */
	UPROPERTY(BlueprintReadOnly, Category = "WoRAgentPlay")
	FString What;

	/** Milliseconds after the sequence's first frame, measured at the start of the frame that took it. */
	UPROPERTY(BlueprintReadOnly, Category = "WoRAgentPlay")
	int32 AtMs = 0;

	/** The frame that took it; the sequence's first frame is 0. */
	UPROPERTY(BlueprintReadOnly, Category = "WoRAgentPlay")
	int32 Frame = 0;

	/** A screenshot's PNG file (full path); empty otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "WoRAgentPlay")
	FString File;
};

/** What a finished sequence did. */
USTRUCT(BlueprintType)
struct FWoRAgentPlayReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WoRAgentPlay")
	TArray<FWoRAgentPlayFired> Fired;

	/** The game picture's size in pixels when the sequence started: the frame mouse points are given in. */
	UPROPERTY(BlueprintReadOnly, Category = "WoRAgentPlay")
	int32 Width = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WoRAgentPlay")
	int32 Height = 0;
};

/** Completes when a sequence has sent its last input. */
UCLASS(BlueprintType)
class UWoRAgentPlayResult : public UToolCallAsyncResult
{
	GENERATED_BODY()

public:
	bool SetValue(const FWoRAgentPlayReport &InValue)
	{
		return MaybeBroadcastSuccessfulCompletion(FWoRAgentPlayReport(InValue), Value);
	}

	UPROPERTY(BlueprintReadOnly, Category = "ToolsetRegistry")
	FWoRAgentPlayReport Value;
};

/** The running PIE game's world, picture and window. */
struct FWoRAgentPlayGame
{
	UWorld *World = nullptr;
	TSharedPtr<SViewport> Viewport;
	TSharedPtr<SWindow> Window;
};

/**
 * Plays World of Refraction in the running PIE game window as a player would: keys, mouse and wheel sent to the game
 * (through Enhanced Input and the menus), timed input sequences with screenshots, the window's size, and console
 * commands run in the game's world. Mouse points are pixels of the game's own screenshot. The agent has its own
 * mouse pointer: the real one is never moved or read, and the editor keeps full speed in the background, until
 * Release or the end of PIE.
 */
UCLASS(BlueprintType, MinimalAPI)
class UWoRAgentPlayToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Plays a timed sequence of inputs and screenshots without stopping the game. Each step is sent at the start of
	 * the first frame at or after its time; a screenshot shows that frame once drawn. One sequence at a time.
	 * @param Steps The inputs and screenshots, in any order.
	 * @param ScreenshotDir Folder for the PNGs, absolute or relative to the project. Empty: Intermediate/AgentPlay. Files of the same step and time are overwritten.
	 * @return What was sent, with each one's real time and frame, and the screenshots' files.
	 */
	UFUNCTION(meta = (AICallable), Category = "WoRAgentPlay")
	static WORLD_OF_REFRACTIONEDITOR_API UWoRAgentPlayResult *PlaySequence(const TArray<FWoRAgentPlayStep> &Steps, const FString &ScreenshotDir = TEXT(""));

	/**
	 * Presses a key and releases it HoldMs later, at least one frame apart: a tap, or a hold with a longer time.
	 * @param Key An Unreal key name: "Tab", "SpaceBar", "Enter", "Escape", "W", "Gamepad_FaceButton_Bottom".
	 * @param HoldMs Milliseconds held. Under 200 is a tap to the game.
	 */
	// The 50 here and on Click is WoRAgentPlaySchedule::DEFAULT_HOLD_MS: a UFUNCTION default must be a literal.
	UFUNCTION(meta = (AICallable), Category = "WoRAgentPlay")
	static WORLD_OF_REFRACTIONEDITOR_API UWoRAgentPlayResult *TapKey(const FString &Key, int32 HoldMs = 50);

	/**
	 * Presses a key and leaves it down until KeyUp.
	 * @param Key An Unreal key name.
	 */
	UFUNCTION(meta = (AICallable), Category = "WoRAgentPlay")
	static WORLD_OF_REFRACTIONEDITOR_API UWoRAgentPlayResult *KeyDown(const FString &Key);

	/**
	 * Releases a key.
	 * @param Key An Unreal key name.
	 */
	UFUNCTION(meta = (AICallable), Category = "WoRAgentPlay")
	static WORLD_OF_REFRACTIONEDITOR_API UWoRAgentPlayResult *KeyUp(const FString &Key);

	/**
	 * Moves the agent's mouse pointer to a point of the game picture and leaves it there (hover). The real pointer
	 * is not moved and can be anywhere.
	 * @param X Pixels from the left of the game's screenshot.
	 * @param Y Pixels from its top.
	 */
	UFUNCTION(meta = (AICallable), Category = "WoRAgentPlay")
	static WORLD_OF_REFRACTIONEDITOR_API UWoRAgentPlayResult *MoveMouse(int32 X, int32 Y);

	/**
	 * Moves the mouse to a point of the game picture and clicks there.
	 * @param X Pixels from the left of the game's screenshot.
	 * @param Y Pixels from its top.
	 * @param Button "LeftMouseButton", "RightMouseButton" or "MiddleMouseButton".
	 * @param HoldMs Milliseconds from the press to the release.
	 */
	UFUNCTION(meta = (AICallable), Category = "WoRAgentPlay")
	static WORLD_OF_REFRACTIONEDITOR_API UWoRAgentPlayResult *Click(int32 X, int32 Y, const FString &Button = TEXT("LeftMouseButton"), int32 HoldMs = 50);

	/**
	 * Moves the mouse to a point of the game picture and turns the wheel there.
	 * @param X Pixels from the left of the game's screenshot.
	 * @param Y Pixels from its top.
	 * @param Notches Wheel notches; positive scrolls up.
	 */
	UFUNCTION(meta = (AICallable), Category = "WoRAgentPlay")
	static WORLD_OF_REFRACTIONEDITOR_API UWoRAgentPlayResult *Wheel(int32 X, int32 Y, float Notches);

	/**
	 * A picture of the game alone (no window frame, no editor): the pixel frame the mouse tools take points in.
	 */
	UFUNCTION(meta = (AICallable), Category = "WoRAgentPlay")
	static WORLD_OF_REFRACTIONEDITOR_API FToolsetImage Screenshot();

	/**
	 * Resizes the PIE window so the game picture is Width x Height pixels. PIE must run in its own window
	 * (New Editor Window), not inside the level editor.
	 */
	UFUNCTION(meta = (AICallable), Category = "WoRAgentPlay")
	static WORLD_OF_REFRACTIONEDITOR_API void SetWindowSize(int32 Width, int32 Height);

	/**
	 * Runs a console command in the PIE game's world, as the first local player, without opening the console or
	 * moving keyboard focus.
	 * @param Command The command and its arguments, e.g. "wor.Party.Print".
	 * @return What the command printed and what was logged while it ran. Empty when it printed nothing.
	 */
	UFUNCTION(meta = (AICallable), Category = "WoRAgentPlay")
	static WORLD_OF_REFRACTIONEDITOR_API FString RunConsoleCommand(const FString &Command);

	/**
	 * Gives the editor back to a person: stops a playing sequence, returns the mouse to the real pointer and puts the
	 * editor's background throttle back as it was. Also happens when PIE stops. The next play tool takes it again.
	 */
	UFUNCTION(meta = (AICallable), Category = "WoRAgentPlay")
	static WORLD_OF_REFRACTIONEDITOR_API void Release();

	/**
	 * The tool's view of the game: whether PIE runs, whether the agent holds the editor, the picture and window
	 * sizes, where keyboard focus and the agent's mouse are, and whether a sequence is still playing.
	 */
	UFUNCTION(meta = (AICallable), Category = "WoRAgentPlay")
	static WORLD_OF_REFRACTIONEDITOR_API FString GetState();

	/** The PIE game; false with the reason when PIE is not running or has no window. */
	static WORLD_OF_REFRACTIONEDITOR_API bool FindGame(FWoRAgentPlayGame &OutGame, FString &OutWhy);

	/** Inputs and screenshots of the running sequence not yet sent; 0 when idle. */
	static WORLD_OF_REFRACTIONEDITOR_API int32 GetPendingEventCount();

	/** Stops the running sequence, failing its result with Why; nothing when idle. */
	static WORLD_OF_REFRACTIONEDITOR_API void CancelSequence(const FString &Why);

private:
	static UWoRAgentPlayResult *Play(const TArray<FWoRAgentPlayStep> &Steps, const FString &ScreenshotDir);
};
