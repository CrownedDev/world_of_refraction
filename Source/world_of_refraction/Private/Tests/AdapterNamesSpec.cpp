// AdapterNamesSpec.cpp
// B-298 (D-107): the spell-channelling gear is an "Adapter" to the player; a ring is one form of it. The name table
// gives the words, every adapter asset starts as the Ring form, and the Jeweler sells adapters.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRGameNames.h"
#include "Equipment/Rings/EAdapterForm.h"
#include "Equipment/Rings/RingData.h"
#include "Merchant/MerchantData.h"

BEGIN_DEFINE_SPEC(FAdapterNamesSpec, "WoR.Loadout.AdapterNames", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FAdapterNamesSpec)

void FAdapterNamesSpec::Define()
{
	It("names the gear Adapter, plural and active", [this]()
	{
		TestEqual(TEXT("singular"), WoRGameNames::GetAdapterWord().ToString(), FString(TEXT("Adapter")));
		TestEqual(TEXT("plural"), WoRGameNames::GetAdapterWordPlural().ToString(), FString(TEXT("Adapters")));
		TestEqual(TEXT("active"), WoRGameNames::GetActiveAdapterWord().ToString(), FString(TEXT("Active adapter")));
	});

	It("names each form, and a ring is one of them", [this]()
	{
		TestEqual(TEXT("Ring"), WoRGameNames::GetAdapterFormName(EAdapterForm::Ring).ToString(), FString(TEXT("Ring")));
		TestEqual(TEXT("Pistol"), WoRGameNames::GetAdapterFormName(EAdapterForm::Pistol).ToString(), FString(TEXT("Pistol")));
		TestEqual(TEXT("Trainer"), WoRGameNames::GetAdapterFormName(EAdapterForm::Trainer).ToString(), FString(TEXT("Trainer")));
	});

	It("starts every adapter asset as the Ring form, with no form art set", [this]()
	{
		const URingData *Adapter = GetDefault<URingData>();
		TestTrue(TEXT("Ring form"), Adapter->Form == EAdapterForm::Ring);
		TestTrue(TEXT("no form mesh"), Adapter->FormMesh.IsNull());
		TestTrue(TEXT("no form montage"), Adapter->FormCastMontage.IsNull());
	});

	It("has the Jeweler sell adapters", [this]()
	{
		TestTrue(TEXT("Jeweler wares"),
				 WoRGameNames::GetMerchantTypeName(EMerchantType::Jeweler).ToString().Contains(TEXT("adapters")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
