// NameTablesSpec.cpp
// B-253 (DRY audit A14): crystal names and tier letters come from one table each (ItemIdentity::GetTypeName,
// TierHelpers); the evolution and debug copies forward to them with the same text (the debug ones are private).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Inventory/ItemTier.h"

BEGIN_DEFINE_SPEC(FNameTablesSpec, "WoR.Items.NameTables", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FNameTablesSpec)

void FNameTablesSpec::Define()
{
	It("names an evolution's crystal and tier as before", [this]()
	{
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		Evolution->CrystalType = ECrystalType::Sapphire;
		Evolution->Tier = EItemTier::B_Tier;
		TestEqual(TEXT("crystal"), Evolution->GetCrystalName(), FString(TEXT("Sapphire")));
		TestEqual(TEXT("tier name"), Evolution->GetTierName(), FString(TEXT("B-Tier")));
		TestEqual(TEXT("tier letter"), Evolution->GetTierString(), FString(TEXT("B")));
		TestEqual(TEXT("full name"), Evolution->GetFullItemName(), FString(TEXT("Sapphire (B)")));
	});
}

#endif
