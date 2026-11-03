// LaddersByNumberSpec.cpp
// B-288: every rung of the combat ladders pinned by number, so a retune is a deliberate edit here too:
// tier-gap damage and cost (-4..+4), requirement gap (-5..+5), tier power (F..S), the crit damage gear
// ceiling x2.5 and the Damage Taken Down cap 90%. WoR.Scaling covers the shapes; this covers the values.
// The C++ ladders are pinned (a tuning asset may retune them on purpose); one wrapper call per ladder checks
// that the tuning path falls through to the C++ value while no asset exists.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/StatLayers.h"
#include "Combat/CombatConstants.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Combat/Damage/TierGapConstants.h"
#include "Combat/Damage/TierPowerConstants.h"
#include "Combat/Tuning/CombatTuningData.h"

namespace
{
	constexpr float LADDER_TOLERANCE = 0.0001f;

	constexpr int32 TIER_GAP_FIRST = -4;
	constexpr int32 TIER_GAP_LAST = 4;
	/** Gap -4 .. +4: an action through a stronger channel hits harder; out-tiering the channel costs up to half. */
	constexpr float TIER_GAP_DAMAGE_EXPECTED[] = {1.30f, 1.20f, 1.13f, 1.06f, 1.00f, 0.90f, 0.78f, 0.64f, 0.50f};
	/** Gap -4 .. +4: the opposite way, down to x0.77 and up to x2.00. */
	constexpr float TIER_GAP_COST_EXPECTED[] = {0.77f, 0.83f, 0.88f, 0.94f, 1.00f, 1.11f, 1.28f, 1.56f, 2.00f};

	constexpr int32 REQ_GAP_FIRST = -5;
	constexpr int32 REQ_GAP_LAST = 5;
	/** Gap -5 .. +5 (required - yours): +40% over-statted down to x0.32 five under. */
	constexpr float REQUIREMENT_GAP_EXPECTED[] = {1.40f, 1.30f, 1.20f, 1.13f, 1.06f, 1.00f, 0.90f, 0.78f, 0.64f, 0.50f, 0.32f};

	constexpr int32 LADDERS_TIER_COUNT = 7;
	/** F .. S: about +30% a tier. */
	constexpr float TIER_POWER_EXPECTED[LADDERS_TIER_COUNT] = {1.00f, 1.30f, 1.70f, 2.20f, 2.85f, 3.70f, 4.80f};

	constexpr float LADDERS_CRIT_GEAR_CEILING = 2.5f;
	constexpr float LADDERS_CRIT_STAT_CAP = 1.5f;
	constexpr float HUGE_GEAR_FACTOR = 10.0f;

	constexpr float DAMAGE_TAKEN_DOWN_CAP = 90.0f;
	constexpr float FACTOR_AT_CAP = 0.1f;
	constexpr float OVER_THE_CAP = 150.0f;
	constexpr float UNDER_THE_CAP = 25.0f;
	constexpr float FACTOR_UNDER_THE_CAP = 0.75f;
	constexpr float DAMAGE_TAKEN_UP = 200.0f;
	constexpr float FACTOR_FOR_UP = 3.0f;
	constexpr float NO_PERCENT = 0.0f;

	/** Gap -> index into a ladder table that starts at First. */
	int32 IndexOf(int32 Gap, int32 First)
	{
		return Gap - First;
	}
}

BEGIN_DEFINE_SPEC(FLaddersByNumberSpec, "WoR.Scaling.Ladders",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FLaddersByNumberSpec)

