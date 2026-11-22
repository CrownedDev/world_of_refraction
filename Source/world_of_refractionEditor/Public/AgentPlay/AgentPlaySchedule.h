// AgentPlaySchedule.h
// B-573: the pure part of the agent play tool. A sequence's steps become timed events (a tap or a click is a press
// and a later release; a mouse step moves the pointer first), and each frame takes the events that are due.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

#include "AgentPlaySchedule.generated.h"

namespace WoRAgentPlaySchedule
{
	/** A tap's hold: a few frames, well under Enhanced Input's 0.2 s Tap limit. */
	constexpr int32 DEFAULT_HOLD_MS = 50;
	/** The longest a sequence may run. */
	constexpr int32 MAX_SEQUENCE_MS = 120000;
}

UENUM(BlueprintType)
enum class EWoRAgentPlayAction : uint8
{
	KeyDown,
	KeyUp,
	/** KeyDown, then KeyUp HoldMs later. */
	KeyTap,
	MouseMove,
	MouseDown,
	MouseUp,
	/** MouseDown, then MouseUp HoldMs later. */
	MouseClick,
	MouseWheel,
	Screenshot,
};

/** One step of a timed input sequence in the PIE game window. */
USTRUCT(BlueprintType)
struct FWoRAgentPlayStep
{
	GENERATED_BODY()

	/** Milliseconds after the sequence's first frame. */
	UPROPERTY(BlueprintReadWrite, Category = "WoRAgentPlay", meta = (ClampMin = "0"))
	int32 AtMs = 0;

	UPROPERTY(BlueprintReadWrite, Category = "WoRAgentPlay")
	EWoRAgentPlayAction Action = EWoRAgentPlayAction::KeyTap;

	/** An Unreal key name: "Tab", "SpaceBar", "Enter", "W", "Gamepad_FaceButton_Bottom". On a mouse button step:
	 *  "LeftMouseButton" (also when empty), "RightMouseButton", "MiddleMouseButton". */
	UPROPERTY(BlueprintReadWrite, Category = "WoRAgentPlay")
	FString Key;

	/** KeyTap and MouseClick: milliseconds from the press to the release. The release is never in the press's frame. */
	UPROPERTY(BlueprintReadWrite, Category = "WoRAgentPlay", meta = (ClampMin = "0"))
	int32 HoldMs = WoRAgentPlaySchedule::DEFAULT_HOLD_MS;

	/** Mouse steps: the point, in pixels of the game's screenshot (0,0 is its top left). */
	UPROPERTY(BlueprintReadWrite, Category = "WoRAgentPlay")
	int32 X = 0;

	UPROPERTY(BlueprintReadWrite, Category = "WoRAgentPlay")
	int32 Y = 0;

	/** MouseWheel: notches; positive scrolls up. */
	UPROPERTY(BlueprintReadWrite, Category = "WoRAgentPlay")
	float Wheel = 0.0f;
};

/** One input sent in one frame. Action is never KeyTap or MouseClick here. */
struct FWoRAgentPlayEvent
{
	int32 AtMs = 0;
	int32 StepIndex = INDEX_NONE;
	EWoRAgentPlayAction Action = EWoRAgentPlayAction::KeyDown;
	FKey Key;
};

namespace WoRAgentPlaySchedule
{
	/** The step's key; a mouse step with no key is the left button. */
	WORLD_OF_REFRACTIONEDITOR_API FKey ResolveKey(const FWoRAgentPlayStep &Step);

	/** Why the steps can't be played; empty when they can. A zero ViewportSize skips the point check. */
	WORLD_OF_REFRACTIONEDITOR_API FString Validate(const TArray<FWoRAgentPlayStep> &Steps, FIntPoint ViewportSize);

	/** The steps as events in time order; steps at the same time keep their order. */
	WORLD_OF_REFRACTIONEDITOR_API TArray<FWoRAgentPlayEvent> BuildEvents(const TArray<FWoRAgentPlayStep> &Steps);

	/** How many events from First are sent this frame: those due at ElapsedMs, stopping before a release whose key
	 *  was pressed in this same frame (the game must see the key down for one frame at least). */
	WORLD_OF_REFRACTIONEDITOR_API int32 CountDue(const TArray<FWoRAgentPlayEvent> &Events, int32 First, int32 ElapsedMs);
}
