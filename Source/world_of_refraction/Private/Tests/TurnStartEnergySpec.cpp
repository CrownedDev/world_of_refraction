// TurnStartEnergySpec.cpp
// B-173 (D-089, changed 2026-11-09): at the start of each of the holder's turns it gains a share of Max EP (at
// least the minimum), through ServerGainEnergy's gates: Broken Darkness and a dead holder gain none, and EP never
// passes Max EP. No other base rule gives EP (basic attacks and Perfect Guards give it only through gear effects).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Combat/Mechanics/TurnStartEnergy.h"
#include "Debug/Combat/TurnStartEnergyDebug.h"

namespace
{
	constexpr int32 ENERGY_TEST_MAX_EP = 50;
	constexpr int32 ENERGY_TEST_START_EP = 10;
	/** Small enough that the share rounds below the minimum. */
	constexpr int32 ENERGY_TEST_SMALL_MAX_EP = 4;
}

BEGIN_DEFINE_SPEC(FTurnStartEnergySpec, "WoR.Energy.TurnStart",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Hero = nullptr;
	UCharacterDataComponent *Stats = nullptr;

	void SetEP(int32 Current, int32 Max)
	{
		Stats->MaxEP = Max;
		Stats->CurrentEP = Current;
	}

	int32 ExpectedShare(int32 Max) const
	{
		return FMath::Max(CombatConstants::TURN_START_EP_MIN, FMath::RoundToInt(Max * CombatConstants::TURN_START_EP_SHARE_OF_MAX));
	}
END_DEFINE_SPEC(FTurnStartEnergySpec)

void FTurnStartEnergySpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		Hero = TestWorld->SpawnCombatant(Data);
		Stats = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		TestNotNull(TEXT("Hero stats"), Stats);
	});

	AfterEach([this]()
	{
		Hero = nullptr;
		Stats = nullptr;
		TestWorld.Reset();
	});

	It("gains a share of Max EP at turn start", [this]()
	{
		if (!Stats)
		{
			return;
		}
		SetEP(ENERGY_TEST_START_EP, ENERGY_TEST_MAX_EP);
		const int32 Share = ExpectedShare(ENERGY_TEST_MAX_EP);
		TestEqual(TEXT("offer"), TurnStartEnergy::GetAmount(Hero), Share);
		TestEqual(TEXT("gained"), TurnStartEnergy::Apply(Hero), Share);
		TestEqual(TEXT("EP after"), Stats->CurrentEP, ENERGY_TEST_START_EP + Share);
		TestTrue(TEXT("debug line names the offer"),
				 UTurnStartEnergyDebug::GetEnergyString(Hero).Contains(FString::Printf(TEXT("+%d"), Share)));
	});

	It("gains at least the minimum on a small pool", [this]()
	{
		if (!Stats)
		{
			return;
		}
		SetEP(0, ENERGY_TEST_SMALL_MAX_EP);
		TestEqual(TEXT("minimum"), TurnStartEnergy::Apply(Hero), ExpectedShare(ENERGY_TEST_SMALL_MAX_EP));
		TestTrue(TEXT("at least the minimum"), Stats->CurrentEP >= CombatConstants::TURN_START_EP_MIN);
	});

	It("never passes Max EP", [this]()
	{
		if (!Stats)
		{
			return;
		}
		SetEP(ENERGY_TEST_MAX_EP, ENERGY_TEST_MAX_EP);
		TestEqual(TEXT("nothing gained when full"), TurnStartEnergy::Apply(Hero), 0);
		TestEqual(TEXT("still full"), Stats->CurrentEP, ENERGY_TEST_MAX_EP);
	});

	It("gives Broken Darkness nothing (its absorption is its EP)", [this]()
	{
		if (!Stats)
		{
			return;
		}
		SetEP(ENERGY_TEST_START_EP, ENERGY_TEST_MAX_EP);
		Stats->bIsBrokenDarkness = true;
		TestEqual(TEXT("nothing gained"), TurnStartEnergy::Apply(Hero), 0);
		TestEqual(TEXT("EP unchanged"), Stats->CurrentEP, ENERGY_TEST_START_EP);
		TestTrue(TEXT("debug line names the gate"), UTurnStartEnergyDebug::GetEnergyString(Hero).Contains(TEXT("Broken Darkness")));
	});

	It("gives a dead holder nothing", [this]()
	{
		if (!Stats)
		{
			return;
		}
		SetEP(ENERGY_TEST_START_EP, ENERGY_TEST_MAX_EP);
		Stats->bIsAlive = false;
		TestEqual(TEXT("nothing gained"), TurnStartEnergy::Apply(Hero), 0);
		TestEqual(TEXT("EP unchanged"), Stats->CurrentEP, ENERGY_TEST_START_EP);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
