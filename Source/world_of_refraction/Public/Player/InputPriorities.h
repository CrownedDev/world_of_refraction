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
}
