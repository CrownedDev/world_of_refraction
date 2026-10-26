// LuckDodgeSpec.cpp
// D-085 (Crown, 2026-10-26; B-169): a hit that would land can be dodged by luck, scaled by Luck
// up to 15%, rolled on the battle random stream so a fight replays from its seed.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatConstants.h"
#include "Combat/Defense/LuckDodge.h"
#include "Combat/Mechanics/BattleRandomSubsystem.h"

namespace
{
	constexpr int32 LUCK_DODGE_TEST_SEED = 8085;
	constexpr int32 LUCK_DODGE_TEST_ROLLS = 20000;
	/** Allowed gap between the seeded dodge rate and the chance (about 3 standard deviations). */
	constexpr float LUCK_DODGE_TEST_RATE_TOLERANCE = 0.01f;
	constexpr float LUCK_DODGE_TEST_EXPECTED_CAP = 0.15f;
	constexpr float LUCK_DODGE_TEST_NO_LUCK = 0.0f;
	constexpr float LUCK_DODGE_TEST_HALF_LUCK = 0.5f;
	constexpr float LUCK_DODGE_TEST_FULL_LUCK = 1.0f;
	constexpr float LUCK_DODGE_TEST_CURSED_LUCK = -0.5f;

	int32 LuckDodgeTestCountDodges(UWorld *World, float LuckNorm, int32 Rolls)
	{
		int32 Dodges = 0;
		for (int32 Roll = 0; Roll < Rolls; ++Roll)
		{
			Dodges += LuckDodge::Rolls(LuckNorm, UBattleRandomSubsystem::FRand(World)) ? 1 : 0;
		}
		return Dodges;
	}
}

BEGIN_DEFINE_SPEC(FLuckDodgeSpec, "WoR.Defense.LuckDodge",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UBattleRandomSubsystem *BattleRandom = nullptr;

	float SeededRate(float LuckNorm)
	{
		BattleRandom->SeedStream(LUCK_DODGE_TEST_SEED);
		return static_cast<float>(LuckDodgeTestCountDodges(TestWorld->GetWorld(), LuckNorm, LUCK_DODGE_TEST_ROLLS))
			 / LUCK_DODGE_TEST_ROLLS;
	}
END_DEFINE_SPEC(FLuckDodgeSpec)

void FLuckDodgeSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		BattleRandom = UBattleRandomSubsystem::Get(TestWorld->GetWorld());
		TestNotNull(TEXT("battle random stream"), BattleRandom);
	});

	AfterEach([this]()
	{
		BattleRandom = nullptr;
		TestWorld.Reset();
	});

	Describe("the chance", [this]()
	{
		It("is capped at 15%", [this]()
		{
			TestEqual(TEXT("cap"), CombatConstants::LUCK_DODGE_MAX, LUCK_DODGE_TEST_EXPECTED_CAP);
		});

		It("scales with Luck and reaches the cap at full Luck", [this]()
		{
			TestEqual(TEXT("no luck"), LuckDodge::Chance(LUCK_DODGE_TEST_NO_LUCK), 0.0f);
			TestEqual(TEXT("half luck"), LuckDodge::Chance(LUCK_DODGE_TEST_HALF_LUCK), LUCK_DODGE_TEST_EXPECTED_CAP * LUCK_DODGE_TEST_HALF_LUCK);
			TestEqual(TEXT("full luck"), LuckDodge::Chance(LUCK_DODGE_TEST_FULL_LUCK), LUCK_DODGE_TEST_EXPECTED_CAP);
		});

		It("never goes above 15% with gear, nor below 0 when cursed", [this]()
		{
			TestEqual(TEXT("gear ceiling"), LuckDodge::Chance(CombatConstants::LUCK_GEAR_CEILING), LUCK_DODGE_TEST_EXPECTED_CAP);
			TestEqual(TEXT("cursed"), LuckDodge::Chance(LUCK_DODGE_TEST_CURSED_LUCK), 0.0f);
		});
	});

	Describe("on a fixed seed", [this]()
	{
		It("dodges at the expected rate", [this]()
		{
			if (!BattleRandom)
			{
				return;
			}
			TestNearlyEqual(TEXT("half luck"), SeededRate(LUCK_DODGE_TEST_HALF_LUCK),
							LuckDodge::Chance(LUCK_DODGE_TEST_HALF_LUCK), LUCK_DODGE_TEST_RATE_TOLERANCE);
			TestNearlyEqual(TEXT("full luck"), SeededRate(LUCK_DODGE_TEST_FULL_LUCK),
							LUCK_DODGE_TEST_EXPECTED_CAP, LUCK_DODGE_TEST_RATE_TOLERANCE);
		});

		It("is never above 15%, and never with no Luck", [this]()
		{
			if (!BattleRandom)
			{
				return;
			}
			TestTrue(TEXT("gear ceiling stays at the cap"),
					 SeededRate(CombatConstants::LUCK_GEAR_CEILING) <= LUCK_DODGE_TEST_EXPECTED_CAP + LUCK_DODGE_TEST_RATE_TOLERANCE);
			TestEqual(TEXT("no luck never dodges"), SeededRate(LUCK_DODGE_TEST_NO_LUCK), 0.0f);
			TestEqual(TEXT("cursed never dodges"), SeededRate(LUCK_DODGE_TEST_CURSED_LUCK), 0.0f);
		});

		It("replays the same dodges from the same seed", [this]()
		{
			if (!BattleRandom)
			{
				return;
			}
			TestEqual(TEXT("same seed, same rate"), SeededRate(LUCK_DODGE_TEST_FULL_LUCK), SeededRate(LUCK_DODGE_TEST_FULL_LUCK));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
