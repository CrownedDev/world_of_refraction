// WeaponPower.h
// B-220 (D-101, analysis/BaselineNumbers.md "Decided"): a weapon's own damage. Weapon-delivered attacks and
// abilities add a weapon part to the skill part (the deckbuilder's flat weapon bonus):
//   WeaponPart = Power(instance tier) x (1 + sum of grade x Curve(live pillar level)) x requirement factor
// Power is authored at the asset's tier and grows with a levelled copy's tier power. The grades are the
// EScalingTier ladder on Mind / Body / Spirit; Curve is the soft cap over the run's pillar levels 0..7. The
// requirement factor (D-082, Q-162) cuts the weapon part only. Spells have no weapon part. Power 0 adds nothing.
// Debug: WoRWeaponPower::GetWeaponPartString, `wor.Weapon.Print`.

#pragma once

#include "CoreMinimal.h"
#include "Character/CharacterDataComponent.h"
#include "Inventory/ItemTier.h"
#include "Skills/Definitions/EScalingTier.h"
#include "WeaponPower.generated.h"

class UWeaponData;
struct FWorldPillarLevels;

/** One scaling grade on a weapon: its Power grows with <Pillar> at <Tier> steepness (D-101). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FPillarScaling
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scaling")
	EWorldPillar Pillar = EWorldPillar::Body;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scaling")
	EScalingTier Tier = EScalingTier::C;
};

namespace WoRWeaponPower
{
	/** The pillar soft cap: 1 - (1 - L/7)^2 over the run's levels 0..7 (Crown, BaselineNumbers.md).
	 *  TODO: B-224 moves it into the combat tuning data. */
	WORLD_OF_REFRACTION_API float GetPillarCurve(int32 Level);

	/** Power at a copy's tier: authored Power x TIER_POWER(instance) / TIER_POWER(asset tier). */
	WORLD_OF_REFRACTION_API float GetScaledPower(const UWeaponData &Weapon, EItemTier InstanceTier);

	/** 1 + each grade's coefficient x the curve of its pillar's live level. */
	WORLD_OF_REFRACTION_API float GetScalingFactor(const TArray<FPillarScaling> &Scaling, const FWorldPillarLevels &Levels);

	/** The weapon part of a weapon-delivered hit (before the damage maths). 0 for Power 0. */
	WORLD_OF_REFRACTION_API float GetWeaponPart(const UWeaponData &Weapon, EItemTier InstanceTier, const FWorldPillarLevels &Levels,
												float RequirementFactor);

	/** Power, grades and each factor, one line each. */
	WORLD_OF_REFRACTION_API FString GetWeaponPartString(const UWeaponData &Weapon, EItemTier InstanceTier,
														const FWorldPillarLevels &Levels, float RequirementFactor);
}
