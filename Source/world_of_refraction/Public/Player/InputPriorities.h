// InputPriorities.h
// B-129: every Enhanced Input mapping-context priority the C++ adds, in one place (higher wins a shared key).
// Each context and its priority has its row in docs/Design/InputMap.md ("Mapping contexts").

#pragma once

#include "CoreMinimal.h"

namespace WoRInputPriority
{
	/** The hub / trial map's exploration context (AExplorationPlayerController; designer-overridable per BP). */
	constexpr int32 EXPLORATION = 0;

	/** The editor-authored combat context (Guard, Dodge; ACombatPlayerController::CombatMappingContext). */
	constexpr int32 COMBAT = 0;

	/** The code-built combat menu keys (B-093, B-391, B-406b): above COMBAT, so a key both map goes to the menu. */
	constexpr int32 COMBAT_MENU_KEYS = COMBAT + 1;

	static_assert(COMBAT_MENU_KEYS > COMBAT, "the combat menu keys must win a key the combat context also maps");

	/** B-459c: the host's hold-to-answer key (View / Tab), only while a join request waits. It doesn't eat the key, so
	 *  a tap still reaches the rest; above the others so the hold is seen first. */
	constexpr int32 JOIN_REQUEST_CARD = COMBAT_MENU_KEYS + 1;
}
