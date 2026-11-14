// LoadoutV2CompareSpec.cpp
// B-476e (UIScreens.md #2-3): an apparel row says its HP, Defence and Amp lines (the v2 Apparel box and Compare read
// them); the Amp lines in words, only the ones it has.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/FAmpBonus.h"
#include "Loadout/Entries/FApparelEntry.h"
#include "UI/Menus/LoadoutScreenModel.h"

namespace
{
	constexpr int32 COMPARE_TEST_HP = 20;
	constexpr int32 COMPARE_TEST_DEFENCE = 3;
	constexpr int32 COMPARE_TEST_FIRE_AMP = 2;

	bool HasDetail(const FLoadoutScreenRow &Row, const TCHAR *Label, const FString &Value)
	{
		return Row.Details.ContainsByPredicate([Label, &Value](const FLoadoutScreenDetail &Detail)
		{
			return Detail.Label.ToString() == Label && Detail.Value.ToString().Contains(Value);
		});
	}
}

BEGIN_DEFINE_SPEC(FLoadoutV2CompareSpec, "WoR.UI.LoadoutV2Compare", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FLoadoutV2CompareSpec)

void FLoadoutV2CompareSpec::Define()
{
	It("says the Amp lines it has, in words, and nothing without any", [this]()
	{
		TestTrue(TEXT("no Amp: empty"), WoRLoadoutScreen::GetAmpText(FAmpBonus()).IsEmpty());
		FAmpBonus Amp;
		Amp.Fire = COMPARE_TEST_FIRE_AMP;
		Amp.Spell = 1;
		const FString Text = WoRLoadoutScreen::GetAmpText(Amp).ToString();
		TestTrue(TEXT("the Fire line"), Text.Contains(TEXT("Amp +2")));
		TestTrue(TEXT("the Spell line"), Text.Contains(TEXT("Spell Amp +1")));
		TestFalse(TEXT("no line it hasn't"), Text.Contains(TEXT("Physical")));
	});

	It("gives an apparel row its HP, Defence and Amp", [this]()
	{
		UApparelData *Coat = NewObject<UApparelData>(GetTransientPackage());
		Coat->BaseStatBonus.BonusMaxHP = COMPARE_TEST_HP;
		Coat->BaseStatBonus.BonusDefense = COMPARE_TEST_DEFENCE;
		Coat->BaseStatBonus.Amp.Fire = COMPARE_TEST_FIRE_AMP;
		FApparelEntry Entry;
		Entry.Apparel = Coat;
		const FLoadoutScreenRow Row = WoRLoadoutScreen::BuildApparelRow(Entry);
		TestTrue(TEXT("HP"), HasDetail(Row, TEXT("HP"), TEXT("+20")));
		TestTrue(TEXT("Defence"), HasDetail(Row, TEXT("Defence"), TEXT("+3")));
		TestTrue(TEXT("Amp"), HasDetail(Row, TEXT("Amp"), TEXT("+2")));
		FApparelEntry Plain;
		Plain.Apparel = NewObject<UApparelData>(GetTransientPackage());
		TestFalse(TEXT("no lines it hasn't"), HasDetail(WoRLoadoutScreen::BuildApparelRow(Plain), TEXT("HP"), TEXT("")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
