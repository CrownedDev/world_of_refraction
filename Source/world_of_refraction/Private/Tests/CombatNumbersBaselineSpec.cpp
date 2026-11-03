// CombatNumbersBaselineSpec.cpp
// B-224 (Crown, analysis/BaselineNumbers.md "Decided"): every number of the combat baseline pinned: the pillar
// soft cap 0..7, reference weapon Power and main-pillar requirement by tier, enemy HP by rank, the weapon
// requirement floor 0.5 (weapon part only), the Defense ceiling 75% and the un-invested crit x1.25; the tuning
// data starts at these values and fills its ladder rows with them; Elite is a fleeable rank below Mini-boss.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/StatLayers.h"
#include "Combat/CombatConstants.h"
#include "Combat/Encounter/EEncounterRank.h"
#include "Combat/FleeRules.h"
#include "Combat/Tuning/CombatBaselineConstants.h"
#include "Combat/Tuning/CombatTuningData.h"
#include "Engine/CurveTable.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Weapons/WeaponPower.h"
#include "Skills/Definitions/WorldStatRequirements.h"
#include "Trial/TrialGate.h"

namespace
{
	constexpr float BASELINE_TOLERANCE = 0.001f;

	/** 1 - (1 - L/7)^2 at L = 0..7. */
	constexpr float PILLAR_CURVE_EXPECTED[] = {0.0f, 0.2653f, 0.4898f, 0.6735f, 0.8163f, 0.9184f, 0.9796f, 1.0f};
	constexpr float REFERENCE_POWER_EXPECTED[] = {40.0f, 52.0f, 68.0f, 88.0f, 114.0f, 148.0f, 192.0f};
	constexpr int32 REQUIREMENT_EXPECTED[] = {0, 1, 2, 3, 5, 6, 7};
	constexpr EItemTier TIERS[] = {EItemTier::F_Tier, EItemTier::E_Tier, EItemTier::D_Tier, EItemTier::C_Tier,
								   EItemTier::B_Tier, EItemTier::A_Tier, EItemTier::S_Tier};
	constexpr int32 TIER_COUNT = 7;

	constexpr float RANK_NORMAL = 1.0f;
	constexpr float RANK_ELITE = 1.5f;
	constexpr float RANK_MINIBOSS = 3.0f;
	constexpr float RANK_BOSS = 6.0f;

	constexpr float REQUIREMENT_FLOOR = 0.5f;
	/** The gap whose ladder rung (0.50) first meets the floor, and the one below it (0.32). */
	constexpr int32 GAP_AT_FLOOR = 4;
	constexpr int32 GAP_UNDER_FLOOR = 5;
	constexpr int32 GAP_ABOVE_FLOOR = 3;
	constexpr float LADDER_AT_GAP_THREE = 0.64f;

	constexpr float DEFENSE_CEILING = 0.75f;
	constexpr float DEFENSE_STAT_ALONE = 0.5f;
	constexpr float HUGE_DEFENSE_FACTOR = 10.0f;
	constexpr float CRIT_BASE = 1.25f;
	constexpr float CRIT_STAT_CAP = 1.5f;
	constexpr float CRIT_GEAR_CEILING = 2.5f;

	FWorldStatRequirements BodyRequirementOf(int32 Level)
	{
		FWorldStatRequirements Requirements;
		Requirements.RequiredWorldBody = Level;
		return Requirements;
	}
}

BEGIN_DEFINE_SPEC(FCombatNumbersBaselineSpec, "WoR.Tuning.Baseline",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FCombatNumbersBaselineSpec)

