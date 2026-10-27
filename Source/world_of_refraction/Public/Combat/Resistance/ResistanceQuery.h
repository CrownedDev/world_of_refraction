// ResistanceQuery.h
// B-216 (ResistanceRedesign.md §3.2-3.3): one resistance per damage type, and the buildup
// resistance composed from it. Until GAS stage 3 (B-042) these read the existing sources; then
// GetTypeResistance reads UWoRAttributeSet_Resistance instead, and callers don't change.
// Fractions throughout (0.15 = 15%). Debug: UResistanceQueryDebug (wor.CompareResistance).

#pragma once

#include "CoreMinimal.h"
#include "Combat/Resistance/EWoRDamageType.h"

class AActor;

namespace WoRResistance
{
	/** Target's resistance to one damage type: typed resistance buffs minus debuffs (#4), the class
	 *  table (#5) and gear rolls (#6). None is the non-elemental bucket: buffs and debuffs keyed to
	 *  no element and no physical type (the class table and gear have no None column). Ice has no
	 *  source yet (B-064a). Uncapped; each caller applies its own cap. */
	WORLD_OF_REFRACTION_API float GetTypeResistance(AActor *Target, EWoRDamageType DamageType);

	/** What a hit's two status axes resist: its element's type (or the non-elemental bucket) plus
	 *  its physical subtype's, when it has one. The Matched layer of buildup without #7 and #8. */
	WORLD_OF_REFRACTION_API float GetHitTypeResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType PhysicalType);

	/** General buildup resistance (#1-3): the capped Spirit stat, times gear points, times attached
	 *  stones, clamped to [0, RESISTANCE_MAX]. 0 without CharacterData. */
	WORLD_OF_REFRACTION_API float GetGeneralStatusResistance(AActor *Target);

	/** Signed blanket buildup resistance (#7 ModifyStatusResist, #8 boss rising resistance). */
	WORLD_OF_REFRACTION_API float GetStatusResistBonus(AActor *Target);

	/** The value AddStatusBuildup applies as Amount *= (1 - this):
	 *  clamp(General + HitType + Bonus, RESISTANCE_MIN, RESISTANCE_MAX). 0 without CharacterData. */
	WORLD_OF_REFRACTION_API float GetTotalStatusResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType PhysicalType);
}
