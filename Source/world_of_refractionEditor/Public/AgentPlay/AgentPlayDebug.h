// AgentPlayDebug.h
// B-573: inspection for the agent play tool.
// wor.Agent.PrintState   logs GetStateString (also the toolset's GetState tool).

#pragma once

#include "CoreMinimal.h"

namespace WoRAgentPlayDebug
{
	/** Whether PIE runs, the game picture's and the window's size, whether the window is PIE's own, what has keyboard
	 *  focus and whether it is inside the game, whether the agent holds the editor (its own pointer, no background
	 *  throttle), the mouse in picture pixels, and how much of a sequence is left. */
	WORLD_OF_REFRACTIONEDITOR_API FString GetStateString();

	WORLD_OF_REFRACTIONEDITOR_API void PrintState();
}
