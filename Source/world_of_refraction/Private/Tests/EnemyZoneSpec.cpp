// EnemyZoneSpec.cpp
// B-186 (Enemies §4, Q-186): a zone on the map sets the strength of the fights started inside it
// (the highest tier where zones overlap), so the same team fights at two strengths in two zones.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Encounter/EnemyZone.h"
#include "Trial/TrialGate.h"

namespace
{
	constexpr int32 ZONE_TEST_LOW_TIER = 1;
	constexpr int32 ZONE_TEST_HIGH_TIER = 3;
	const FVector ZONE_TEST_FAR_AWAY(100000.0f, 0.0f, 0.0f);
}

BEGIN_DEFINE_SPEC(FEnemyZoneSpec, "WoR.Trial.EnemyZone",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	AEnemyZone *SpawnZone(const FVector &Location, int32 Tier)
	{
		AEnemyZone *Zone = TestWorld->GetWorld()->SpawnActor<AEnemyZone>(Location, FRotator::ZeroRotator);
		if (Zone)
		{
			Zone->Zone.Tier = Tier;
		}
		return Zone;
	}
END_DEFINE_SPEC(FEnemyZoneSpec)

void FEnemyZoneSpec::Define()
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

	It("makes each tier stronger, from base strength at tier 0", [this]()
	{
		TestEqual(TEXT("Tier 0 is base"), AEnemyZone::GetTierStrengthMultiplier(0), 1.0f);
		for (int32 Tier = 1; Tier <= EnemyZoneConstants::MAX_ZONE_TIER; ++Tier)
		{
			TestTrue(FString::Printf(TEXT("Tier %d is stronger than tier %d"), Tier, Tier - 1),
					 AEnemyZone::GetTierStrengthMultiplier(Tier) > AEnemyZone::GetTierStrengthMultiplier(Tier - 1));
		}
		TestEqual(TEXT("Past the top clamps"), AEnemyZone::GetTierStrengthMultiplier(EnemyZoneConstants::MAX_ZONE_TIER + 1),
				  AEnemyZone::GetTierStrengthMultiplier(EnemyZoneConstants::MAX_ZONE_TIER));
	});

	It("gives a fight the highest tier of the zones it starts in, tier 0 outside", [this]()
	{
		AEnemyZone *Low = SpawnZone(FVector::ZeroVector, ZONE_TEST_LOW_TIER);
		AEnemyZone *High = SpawnZone(FVector::ZeroVector, ZONE_TEST_HIGH_TIER);
		if (!TestTrue(TEXT("Zones spawned"), Low && High)) return;
		TestEqual(TEXT("Overlap: highest tier"), AEnemyZone::FindZoneAt(TestWorld->GetWorld(), FVector::ZeroVector).Tier, ZONE_TEST_HIGH_TIER);
		TestEqual(TEXT("Outside: tier 0"), AEnemyZone::FindZoneAt(TestWorld->GetWorld(), ZONE_TEST_FAR_AWAY).Tier, 0);
	});

	It("fights the same team at two strengths in two zones", [this]()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		UCharacterDataComponent *InLow = TestWorld->SpawnCombatant(Data, true)->FindComponentByClass<UCharacterDataComponent>();
		UCharacterDataComponent *InHigh = TestWorld->SpawnCombatant(Data, true)->FindComponentByClass<UCharacterDataComponent>();
		if (!TestTrue(TEXT("Enemies spawned"), InLow && InHigh)) return;
		TrialGate::ApplyEnemyStrength(InLow->GetOwner(), AEnemyZone::GetTierStrengthMultiplier(ZONE_TEST_LOW_TIER));
		TrialGate::ApplyEnemyStrength(InHigh->GetOwner(), AEnemyZone::GetTierStrengthMultiplier(ZONE_TEST_HIGH_TIER));
		TestTrue(TEXT("The higher zone's enemy has more max HP"), InHigh->MaxHP > InLow->MaxHP);
	});
}

#endif
