// ResistanceQuery.h
// B-216 (ResistanceRedesign.md §3.2-3.3): one resistance per damage type, and the buildup
// resistance composed from it. Until GAS stage 3 (B-042) these read the existing sources; then
// GetTypeResistance reads UWoRAttributeSet_Resistance instead, and callers don't change.
// Fractions throughout (0.15 = 15%). Debug: UResistanceQueryDebug (wor.CompareResistance).

#pragma once

#include "CoreMinimal.h"
#include "Combat/Resistance/EWoRDamageType.h"

class AActor;
struct FWoRStatLayers;

namespace WoRResistance
{
	/** One damage type's resistance by source (B-042w), so the getter and the GAS resistance sources add the
	 *  same buckets: typed buffs minus debuffs (#4), the class table (#5), gear rolls (#6). */
	struct FTypeResistanceSources
	{
		float Effect = 0.0f;
		float Class = 0.0f;
		float Gear = 0.0f;

		float Sum() const { return Effect + Class + Gear; }
	};

	/** The blanket buildup resistance by source (B-042w): ModifyStatusResist (#7), boss rising resistance (#8). */
	struct FStatusResistBonusSources
	{
		float Effect = 0.0f;
		float Boss = 0.0f;

		float Sum() const { return Effect + Boss; }
	};

	WORLD_OF_REFRACTION_API FTypeResistanceSources GetTypeResistanceSources(AActor *Target, EWoRDamageType DamageType);
	WORLD_OF_REFRACTION_API FStatusResistBonusSources GetStatusResistBonusSources(AActor *Target);

	/** GetGeneralStatusResistance's Pattern P layers (B-042w): base = the capped Spirit stat (#1), gear = the
	 *  gear points (#2), stone = the attached stones (#3), clamped to [0, RESISTANCE_MAX]; Compose() is the
	 *  getter. False without CharacterData (its 0). */
	WORLD_OF_REFRACTION_API bool GetGeneralStatusResistanceLayers(AActor *Target, FWoRStatLayers &OutLayers);

	/** Target's resistance to one damage type: typed resistance buffs minus debuffs (#4), the class
	 *  table (#5) and gear rolls (#6). None is the non-elemental bucket: buffs and debuffs keyed to
	 *  no element and no physical type (the class table and gear have no None column). Ice has no
	 *  source yet (B-064a). Uncapped; each caller applies its own cap. */
	WORLD_OF_REFRACTION_API float GetTypeResistance(AActor *Target, EWoRDamageType DamageType);

	/** Target's resistance to Element's damage (D-036): its element type's resistance, plus Ice for
	 *  Water (Q-145b). 0 for a non-elemental element (None, a leftover Generic): physical is Defense
	 *  only (Q-145). Uncapped; the damage step caps it (WoRDamageMath::GetElementMultiplier). */
	WORLD_OF_REFRACTION_API float GetElementDamageResistance(AActor *Target, ESpellElement Element);

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
