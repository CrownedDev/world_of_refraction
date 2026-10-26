// CurrencyClampSpec.cpp
// B-075: a balance stops at the int32 limit instead of wrapping negative.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"

namespace
{
	constexpr int32 CURRENCY_CLAMP_TEST_AMOUNT = 10;
}

BEGIN_DEFINE_SPEC(FCurrencyClampSpec, "WoR.Economy.CurrencyClamp",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FCurrencyClampSpec)

void FCurrencyClampSpec::Define()
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

	It("stops a balance at the int32 limit instead of wrapping", [this]()
	{
		AActor *Holder = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<AActor>() : nullptr;
		if (!TestNotNull(TEXT("Holder"), Holder)) return;
		UCurrencyComponent *Wallet = NewObject<UCurrencyComponent>(Holder);
		Holder->AddInstanceComponent(Wallet);
		Wallet->RegisterComponent();

		for (ECurrencyType Currency : {ECurrencyType::Gold, ECurrencyType::Prisms, ECurrencyType::GearEssence, ECurrencyType::EssenceTyped})
		{
			TestTrue(TEXT("Add to near the limit"), Wallet->Add(Currency, TNumericLimits<int32>::Max() - CURRENCY_CLAMP_TEST_AMOUNT));
			Wallet->Add(Currency, CURRENCY_CLAMP_TEST_AMOUNT * 2);
			TestEqual(TEXT("Held at the limit"), Wallet->GetBalance(Currency), TNumericLimits<int32>::Max());
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
