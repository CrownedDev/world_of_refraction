// ZoneHealthOnceSpec.cpp
// B-327 (simplicity audit): ApplyEnemyStrength(S) then ApplyEnemyRankHealth(S, rank) gives S x rank x tier, S once
// (MaxHPBuff percents add, USkillEffectManager::GetTotalStatModifier). B-328 (D-120): S is the fight's HP multiplier,
// and the zone is no longer part of it (enemy HP = own x rank x Combat Tier power); the zone scales damage and guides
// which tier of enemy a zone places.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Encounter/EnemyZone.h"
#include "Trial/TrialGate.h"

namespace
{
	/** The highest zone (x1.75) and a C enemy (x2.2), so a doubled zone would be far off. */
	constexpr int32 ZONE_ONCE_TIER = EnemyZoneConstants::MAX_ZONE_TIER;
	/** MaxHP is rounded once after the multiply. */
	constexpr float ZONE_ONCE_ROUNDING = 1.0f;
}

BEGIN_DEFINE_SPEC(FZoneHealthOnceSpec, "WoR.Trial.ZoneHealthOnce",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FZoneHealthOnceSpec)

void FZoneHealthOnceSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("B-328 (D-120): the zone adds no HP, still scales damage, and asks for stronger enemies", [this]()
	{
		const int32 TopZone = EnemyZoneConstants::MAX_ZONE_TIER;
		TestEqual(TEXT("HP: zone 4 = zone 0"), TrialGate::GetFightEnemyHealthMultiplier(ETrialDifficulty::Medium, TopZone),
				  TrialGate::GetFightEnemyHealthMultiplier(ETrialDifficulty::Medium, 0), KINDA_SMALL_NUMBER);
		TestEqual(TEXT("damage: zone 4 = zone 0 x its strength"), TrialGate::GetFightEnemyDamageMultiplier(ETrialDifficulty::Medium, TopZone),
				  TrialGate::GetFightEnemyDamageMultiplier(ETrialDifficulty::Medium, 0) * AEnemyZone::GetTierStrengthMultiplier(TopZone),
				  KINDA_SMALL_NUMBER);
		for (int32 Zone = 1; Zone <= TopZone; ++Zone)
		{
			TestTrue(FString::Printf(TEXT("zone %d asks for a higher tier than zone %d"), Zone, Zone - 1),
					 AEnemyZone::GetEnemyTierGuide(Zone) > AEnemyZone::GetEnemyTierGuide(Zone - 1));
		}
	});

	It("a zone-4 C boss fight: own x rank x tier HP (no zone), each multiplier once", [this]()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->EnemyTier = EItemTier::C_Tier;
		AActor *Enemy = TestWorld->SpawnCombatant(Data, true);
		UCharacterDataComponent *CharComp = Enemy ? Enemy->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("enemy"), CharComp))
		{
			return;
		}
		const float BaseMaxHP = static_cast<float>(CharComp->MaxHP);
		const float Zone = TrialGate::GetFightEnemyHealthMultiplier(ETrialDifficulty::Medium, ZONE_ONCE_TIER); // x1 since D-120
		const float Rank = TrialGate::GetRankHealthMultiplier(EEncounterRank::Boss);
		const float Tier = TrialGate::GetEnemyTierHealthMultiplier(Enemy);

		// The order BattleGameMode uses: strength first, then rank x tier on top.
		TestTrue(TEXT("strength applied"), TrialGate::ApplyEnemyStrength(Enemy, Zone, Zone));
		TestEqual(TEXT("after strength: x the fight HP"), static_cast<float>(CharComp->MaxHP), BaseMaxHP * Zone, ZONE_ONCE_ROUNDING);
		TestTrue(TEXT("rank applied"), TrialGate::ApplyEnemyRankHealth(Enemy, Zone, EEncounterRank::Boss));
		TestEqual(TEXT("fight HP x rank x tier, once"), static_cast<float>(CharComp->MaxHP), BaseMaxHP * Zone * Rank * Tier,
				  ZONE_ONCE_ROUNDING);
		TestEqual(TEXT("no zone HP"), static_cast<float>(CharComp->MaxHP), BaseMaxHP * Rank * Tier * TrialGate::GetEnemyHealthMultiplier(ETrialDifficulty::Medium),
				  ZONE_ONCE_ROUNDING);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