void FLaddersByNumberSpec::Define()
{
	It("pins the tier-gap damage ladder -4..+4: 1.30 1.20 1.13 1.06 1.00 0.90 0.78 0.64 0.50", [this]()
	{
		for (int32 Gap = TIER_GAP_FIRST; Gap <= TIER_GAP_LAST; ++Gap)
		{
			TestEqual(FString::Printf(TEXT("damage at gap %+d"), Gap), TierGapDamage::CodeTierGapDamage(Gap),
					  TIER_GAP_DAMAGE_EXPECTED[IndexOf(Gap, TIER_GAP_FIRST)], LADDER_TOLERANCE);
		}
		// Through the tier pair the game uses: an S action through an F channel is the +4 rung.
		if (!UCombatTuningData::GetActive())
		{
			TestEqual(TEXT("S through F"), TierGapDamage::GetTierGapDamageMultiplier(EItemTier::S_Tier, EItemTier::F_Tier),
					  TIER_GAP_DAMAGE_EXPECTED[IndexOf(TIER_GAP_LAST, TIER_GAP_FIRST)], LADDER_TOLERANCE);
		}
	});

	It("pins the tier-gap cost ladder -4..+4: 0.77 0.83 0.88 0.94 1.00 1.11 1.28 1.56 2.00", [this]()
	{
		for (int32 Gap = TIER_GAP_FIRST; Gap <= TIER_GAP_LAST; ++Gap)
		{
			TestEqual(FString::Printf(TEXT("cost at gap %+d"), Gap), TierGapDamage::CodeTierGapCost(Gap),
					  TIER_GAP_COST_EXPECTED[IndexOf(Gap, TIER_GAP_FIRST)], LADDER_TOLERANCE);
		}
		if (!UCombatTuningData::GetActive())
		{
			TestEqual(TEXT("S through F"), TierGapDamage::GetTierGapCostMultiplier(EItemTier::S_Tier, EItemTier::F_Tier),
					  TIER_GAP_COST_EXPECTED[IndexOf(TIER_GAP_LAST, TIER_GAP_FIRST)], LADDER_TOLERANCE);
		}
	});

	// B-326 (D-119): rungs +1..+5 are unread now (any shortfall is the flat REQUIREMENT_UNMET_DAMAGE_FACTOR); a met
	// skill still reads 0 and below (the over-requirement boost). Retire the +rungs after Crown's PIE pass.
	It("pins the requirement-gap ladder -5..+5: 1.40 1.30 1.20 1.13 1.06 1.00 0.90 0.78 0.64 0.50 0.32", [this]()
	{
		for (int32 Gap = REQ_GAP_FIRST; Gap <= REQ_GAP_LAST; ++Gap)
		{
			TestEqual(FString::Printf(TEXT("requirement at gap %+d"), Gap), TierGapDamage::CodeRequirementGap(Gap),
					  REQUIREMENT_GAP_EXPECTED[IndexOf(Gap, REQ_GAP_FIRST)], LADDER_TOLERANCE);
		}
		if (!UCombatTuningData::GetActive())
		{
			TestEqual(TEXT("five under, through the tuning path"), TierGapDamage::GetRequirementGapMultiplier(REQ_GAP_LAST),
					  REQUIREMENT_GAP_EXPECTED[IndexOf(REQ_GAP_LAST, REQ_GAP_FIRST)], LADDER_TOLERANCE);
		}
	});

	It("pins tier power F..S: 1.00 1.30 1.70 2.20 2.85 3.70 4.80", [this]()
	{
		for (int32 Index = 0; Index < LADDERS_TIER_COUNT; ++Index)
		{
			TestEqual(FString::Printf(TEXT("tier power of tier %d"), Index), TierPowerScaling::CodeTierPower(Index), TIER_POWER_EXPECTED[Index],
					  LADDER_TOLERANCE);
		}
		if (!UCombatTuningData::GetActive())
		{
			TestEqual(TEXT("S, through the tuning path"), TierPowerScaling::GetTierPowerMultiplier(EItemTier::S_Tier),
					  TIER_POWER_EXPECTED[LADDERS_TIER_COUNT - 1], LADDER_TOLERANCE);
		}
	});

	It("pins the crit damage gear ceiling at x2.5 and composes to it", [this]()
	{
		TestEqual(TEXT("CRIT_DAMAGE_GEAR_CEILING"), CombatConstants::CRIT_DAMAGE_GEAR_CEILING, LADDERS_CRIT_GEAR_CEILING, LADDER_TOLERANCE);
		FWoRStatLayers Layers = FWoRStatLayers::ForStat(ESubStat::CritDamage);
		TestEqual(TEXT("compose clamp"), Layers.Max, LADDERS_CRIT_GEAR_CEILING, LADDER_TOLERANCE);
		Layers.StatBase = LADDERS_CRIT_STAT_CAP;
		Layers.GearFactor = HUGE_GEAR_FACTOR;
		TestEqual(TEXT("a crit never exceeds x2.5 with gear"), Layers.Compose(), LADDERS_CRIT_GEAR_CEILING, LADDER_TOLERANCE);
	});

	It("caps Damage Taken Down at 90% and leaves Damage Taken Up uncapped", [this]()
	{
		TestEqual(TEXT("DAMAGE_TAKEN_DOWN_CAP_PERCENT"), CombatConstants::DAMAGE_TAKEN_DOWN_CAP_PERCENT, DAMAGE_TAKEN_DOWN_CAP, LADDER_TOLERANCE);
		TestEqual(TEXT("25% down"), UDamageCalculator::GetDamageTakenFactor(UNDER_THE_CAP, NO_PERCENT), FACTOR_UNDER_THE_CAP, LADDER_TOLERANCE);
		TestEqual(TEXT("at the cap"), UDamageCalculator::GetDamageTakenFactor(DAMAGE_TAKEN_DOWN_CAP, NO_PERCENT), FACTOR_AT_CAP, LADDER_TOLERANCE);
		TestEqual(TEXT("past the cap is the cap"), UDamageCalculator::GetDamageTakenFactor(OVER_THE_CAP, NO_PERCENT), FACTOR_AT_CAP, LADDER_TOLERANCE);
		TestEqual(TEXT("200% up"), UDamageCalculator::GetDamageTakenFactor(NO_PERCENT, DAMAGE_TAKEN_UP), FACTOR_FOR_UP, LADDER_TOLERANCE);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
