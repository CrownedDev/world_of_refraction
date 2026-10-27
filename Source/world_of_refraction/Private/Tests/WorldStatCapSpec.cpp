// WorldStatCapSpec.cpp
// B-222 (Crown, analysis/BaselineNumbers.md): a pillar can't pass 7 in a run, 21 in total. The asset level plus
// head-start clamps at 7, and earned or bought levels stop there.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/StatConstants.h"

namespace
{
	constexpr int32 CAP_TEST_ASSET_LEVEL = 5;
	constexpr int32 CAP_TEST_ONE_LEVEL = 1;
}

BEGIN_DEFINE_SPEC(FWorldStatCapSpec, "WoR.WorldStat.LiveCap", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCharacterDataComponent *Char = nullptr;
END_DEFINE_SPEC(FWorldStatCapSpec)

void FWorldStatCapSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->WorldBodyLevel = CAP_TEST_ASSET_LEVEL;
		AActor *Hero = TestWorld->SpawnCombatant(Data);
		Char = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		TestNotNull(TEXT("CharacterDataComponent"), Char);
	});

	AfterEach([this]()
	{
		Char = nullptr;
		TestWorld.Reset();
	});

	It("is the asset's own ceiling, 7", [this]()
	{
		TestEqual(TEXT("LIVE_MAX"), WorldStatConstants::LIVE_MAX, StatConstants::MAX_WORLD_STAT_LEVEL);
		TestEqual(TEXT("7 per pillar"), WorldStatConstants::LIVE_MAX, 7);
	});

	It("clamps the asset level plus head-start at 7", [this]()
	{
		if (!Char) return;
		Char->HeadStartBody = WorldStatConstants::HEAD_START_BUDGET;
		Char->SeedWorldStats();
		TestEqual(TEXT("5 + 4 reads 7"), Char->GetWorldBody(), WorldStatConstants::LIVE_MAX);
	});

	It("stops earned levels at 7", [this]()
	{
		if (!Char) return;
		TestTrue(TEXT("5 + 2 is granted"), Char->AddEarnedWorldStat(EWorldPillar::Body, WorldStatConstants::LIVE_MAX - CAP_TEST_ASSET_LEVEL));
		TestEqual(TEXT("at 7"), Char->GetWorldBody(), WorldStatConstants::LIVE_MAX);
		TestFalse(TEXT("one more is refused"), Char->AddEarnedWorldStat(EWorldPillar::Body, CAP_TEST_ONE_LEVEL));
		TestEqual(TEXT("still 7"), Char->GetWorldBody(), WorldStatConstants::LIVE_MAX);
	});

	It("clamps a big earned step at 7", [this]()
	{
		if (!Char) return;
		TestTrue(TEXT("granted"), Char->AddEarnedWorldStat(EWorldPillar::Body, WorldStatConstants::LIVE_MAX));
		TestEqual(TEXT("7, not 12"), Char->GetWorldBody(), WorldStatConstants::LIVE_MAX);
	});
}

#endif
