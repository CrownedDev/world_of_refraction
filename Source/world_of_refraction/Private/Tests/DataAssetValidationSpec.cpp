// DataAssetValidationSpec.cpp
// B-120: editor validation and debug dumps for the data assets that had none.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Character/CosmeticsData.h"
#include "Character/StanceData.h"
#include "Infusion/InfusionDisplayData.h"
#include "Inventory/InventoryData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Trial/TrialData.h"

namespace
{
	const TCHAR *TEST_LEVEL_PATH = TEXT("/Game/Maps/TestTrialLevel.TestTrialLevel");
	// Mirrors the Expected[] list in UCosmeticsData::IsDataValid; update both together.
	constexpr int32 COSMETICS_EXPECTED_FIELDS = 6;
}

BEGIN_DEFINE_SPEC(FDataAssetValidationSpec, "WoR.Data.Validation",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDataAssetValidationSpec)

void FDataAssetValidationSpec::Define()
{
	Describe("UTrialData", [this]()
	{
		It("is invalid without a Level", [this]()
		{
			UTrialData *Trial = NewObject<UTrialData>();
			FDataValidationContext Context;
			TestEqual(TEXT("Result"), Trial->IsDataValid(Context), EDataValidationResult::Invalid);
			TestEqual(TEXT("One error"), Context.GetNumErrors(), 1u);
		});

		It("is valid with a Level, warning about the missing name and encounter level", [this]()
		{
			UTrialData *Trial = NewObject<UTrialData>();
			Trial->Level = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEST_LEVEL_PATH));
			FDataValidationContext Context;
			TestNotEqual(TEXT("Result"), Trial->IsDataValid(Context), EDataValidationResult::Invalid);
			TestEqual(TEXT("No errors"), Context.GetNumErrors(), 0u);
			TestEqual(TEXT("Two warnings"), Context.GetNumWarnings(), 2u);
		});

		It("lists its level and variations in the debug string", [this]()
		{
			UTrialData *Trial = NewObject<UTrialData>();
			Trial->Level = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEST_LEVEL_PATH));
			Trial->Variations.AddDefaulted();
			const FString Dump = Trial->GetTrialDataString();
			TestTrue(TEXT("Has the level"), Dump.Contains(TEST_LEVEL_PATH));
			TestTrue(TEXT("Has the variation"), Dump.Contains(TEXT("[0]")));
		});
	});

	It("warns, never errors, about an empty cosmetics asset", [this]()
	{
		UCosmeticsData *Cosmetics = NewObject<UCosmeticsData>();
		FDataValidationContext Context;
		TestNotEqual(TEXT("Result"), Cosmetics->IsDataValid(Context), EDataValidationResult::Invalid);
		TestEqual(TEXT("No errors"), Context.GetNumErrors(), 0u);
		TestEqual(TEXT("One warning per expected field"), Context.GetNumWarnings(), uint32(COSMETICS_EXPECTED_FIELDS));
		TestTrue(TEXT("Dump names the asset"), Cosmetics->GetCosmeticsDataString().Contains(Cosmetics->GetName()));
	});

	It("warns about a stance with no montage and an infusion display with no VFX", [this]()
	{
		FDataValidationContext StanceContext;
		NewObject<UStanceData>()->IsDataValid(StanceContext);
		TestEqual(TEXT("Stance: no errors"), StanceContext.GetNumErrors(), 0u);
		TestEqual(TEXT("Stance: montage warning"), StanceContext.GetNumWarnings(), 1u);

		FDataValidationContext InfusionContext;
		NewObject<UInfusionDisplayData>()->IsDataValid(InfusionContext);
		TestEqual(TEXT("Infusion: no errors"), InfusionContext.GetNumErrors(), 0u);
		TestEqual(TEXT("Infusion: VFX warning"), InfusionContext.GetNumWarnings(), 1u);
	});

	It("dumps skills and inventories without a character", [this]()
	{
		UAbilityData *Ability = NewObject<UAbilityData>();
		TestTrue(TEXT("Skill dump names the skill"), Ability->GetSkillDataString().Contains(Ability->Name));

		UInventoryData *Inventory = NewObject<UInventoryData>();
		Inventory->Abilities.Add(Ability);
		TestTrue(TEXT("Inventory dump lists the ability"),
				 Inventory->GetInventoryDataString().Contains(FString::Printf(TEXT("Abilities (1): %s"), *Ability->GetName())));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
