// CrystalDescriptionSpec.cpp
// B-460 (D-061): the crystal and stone description table is FText. Every crystal kind at every tier reads as a whole
// sentence with no argument left unfilled; the flavor sentence keeps its old words.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Crystals/CrystalDescription.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Inventory/ItemTier.h"

BEGIN_DEFINE_SPEC(FCrystalDescriptionSpec, "WoR.Items.CrystalDescription", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FCrystalDescriptionSpec)

void FCrystalDescriptionSpec::Define()
{
	It("keeps the flavor sentence's words", [this]()
	{
		TestEqual(TEXT("a gem"), CrystalDescription::GetCrystalText(FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier)).ToString(), FString(TEXT("A crude garnet crystal.")));
		TestEqual(TEXT("the S adjective"), CrystalDescription::GetTierDescriptor(EItemTier::S_Tier).ToString(), FString(TEXT("legendary")));
	});

	It("fills every argument for every kind and tier", [this]()
	{
		const UEnum *Types = StaticEnum<ECrystalType>();
		const EItemTier Tiers[] = {EItemTier::F_Tier, EItemTier::C_Tier, EItemTier::S_Tier};
		for (int32 Index = 0; Types && Index < Types->NumEnums() - 1; ++Index)
		{
			const ECrystalType Type = static_cast<ECrystalType>(Types->GetValueByIndex(Index));
			if (Type == ECrystalType::None)
			{
				continue;
			}
			for (const EItemTier Tier : Tiers)
			{
				const FString Effect = CrystalDescription::GetItemEffectText(FCrystalId(Type, Tier)).ToString();
				const FString Flavor = CrystalDescription::GetCrystalText(FCrystalId(Type, Tier)).ToString();
				const FString Label = FString::Printf(TEXT("%s %s"), *Types->GetNameStringByIndex(Index), *UEnum::GetValueAsString(Tier));
				TestFalse(Label + TEXT(": effect text"), Effect.IsEmpty() || Effect.Contains(TEXT("{")));
				TestFalse(Label + TEXT(": flavor text"), Flavor.IsEmpty() || Flavor.Contains(TEXT("{")));
			}
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
