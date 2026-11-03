// ZoneHealthOnceSpec.cpp
// B-327 (simplicity audit): an enemy's max HP takes the zone's strength once. ABattleGameMode and the balance sim
// call ApplyEnemyStrength(zone HP) and then ApplyEnemyRankHealth(zone HP, rank). The second effect is
// zone x (rank x tier - 1), and MaxHPBuff percents add (USkillEffectManager::GetTotalStatModifier), so the total is
// zone x rank x tier, not zone squared. This pins that product so a later change can't stack the zone twice.

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

	It("gives a zone-4 C boss base x rank x tier HP: the zone changes no stats (D-120)", [this]()
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
		const float Zone = AEnemyZone::GetTierStrengthMultiplier(ZONE_ONCE_TIER);
		const float Rank = TrialGate::GetRankHealthMultiplier(EEncounterRank::Boss);
		const float Tier = TrialGate::GetEnemyTierHealthMultiplier(Enemy);
		TestTrue(TEXT("the zone scales"), Zone > 1.0f);

		// The order BattleGameMode uses: the door's strength first (Medium: x1 HP and x1 damage; the zone enters
		// nowhere, D-120), then rank x tier on top.
		const float HealthShare = TrialGate::GetEnemyHealthMultiplier(ETrialDifficulty::Medium);
		const float DamageShare = TrialGate::GetEnemyStrengthMultiplier(ETrialDifficulty::Medium);
		TestEqual(TEXT("no HP from the difficulty"), HealthShare, 1.0f);
		TrialGate::ApplyEnemyStrength(Enemy, HealthShare, DamageShare); // a no-op at Medium
		TestEqual(TEXT("after strength: HP unchanged"), static_cast<float>(CharComp->MaxHP), BaseMaxHP, ZONE_ONCE_ROUNDING);
		TestTrue(TEXT("rank applied"), TrialGate::ApplyEnemyRankHealth(Enemy, HealthShare, EEncounterRank::Boss));
		TestEqual(TEXT("base x rank x tier"), static_cast<float>(CharComp->MaxHP), BaseMaxHP * Rank * Tier, ZONE_ONCE_ROUNDING);
		TestTrue(TEXT("less than with the zone's HP"), static_cast<float>(CharComp->MaxHP) < BaseMaxHP * Zone * Rank * Tier - ZONE_ONCE_ROUNDING);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
