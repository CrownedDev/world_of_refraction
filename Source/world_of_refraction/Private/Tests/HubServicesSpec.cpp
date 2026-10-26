// HubServicesSpec.cpp
// B-195 (HubAndStory §3, D-047, D-048, Q-195): every hub service has a merchant NPC (V in an
// outfit), including the Stone Smith and the loadout vendor; the coverage check names the gaps.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Merchant/MerchantData.h"
#include "Merchant/MerchantInteractable.h"

BEGIN_DEFINE_SPEC(FHubServicesSpec, "WoR.Hub.Services",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	void SpawnService(EMerchantType Type)
	{
		UMerchantData *Data = NewObject<UMerchantData>(GetTransientPackage());
		Data->MerchantType = Type;
		AMerchantInteractable *Npc = TestWorld->GetWorld()->SpawnActorDeferred<AMerchantInteractable>(
			AMerchantInteractable::StaticClass(), FTransform::Identity);
		Npc->Merchant = Data;
		Npc->FinishSpawning(FTransform::Identity);
	}
END_DEFINE_SPEC(FHubServicesSpec)

void FHubServicesSpec::Define()
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

	It("names every service with no NPC, the Stone Smith and loadout vendor included", [this]()
	{
		SpawnService(EMerchantType::Blacksmith);
		const TArray<EMerchantType> Missing = AMerchantInteractable::GetMissingServices(TestWorld->GetWorld());
		TestFalse(TEXT("The Blacksmith is there"), Missing.Contains(EMerchantType::Blacksmith));
		TestTrue(TEXT("Stone Smith missing"), Missing.Contains(EMerchantType::StoneSmith));
		TestTrue(TEXT("Loadout vendor missing"), Missing.Contains(EMerchantType::LoadoutVendor));
		TestTrue(TEXT("The check lists the gaps"),
				 AMerchantInteractable::GetServicesString(TestWorld->GetWorld()).Contains(TEXT("Stone Smith")));
	});

	It("finds no gap once each service has its NPC", [this]()
	{
		for (EMerchantType Type : TEnumRange<EMerchantType>())
		{
			SpawnService(Type);
		}
		TestEqual(TEXT("Every service placed"), AMerchantInteractable::GetMissingServices(TestWorld->GetWorld()).Num(), 0);
	});
}

#endif
