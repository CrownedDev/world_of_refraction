// WeaponPowerSpec.cpp
// B-220 (D-101, analysis/BaselineNumbers.md "Decided"): a weapon's own part of an attack or ability,
// Power(copy tier) x (1 + grade x pillar curve) x requirement factor. A higher-Power weapon hits harder; a
// Body-A weapon gains more from Body than a Body-C one; under the requirement still cuts it.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/StatConstants.h"
#include "Combat/Damage/TierPowerConstants.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Weapons/WeaponPower.h"
#include "Skills/Definitions/WorldStatRequirements.h"

namespace
{
	constexpr int32 POWER_LOW = 40;
	constexpr int32 POWER_HIGH = 88;
	constexpr int32 BODY_LEVEL = 4;
	constexpr float FULL_REQUIREMENT = 1.0f;
	constexpr float HALF_REQUIREMENT = 0.5f;
	constexpr float CURVE_TOLERANCE = 0.001f;
	/** 1 - (1 - 4/7)^2 */
	constexpr float CURVE_AT_FOUR = 0.8163f;

	UWeaponData *NewWeapon(int32 Power, EScalingTier BodyGrade)
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->Power = Power;
		Weapon->Tier = EItemTier::F_Tier;
		FPillarScaling Body;
		Body.Pillar = EWorldPillar::Body;
		Body.Tier = BodyGrade;
		Weapon->StatScaling.Add(Body);
		return Weapon;
	}

	FWorldPillarLevels AtBody(int32 Body)
	{
		FWorldPillarLevels Levels;
		Levels.Body = Body;
		return Levels;
	}
}

BEGIN_DEFINE_SPEC(FWeaponPowerSpec, "WoR.Weapon.Power", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FWeaponPowerSpec)

void FWeaponPowerSpec::Define()
{
	It("curves the pillar level with a soft cap, 0 at 0 and 1 at the cap", [this]()
	{
		TestEqual(TEXT("level 0"), WoRWeaponPower::GetPillarCurve(0), 0.0f, CURVE_TOLERANCE);
		TestEqual(TEXT("level 4"), WoRWeaponPower::GetPillarCurve(BODY_LEVEL), CURVE_AT_FOUR, CURVE_TOLERANCE);
		TestEqual(TEXT("level 7"), WoRWeaponPower::GetPillarCurve(StatConstants::MAX_WORLD_STAT_LEVEL), 1.0f, CURVE_TOLERANCE);
		TestEqual(TEXT("past the cap stays 1"), WoRWeaponPower::GetPillarCurve(StatConstants::MAX_WORLD_STAT_LEVEL + 1), 1.0f,
				  CURVE_TOLERANCE);
	});

	It("gives a higher-Power weapon a bigger part", [this]()
	{
		const float Low = WoRWeaponPower::GetWeaponPart(*NewWeapon(POWER_LOW, EScalingTier::C), EItemTier::F_Tier, AtBody(BODY_LEVEL),
														FULL_REQUIREMENT);
		const float High = WoRWeaponPower::GetWeaponPart(*NewWeapon(POWER_HIGH, EScalingTier::C), EItemTier::F_Tier, AtBody(BODY_LEVEL),
														 FULL_REQUIREMENT);
		TestTrue(TEXT("higher Power hits harder"), High > Low);
	});

	It("gives a Body-A weapon more from Body than a Body-C one", [this]()
	{
		UWeaponData *GradeA = NewWeapon(POWER_LOW, EScalingTier::A);
		UWeaponData *GradeC = NewWeapon(POWER_LOW, EScalingTier::C);
		const float GainA = WoRWeaponPower::GetWeaponPart(*GradeA, EItemTier::F_Tier, AtBody(BODY_LEVEL), FULL_REQUIREMENT) -
							WoRWeaponPower::GetWeaponPart(*GradeA, EItemTier::F_Tier, AtBody(0), FULL_REQUIREMENT);
		const float GainC = WoRWeaponPower::GetWeaponPart(*GradeC, EItemTier::F_Tier, AtBody(BODY_LEVEL), FULL_REQUIREMENT) -
							WoRWeaponPower::GetWeaponPart(*GradeC, EItemTier::F_Tier, AtBody(0), FULL_REQUIREMENT);
		TestTrue(TEXT("Body adds something"), GainC > 0.0f);
		TestTrue(TEXT("A gains more than C"), GainA > GainC);
		TestEqual(TEXT("A's gain = Power x A x curve"), GainA,
				  POWER_LOW * GetScalingTierCoefficient(EScalingTier::A) * WoRWeaponPower::GetPillarCurve(BODY_LEVEL), CURVE_TOLERANCE);
	});

	It("cuts the part by the requirement factor", [this]()
	{
		UWeaponData *Weapon = NewWeapon(POWER_LOW, EScalingTier::C);
		const float Full = WoRWeaponPower::GetWeaponPart(*Weapon, EItemTier::F_Tier, AtBody(BODY_LEVEL), FULL_REQUIREMENT);
		const float Half = WoRWeaponPower::GetWeaponPart(*Weapon, EItemTier::F_Tier, AtBody(BODY_LEVEL), HALF_REQUIREMENT);
		TestEqual(TEXT("half the factor, half the part"), Half, Full * HALF_REQUIREMENT, CURVE_TOLERANCE);
	});

	It("grows Power with a levelled copy's tier power", [this]()
	{
		UWeaponData *Weapon = NewWeapon(POWER_LOW, EScalingTier::C);
		const float Expected = POWER_LOW * TierPowerScaling::GetTierPowerMultiplier(EItemTier::C_Tier) /
							   TierPowerScaling::GetTierPowerMultiplier(EItemTier::F_Tier);
		TestEqual(TEXT("C copy of an F weapon"), WoRWeaponPower::GetScaledPower(*Weapon, EItemTier::C_Tier), Expected, CURVE_TOLERANCE);
	});

	It("adds nothing for Power 0", [this]()
	{
		TestEqual(TEXT("no part"),
				  WoRWeaponPower::GetWeaponPart(*NewWeapon(0, EScalingTier::S), EItemTier::S_Tier, AtBody(BODY_LEVEL), FULL_REQUIREMENT), 0.0f);
	});
}

#endif