void FCombatNumbersBaselineSpec::Define()
{
	It("curves a pillar level 0..7 as a soft cap: 0, 0.27, 0.49, 0.67, 0.82, 0.92, 0.98, 1", [this]()
	{
		for (int32 Level = 0; Level <= CombatBaseline::PILLAR_CURVE_MAX_LEVEL; ++Level)
		{
			TestEqual(FString::Printf(TEXT("curve(%d)"), Level), WoRWeaponPower::GetPillarCurve(Level), PILLAR_CURVE_EXPECTED[Level],
					  BASELINE_TOLERANCE);
		}
		TestEqual(TEXT("the cap is MAX_WORLD_STAT_LEVEL"), CombatBaseline::PILLAR_CURVE_MAX_LEVEL, StatConstants::MAX_WORLD_STAT_LEVEL);
	});

	It("gives a weapon of each tier its reference Power: F 40, E 52, D 68, C 88, B 114, A 148, S 192", [this]()
	{
		for (int32 Index = 0; Index < TIER_COUNT; ++Index)
		{
			TestEqual(FString::Printf(TEXT("Power of tier %d"), Index), WoRWeaponPower::GetReferencePower(TIERS[Index]),
					  REFERENCE_POWER_EXPECTED[Index], BASELINE_TOLERANCE);
		}
	});

	It("asks a weapon of each tier for a main pillar of F 0, E 1, D 2, C 3, B 5, A 6, S 7", [this]()
	{
		for (int32 Index = 0; Index < TIER_COUNT; ++Index)
		{
			TestEqual(FString::Printf(TEXT("requirement of tier %d"), Index), WoRWeaponPower::GetReferenceRequirement(TIERS[Index]),
					  REQUIREMENT_EXPECTED[Index]);
		}
	});

	It("multiplies enemy HP by rank: Normal 1, Elite 1.5, Mini-boss 3, Boss 6", [this]()
	{
		TestEqual(TEXT("Normal"), TrialGate::GetRankHealthMultiplier(EEncounterRank::Normal), RANK_NORMAL, BASELINE_TOLERANCE);
		TestEqual(TEXT("Elite"), TrialGate::GetRankHealthMultiplier(EEncounterRank::Elite), RANK_ELITE, BASELINE_TOLERANCE);
		TestEqual(TEXT("Mini-boss"), TrialGate::GetRankHealthMultiplier(EEncounterRank::MiniBoss), RANK_MINIBOSS, BASELINE_TOLERANCE);
		TestEqual(TEXT("Boss"), TrialGate::GetRankHealthMultiplier(EEncounterRank::Boss), RANK_BOSS, BASELINE_TOLERANCE);
	});

	It("orders Elite between Normal and Mini-boss and lets a party flee it", [this]()
	{
		TestTrue(TEXT("Elite over Normal"), HarderEncounterRank(EEncounterRank::Normal, EEncounterRank::Elite) == EEncounterRank::Elite);
		TestTrue(TEXT("Mini-boss over Elite"), HarderEncounterRank(EEncounterRank::Elite, EEncounterRank::MiniBoss) == EEncounterRank::MiniBoss);
		TestTrue(TEXT("Boss over Elite, either order"), HarderEncounterRank(EEncounterRank::Boss, EEncounterRank::Elite) == EEncounterRank::Boss);
		TestTrue(TEXT("Elite can be fled"), FleeRules::CanFleeRank(EEncounterRank::Elite));
		TestFalse(TEXT("Mini-boss still can't"), FleeRules::CanFleeRank(EEncounterRank::MiniBoss));
	});

	It("floors the weapon requirement factor at 0.5 from four levels under, and only the weapon part reads it", [this]()
	{
		TestEqual(TEXT("C++ floor"), WeaponRequirement::WEAPON_REQ_FACTOR_MIN, REQUIREMENT_FLOOR, BASELINE_TOLERANCE);
		TestEqual(TEXT("tuning default"), WoRTuning::Get().WeaponReqFactorMin, REQUIREMENT_FLOOR, BASELINE_TOLERANCE);
		const FWorldPillarLevels NoLevels;
		TestEqual(TEXT("three under: the ladder"), BodyRequirementOf(GAP_ABOVE_FLOOR).GetWeaponRequirementFactor(NoLevels), LADDER_AT_GAP_THREE,
				  BASELINE_TOLERANCE);
		TestEqual(TEXT("four under: the floor"), BodyRequirementOf(GAP_AT_FLOOR).GetWeaponRequirementFactor(NoLevels), REQUIREMENT_FLOOR,
				  BASELINE_TOLERANCE);
		TestEqual(TEXT("five under: still the floor"), BodyRequirementOf(GAP_UNDER_FLOOR).GetWeaponRequirementFactor(NoLevels), REQUIREMENT_FLOOR,
				  BASELINE_TOLERANCE);
		// The weapon part takes the factor as its own multiplier (WoRWeaponPower::GetWeaponPart); the skill part has no such input.
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->Power = CombatBaseline::WEAPON_REFERENCE_POWER_F;
		Weapon->Tier = EItemTier::F_Tier;
		TestEqual(TEXT("the weapon part halves"), WoRWeaponPower::GetWeaponPart(*Weapon, EItemTier::F_Tier, NoLevels, REQUIREMENT_FLOOR),
				  CombatBaseline::WEAPON_REFERENCE_POWER_F * REQUIREMENT_FLOOR, BASELINE_TOLERANCE);
	});

	It("caps composed Defense at 75%", [this]()
	{
		TestEqual(TEXT("constant"), CombatConstants::DEFENSE_REDUCTION_CEILING, DEFENSE_CEILING, BASELINE_TOLERANCE);
		FWoRStatLayers Layers = FWoRStatLayers::ForStat(ESubStat::Defense);
		TestEqual(TEXT("compose clamp"), Layers.Max, DEFENSE_CEILING, BASELINE_TOLERANCE);
		Layers.StatBase = DEFENSE_STAT_ALONE;
		Layers.GearFactor = HUGE_DEFENSE_FACTOR;
		TestEqual(TEXT("a tank takes a quarter of a hit"), Layers.Compose(), DEFENSE_CEILING, BASELINE_TOLERANCE);
	});

	It("makes an un-invested crit x1.25, the stat reaching x1.5 at full investment, gear x2.5", [this]()
	{
		TestEqual(TEXT("base"), CombatConstants::CRIT_DMG_BASE, CRIT_BASE, BASELINE_TOLERANCE);
		TestEqual(TEXT("stat cap"), CombatConstants::CRIT_DAMAGE_STAT_CAP, CRIT_STAT_CAP, BASELINE_TOLERANCE);
		TestEqual(TEXT("gear ceiling"), CombatConstants::CRIT_DAMAGE_GEAR_CEILING, CRIT_GEAR_CEILING, BASELINE_TOLERANCE);
		const float AtFullInvestment = CombatConstants::CRIT_DMG_BASE + CombatConstants::STAT_DERIVE_DENOM * CombatConstants::CRIT_DAMAGE_PER_POINT;
		TestEqual(TEXT("full investment lands on the stat cap"), AtFullInvestment, CRIT_STAT_CAP, BASELINE_TOLERANCE);
		const FWoRStatLayers Layers = FWoRStatLayers::ForStat(ESubStat::CritDamage);
		TestEqual(TEXT("a crit never drops below x1.25"), Layers.Min, CRIT_BASE, BASELINE_TOLERANCE);
		TestEqual(TEXT("compose ceiling"), Layers.Max, CRIT_GEAR_CEILING, BASELINE_TOLERANCE);
	});

	It("starts the tuning data at the baseline and fills its ladder rows with it", [this]()
	{
		UCombatTuningData *Tuning = NewObject<UCombatTuningData>(GetTransientPackage());
		UCurveTable *Table = NewObject<UCurveTable>(GetTransientPackage());
		TestTrue(TEXT("no differences"), Tuning->GetDifferencesFromCode().IsEmpty());
		if (!TestTrue(TEXT("filled"), Tuning->FillLadderRows(Table)))
		{
			return;
		}
		const ECombatLadder BaselineLadders[] = {ECombatLadder::PillarCurve, ECombatLadder::WeaponReferencePower,
												 ECombatLadder::WeaponRequirementByTier, ECombatLadder::EnemyRankHealth,
												 ECombatLadder::LevelDropChance, ECombatLadder::MiniBossLevels};
		for (const ECombatLadder Ladder : BaselineLadders)
		{
			int32 First, Last;
			UCombatTuningData::GetLadderKeyRange(Ladder, First, Last);
			for (int32 Key = First; Key <= Last; ++Key)
			{
				const float Code = UCombatTuningData::GetCodeLadderValue(Ladder, Key);
				// A wrong CodeValue would show a curve miss: the row must answer on its own.
				TestEqual(FString::Printf(TEXT("ladder %d key %d from the row"), static_cast<int32>(Ladder), Key),
						  Tuning->GetLadder(Ladder, Key, Code + 1.0f), Code, BASELINE_TOLERANCE);
			}
		}
		TestEqual(TEXT("the pillar curve row ends at 7"), Tuning->GetLadder(ECombatLadder::PillarCurve, CombatBaseline::PILLAR_CURVE_MAX_LEVEL, 0.0f),
				  1.0f, BASELINE_TOLERANCE);
		UCurveTable::InvalidateAllCachedCurves();
	});

	It("prints the whole baseline in one string", [this]()
	{
		const FString Text = WoRTuning::Get().GetCombatTuningString();
		TestTrue(TEXT("pillar curve"), Text.Contains(TEXT("PillarCurve")));
		TestTrue(TEXT("rank HP"), Text.Contains(TEXT("EnemyRankHealth")));
		TestTrue(TEXT("requirement floor"), Text.Contains(TEXT("Weapon req floor")));
		TestTrue(TEXT("C++ caps"), Text.Contains(TEXT("defense ceiling")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
