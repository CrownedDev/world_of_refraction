// MenuFeel.h
// UI_Theme.md 5a (Crown, 2026-11-20): menus must answer at once. The timings every menu shares, and the small pieces
// that keep them: our own hold-to-repeat for a held direction (the system's key repeat differs per PC), and the ease
// a highlight, a press and a scroll move with. Headless, so the specs read them. First used by the merged Inventory /
// Loadout screen (ULoadoutV2ScreenWidget).

#pragma once

#include "CoreMinimal.h"

namespace WoRMenuFeel
{
	/** A held direction: how long until the first repeat, and between the ones after. */
	constexpr float HOLD_REPEAT_FIRST_SECONDS = 0.35f;
	constexpr float HOLD_REPEAT_NEXT_SECONDS = 0.09f;
	/** The highlight fading to the new item (80-120 ms). */
	constexpr float HIGHLIGHT_FADE_SECONDS = 0.09f;
	/** A pressed item: darker and a little lower for this long. */
	constexpr float PRESS_SECONDS = 0.08f;
	constexpr float PRESS_OFFSET = 2.0f;
	constexpr float PRESS_DARKEN = 0.7f;
	/** A list or grid gliding to its lit item (100-150 ms). */
	constexpr float SCROLL_GLIDE_SECONDS = 0.12f;
	/** The Move cue (a glass clink) starts this far in, past its lead-in. */
	constexpr float MOVE_SOUND_START_SECONDS = 0.125f;

	/** A decelerating ease (never linear, UI_Theme 5a): fast at first, settling at the end. Alpha and the result are 0 to 1. */
	inline float EaseOut(float Alpha)
	{
		const float Left = 1.0f - FMath::Clamp(Alpha, 0.0f, 1.0f);
		return 1.0f - Left * Left * Left;
	}
}

/** Our own hold-to-repeat. Start it when a direction key goes down (that press acts at once, by itself); feed Advance
 *  the time of each frame while the key stays down: it returns how many repeats are due now. */
struct FWoRHoldRepeat
{
	float HeldSeconds = 0.0f;
	int32 Fired = 0;

	void Start()
	{
		HeldSeconds = 0.0f;
		Fired = 0;
	}

	int32 Advance(float DeltaSeconds)
	{
		HeldSeconds += FMath::Max(DeltaSeconds, 0.0f);
		const int32 Due = HeldSeconds < WoRMenuFeel::HOLD_REPEAT_FIRST_SECONDS
							  ? 0
							  : 1 + FMath::FloorToInt32((HeldSeconds - WoRMenuFeel::HOLD_REPEAT_FIRST_SECONDS) / WoRMenuFeel::HOLD_REPEAT_NEXT_SECONDS);
		const int32 New = Due - Fired;
		Fired = Due;
		return New;
	}
};
