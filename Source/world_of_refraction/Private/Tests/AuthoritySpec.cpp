// AuthoritySpec.cpp
// WoRAuthority::HasAuthority and the components that gate on it (B-027). Not covered: the client
// refusal (a standalone test world is never a client).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "WoRAuthority.h"
#include "Character/CharacterDataComponent.h"
#include "Currency/CurrencyComponent.h"

namespace
{
	template <class TComponent>
	TComponent *AuthorityTestAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	constexpr int32 AUTHORITY_TEST_AMOUNT = 5;
}

BEGIN_DEFINE_SPEC(FAuthoritySpec, "WoR.Authority",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FAuthoritySpec)

void FAuthoritySpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("refuses a null actor", [this]()
	{
		TestFalse(TEXT("null actor"), WoRAuthority::HasAuthority(nullptr));
	});

	It("grants a standalone actor", [this]()
	{
		AActor *Actor = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		TestTrue(TEXT("standalone actor"), WoRAuthority::HasAuthority(Actor));
	});

	It("lets a standalone actor's components change state", [this]()
	{
		AActor *Actor = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UCharacterDataComponent *Character = Actor->FindComponentByClass<UCharacterDataComponent>();
		if (!TestNotNull(TEXT("character component"), Character))
		{
			return;
		}
		const int32 HealthBefore = Character->CurrentHP;
		Character->ServerTakeDamage(AUTHORITY_TEST_AMOUNT);
		TestTrue(TEXT("damage applied"), Character->CurrentHP < HealthBefore);

		UCurrencyComponent *Currency = AuthorityTestAddComponent<UCurrencyComponent>(Actor);
		const int32 GoldBefore = Currency->GetBalance(ECurrencyType::Gold);
		TestTrue(TEXT("gold add accepted"), Currency->Add(ECurrencyType::Gold, AUTHORITY_TEST_AMOUNT));
		TestEqual(TEXT("gold added"), Currency->GetBalance(ECurrencyType::Gold), GoldBefore + AUTHORITY_TEST_AMOUNT);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
