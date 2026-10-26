// LuckDodge.h
// Luck dodge (D-085, Crown 2026-10-26; B-169): a hit that would land (no successful Guard or Dodge
// press) can still be dodged by luck, up to LUCK_DODGE_MAX (15%). Rolled on the battle random stream.

#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatConstants.h"

namespace LuckDodge
{
	/** Chance from normalized luck (UCharacterDataComponent::GetEquipmentModifiedLuck, 0..2): linear,
	 *  reaching the cap at full Luck investment (1.0); gear past that adds nothing.
	 *  TODO: tune the curve (Q-169). */
	inline float Chance(float LuckNorm)
	{
		return FMath::Clamp(LuckNorm * CombatConstants::LUCK_DODGE_MAX, 0.0f, CombatConstants::LUCK_DODGE_MAX);
	}

	/** Roll is a draw in [0, 1) from the battle random stream. */
	inline bool Rolls(float LuckNorm, float Roll)
	{
		return Roll < Chance(LuckNorm);
	}
}
