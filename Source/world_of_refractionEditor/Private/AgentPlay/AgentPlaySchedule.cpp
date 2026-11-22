// AgentPlaySchedule.cpp

#include "AgentPlay/AgentPlaySchedule.h"

namespace
{
	bool IsMouseAction(EWoRAgentPlayAction Action)
	{
		return Action == EWoRAgentPlayAction::MouseMove || Action == EWoRAgentPlayAction::MouseDown || Action == EWoRAgentPlayAction::MouseUp ||
			   Action == EWoRAgentPlayAction::MouseClick || Action == EWoRAgentPlayAction::MouseWheel;
	}

	bool IsPress(EWoRAgentPlayAction Action)
	{
		return Action == EWoRAgentPlayAction::KeyDown || Action == EWoRAgentPlayAction::MouseDown;
	}

	bool IsRelease(EWoRAgentPlayAction Action)
	{
		return Action == EWoRAgentPlayAction::KeyUp || Action == EWoRAgentPlayAction::MouseUp;
	}

	bool NeedsKey(EWoRAgentPlayAction Action)
	{
		return Action == EWoRAgentPlayAction::KeyDown || Action == EWoRAgentPlayAction::KeyUp || Action == EWoRAgentPlayAction::KeyTap;
	}
}

FKey WoRAgentPlaySchedule::ResolveKey(const FWoRAgentPlayStep &Step)
{
	if (Step.Key.IsEmpty() && IsMouseAction(Step.Action))
	{
		return EKeys::LeftMouseButton;
	}
	return FKey(*Step.Key);
}

FString WoRAgentPlaySchedule::Validate(const TArray<FWoRAgentPlayStep> &Steps, FIntPoint ViewportSize)
{
	if (Steps.IsEmpty())
	{
		return TEXT("no steps");
	}
	for (int32 Index = 0; Index < Steps.Num(); ++Index)
	{
		const FWoRAgentPlayStep &Step = Steps[Index];
		if (Step.AtMs < 0 || Step.HoldMs < 0 || Step.AtMs + Step.HoldMs > MAX_SEQUENCE_MS)
		{
			return FString::Printf(TEXT("step %d: its time must be between 0 and %d ms"), Index, MAX_SEQUENCE_MS);
		}
		const bool bMouse = IsMouseAction(Step.Action);
		if (NeedsKey(Step.Action) || (bMouse && !Step.Key.IsEmpty()))
		{
			const FKey Key = ResolveKey(Step);
			if (!Key.IsValid() || Key.IsMouseButton() != bMouse)
			{
				return FString::Printf(TEXT("step %d: '%s' is not %s"), Index, *Step.Key, bMouse ? TEXT("a mouse button") : TEXT("a key name (e.g. Tab, SpaceBar, Enter, W)"));
			}
		}
		if (bMouse && ViewportSize != FIntPoint::ZeroValue && (Step.X < 0 || Step.Y < 0 || Step.X >= ViewportSize.X || Step.Y >= ViewportSize.Y))
		{
			return FString::Printf(TEXT("step %d: the point %d,%d is outside the game's %d x %d picture"), Index, Step.X, Step.Y, ViewportSize.X, ViewportSize.Y);
		}
	}
	return FString();
}

TArray<FWoRAgentPlayEvent> WoRAgentPlaySchedule::BuildEvents(const TArray<FWoRAgentPlayStep> &Steps)
{
	TArray<FWoRAgentPlayEvent> Events;
	for (int32 Index = 0; Index < Steps.Num(); ++Index)
	{
		const FWoRAgentPlayStep &Step = Steps[Index];
		const FKey Key = ResolveKey(Step);
		const auto Add = [&Events, &Step, Index, &Key](EWoRAgentPlayAction Action, int32 AfterMs = 0)
		{
			Events.Add(FWoRAgentPlayEvent{Step.AtMs + AfterMs, Index, Action, Key});
		};
		if (IsMouseAction(Step.Action) && Step.Action != EWoRAgentPlayAction::MouseMove)
		{
			Add(EWoRAgentPlayAction::MouseMove); // the pointer is at the point before the button or the wheel acts
		}
		switch (Step.Action)
		{
		case EWoRAgentPlayAction::KeyTap:
			Add(EWoRAgentPlayAction::KeyDown);
			Add(EWoRAgentPlayAction::KeyUp, Step.HoldMs);
			break;
		case EWoRAgentPlayAction::MouseClick:
			Add(EWoRAgentPlayAction::MouseDown);
			Add(EWoRAgentPlayAction::MouseUp, Step.HoldMs);
			break;
		default:
			Add(Step.Action);
			break;
		}
	}
	Events.StableSort([](const FWoRAgentPlayEvent &A, const FWoRAgentPlayEvent &B) { return A.AtMs < B.AtMs; });
	return Events;
}

int32 WoRAgentPlaySchedule::CountDue(const TArray<FWoRAgentPlayEvent> &Events, int32 First, int32 ElapsedMs)
{
	TArray<FKey, TInlineAllocator<4>> PressedThisFrame;
	int32 Index = FMath::Max(First, 0);
	for (; Index < Events.Num() && Events[Index].AtMs <= ElapsedMs; ++Index)
	{
		const FWoRAgentPlayEvent &Event = Events[Index];
		if (IsRelease(Event.Action) && PressedThisFrame.Contains(Event.Key))
		{
			break;
		}
		if (IsPress(Event.Action))
		{
			PressedThisFrame.Add(Event.Key);
		}
	}
	return Index - FMath::Max(First, 0);
}
