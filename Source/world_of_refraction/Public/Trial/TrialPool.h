// TrialPool.h
// B-566 (D-225, TrialPoolDesign §2 and §9 stage 2, the pool): what a trial can give, zone by zone.
//   - The pool tag: the trial's own (UTrialData::Pool); a zone's LootPool still overrides it for that zone.
//   - The items: TrialLoot::GatherPool on that tag (weapons, adapters, skills, prebuilt finds, apparel, evolutions).
//   - The crystals: every element's gem in every trial (Q-566), leaning to the trial's element (more often, and likelier at a
//     higher tier). Crystals are not assets, so they are not tagged: the pool holds them all.
//   - The tiers: each zone opens one more (zone 0 F to E ... zone 4 F to A).
// Stage 4 (the roll) and stage 5 (drops) pick from here. Debug: wor.Trial.Pool with no tag prints the active trial's
// pool zone by zone (GetTrialPoolString).

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "GameplayTagContainer.h"

class UTrialData;
class UWorld;
struct FEncounterZoneInfo;
enum class ESpellElement : uint8;

/** The tiers a zone holds, both ends included. */
struct FTrialTierRange
{
	EItemTier Min;
	EItemTier Max;
	bool Contains(EItemTier Tier) const { return Tier >= Min && Tier <= Max; }
};

namespace TrialPool
{
	// TODO: tunables (TrialPoolDesign §2: "the two weights are named tunables").
	/** A crystal of the trial's own element is this many times likelier than another element's. */
	inline constexpr float OWN_ELEMENT_WEIGHT = 3.0f;
	inline constexpr float OTHER_ELEMENT_WEIGHT = 1.0f;
	/** Own element: each tier above the zone's lowest weighs this much more (other elements: every tier alike). */
	inline constexpr float OWN_ELEMENT_TIER_STEP = 1.0f;
	/** Zone 0 holds F and the one above; each zone opens one more. */
	inline constexpr int32 ZONE_TIER_SPAN = 1;

	/** Zone 0 F to E, 1 F to D, 2 F to C, 3 F to B, 4 F to A (clamped to 0..MAX_ZONE_TIER). */
	WORLD_OF_REFRACTION_API FTrialTierRange GetZoneTierRange(int32 ZoneTier);
	/** The zone's own LootPool when set, else the trial's Pool; empty with neither. */
	WORLD_OF_REFRACTION_API FGameplayTag GetPoolTag(const UTrialData *Trial, const FEncounterZoneInfo &Zone);

	/** Every element's gem (Q-566: the nine elements; Quartz out), in ECrystalType order. */
	WORLD_OF_REFRACTION_API const TArray<ECrystalType> &GetPoolCrystals();
	/** OWN_ELEMENT_WEIGHT for the trial's element's gem, else OTHER_ELEMENT_WEIGHT (a Generic trial: no lean). */
	WORLD_OF_REFRACTION_API float GetCrystalWeight(ECrystalType Type, ESpellElement TrialElement);
	/** 1 for any tier in Range, plus OWN_ELEMENT_TIER_STEP per tier above Range.Min for the trial's own element; 0
	 *  outside Range. */
	WORLD_OF_REFRACTION_API float GetCrystalTierWeight(EItemTier Tier, const FTrialTierRange &Range, bool bOwnElement);
	/** One crystal, element then tier, by the weights above. */
	WORLD_OF_REFRACTION_API FCrystalId PickCrystal(ESpellElement TrialElement, const FTrialTierRange &Range, FRandomStream &Roll);

	/** The trial's pool: its tag and items, the crystal weights, and each zone in World with its pool and tiers. */
	WORLD_OF_REFRACTION_API FString GetTrialPoolString(const UTrialData *Trial, const UWorld *World);
}
