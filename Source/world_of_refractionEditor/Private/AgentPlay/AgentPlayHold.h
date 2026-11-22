// AgentPlayHold.h
// B-573: the agent's hold on this editor while it plays, so a person can use the PC meanwhile.
//   - The mouse Slate sees is the agent's own (a pointer that exists only inside this editor): the real pointer is
//     never moved, and moving it changes nothing in the game or the editor's hover. Real clicks still land where
//     they are made.
//   - The editor's "use less CPU in the background" throttle is off (in memory only, never saved).
//   - Slate handles pointer capture while this editor is in the background, so a pressed button still holds the
//     agent's pointer until the release and clicks (without it, a UButton never fires).
// Taken by the first tool that plays; given back by Release, when PIE ends, or when the module unloads.

#pragma once

#include "CoreMinimal.h"

namespace WoRAgentPlayHold
{
	/** Takes the hold; nothing when already taken. */
	void Take();

	/** Gives everything back as it was; nothing when not taken. */
	void Release();

	bool IsTaken();

	/** The agent's pointer has not been put on the game yet (or the hold is not taken): it is over nothing. */
	bool IsPointerParked();
}
