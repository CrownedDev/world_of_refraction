// MaxResourcesSpec.cpp
// B-623: wor.Debug.MaxResources' top-up, headless. Every live balance reaches 9999; one above stays; a second run
// changes nothing. The console command's wallet lookup and save are proven in PIE (docs/PIE/B-623.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyTypes.h"
#include "Debug/Currency/MaxResourcesDebug.h"

namespace
{
	constexpr int32 MAX_RESOURCES_TEST_START = 120;
	constexpr int32 MAX_RESOURCES_TEST_ABOVE = 12000;
}

BEGIN_DEFINE_SPEC(FMaxResourcesSpec, "WoR.Debug.MaxResources",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCurrencyComponent *Wallet = nullptr;
END_DEFINE_SPEC(FMaxResourcesSpec)

void FMaxResourcesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Wallet = Hero ? FWoRTestWorld::AddComponent<UCurrencyComponent>(Hero) : nullptr;
		TestNotNull(TEXT("a wallet"), Wallet);
	});

	AfterEach([this]()
	{
		Wallet = nullptr;
		TestWorld.Reset();
	});

	It("tops every live balance up to 9999, leaves one above, and a second run changes nothing", [this]()
	{
		if (!Wallet) return;
		TestEqual(TEXT("the number is 9999"), WoRMaxResources::TEST_BALANCE, 9999);
		Wallet->Add(ECurrencyType::Gold, MAX_RESOURCES_TEST_START);
		Wallet->Add(ECurrencyType::PillarEssence, MAX_RESOURCES_TEST_ABOVE);

		const TArray<FString> Raised = WoRMaxResources::TopUp(*Wallet);
		for (const ECurrencyType Currency : CurrencyBalances::ALL)
		{
			const int32 Expected = Currency == ECurrencyType::PillarEssence ? MAX_RESOURCES_TEST_ABOVE : WoRMaxResources::TEST_BALANCE;
			TestEqual(*UEnum::GetValueAsString(Currency), Wallet->GetBalance(Currency), Expected);
		}
		TestEqual(TEXT("one line per balance raised"), Raised.Num(), static_cast<int32>(UE_ARRAY_COUNT(CurrencyBalances::ALL)) - 1);
		TestEqual(TEXT("Dormant Essence (skills too, B-550)"), Wallet->GetSkillEssence(), WoRMaxResources::TEST_BALANCE);

		TestEqual(TEXT("a second run raises nothing"), WoRMaxResources::TopUp(*Wallet).Num(), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
