// PillarsOnlySpec.cpp
// B-330 (D-123): the player picks pillars only. A level-up raises a pillar and grants no sub-stat points; the
// sub-stat budget is the same at every pillar level (Q-330).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/StatConstants.h"

namespace
{
	constexpr int32 PILLARS_TEST_START_LEVEL = 2;
	constexpr int32 PILLARS_TEST_ONE_LEVEL = 1;
	constexpr int32 PILLARS_TEST_RAW_DAMAGE = 6;
	constexpr int32 PILLARS_TEST_DEFENSE = 4;
	/** The enemy caliber (World Stat Points per kill) is unchanged by B-330: 30 + 3 per pillar level. */
	constexpr int32 PILLARS_TEST_CALIBER_AT_ZERO = 30;
	constexpr int32 PILLARS_TEST_CALIBER_AT_MAX = 93;
}

BEGIN_DEFINE_SPEC(FPillarsOnlySpec, "WoR.WorldStat.PillarsOnly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCharacterData *Data = nullptr;
	UCharacterDataComponent *Char = nullptr;
END_DEFINE_SPEC(FPillarsOnlySpec)

void FPillarsOnlySpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Data = FWoRTestWorld::NewCharacterData();
		Data->WorldBodyLevel = PILLARS_TEST_START_LEVEL;
		Data->RawDamage = PILLARS_TEST_RAW_DAMAGE;
		Data->Defense = PILLARS_TEST_DEFENSE;
		AActor *Hero = TestWorld->SpawnCombatant(Data);
		Char = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		TestNotNull(TEXT("CharacterDataComponent"), Char);
	});

	AfterEach([this]()
	{
		Char = nullptr;
		Data = nullptr;
		TestWorld.Reset();
	});

	It("gives a pillar level and no sub-stat points on a level-up", [this]()
	{
		if (!Char || !Data) return;
		const int32 SpentBefore = Data->GetTotalSpent();
		const int32 BudgetBefore = Data->GetSubStatBudget();

		TestTrue(TEXT("the level-up is granted"), Char->AddEarnedWorldStat(EWorldPillar::Body, PILLARS_TEST_ONE_LEVEL));
		TestEqual(TEXT("the pillar went up one"), Char->GetWorldBody(), PILLARS_TEST_START_LEVEL + PILLARS_TEST_ONE_LEVEL);
		TestEqual(TEXT("sub-stat points unchanged"), Data->GetTotalSpent(), SpentBefore);
		TestEqual(TEXT("Raw Damage points unchanged"), Data->GetTotalRawDamage(), PILLARS_TEST_RAW_DAMAGE);
		TestEqual(TEXT("sub-stat budget unchanged"), Data->GetSubStatBudget(), BudgetBefore);
	});

	It("has the same sub-stat budget at pillar level 0 and at 7/7/7", [this]()
	{
		UCharacterData *Fresh = FWoRTestWorld::NewCharacterData();
		UCharacterData *Maxed = FWoRTestWorld::NewCharacterData();
		Maxed->WorldMindLevel = StatConstants::MAX_WORLD_STAT_LEVEL;
		Maxed->WorldBodyLevel = StatConstants::MAX_WORLD_STAT_LEVEL;
		Maxed->WorldSpiritLevel = StatConstants::MAX_WORLD_STAT_LEVEL;
		TestEqual(TEXT("budget at 0 is the authoring ceiling"), Fresh->GetSubStatBudget(), StatConstants::MAX_AUTHORED_SUBSTAT_POINTS);
		TestEqual(TEXT("pillar levels add nothing"), Maxed->GetSubStatBudget(), Fresh->GetSubStatBudget());
	});

	It("still scales the enemy caliber (GetTotalPool, OBSOLETE as a pool) with pillar levels", [this]()
	{
		UCharacterData *Fresh = FWoRTestWorld::NewCharacterData();
		UCharacterData *Maxed = FWoRTestWorld::NewCharacterData();
		Maxed->WorldMindLevel = StatConstants::MAX_WORLD_STAT_LEVEL;
		Maxed->WorldBodyLevel = StatConstants::MAX_WORLD_STAT_LEVEL;
		Maxed->WorldSpiritLevel = StatConstants::MAX_WORLD_STAT_LEVEL;
		TestEqual(TEXT("caliber at 0/0/0 is 30"), Fresh->GetTotalPool(), PILLARS_TEST_CALIBER_AT_ZERO);
		TestEqual(TEXT("caliber at 7/7/7 is 93"), Maxed->GetTotalPool(), PILLARS_TEST_CALIBER_AT_MAX);
	});

	It("keeps an authored spread within the budget", [this]()
	{
		if (!Data) return;
		TestTrue(TEXT("6 + 4 points fit"), Data->IsWithinSubStatBudget());
	});
}

#endif
