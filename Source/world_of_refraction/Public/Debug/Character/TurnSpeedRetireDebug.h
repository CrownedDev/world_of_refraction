// TurnSpeedRetireDebug.h
// B-170g (ActionPips.md §3.4, Q-170h): finds the assets that still author the retired Turn Speed family, the list
// B-170h works from. Reads the same fields data validation warns on (Q-170g2): Turn Speed effect payloads
// (TurnSpeedBuff / TurnSpeedDebuff / ModifyTurnSpeed), a skill's StatScaling on Turn Speed, a character's TurnSpeed
// points and gear / evolution BaseStatBonus.BonusTurnSpeed.
// Console: wor.Data.ListTurnSpeed (loads every such asset; editor or PIE).

#pragma once

#include "CoreMinimal.h"

namespace TurnSpeedRetireDebug
{
	/** What Turn Speed this asset authors ("effect 0: TurnSpeedBuff; TurnSpeed = 5"), or empty when none (or null). */
	WORLD_OF_REFRACTION_API FString GetTurnSpeedUsesString(const UObject *Asset);

	/** One line per project asset that still authors Turn Speed, or a line saying there are none. Loads the assets. */
	WORLD_OF_REFRACTION_API FString GetTurnSpeedAssetsString();
}
