// TestLoadoutDebugSpec.cpp
// B-503n: wor.Debug.TestLoadout's choices, headless: the variant names, the crystals' elements (never the innate
// element nor the evolution's, all different), the evolution's element and asset, and no played character refused.
// The loadout itself on a real character (the assets, the class rules, R / LB on the strip): PIE (docs/PIE/B-503n.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Debug/Inventory/TestLoadoutDebug.h"

BEGIN_DEFINE_SPEC(FTestLoadoutDebugSpec, "WoR.Debug.TestLoadout", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTestLoadoutDebugSpec)

void FTestLoadoutDebugSpec::Define()
{
	It("reads the variant names, any case; nothing is the played character", [this]()
	{
		ETestLoadoutVariant Variant = ETestLoadoutVariant::Generic;
		TestTrue(TEXT("empty"), WoRTestLoadout::ParseVariant(FString(), Variant) && Variant == ETestLoadoutVariant::Played);
		TestTrue(TEXT("reality"), WoRTestLoadout::ParseVariant(TEXT("reality"), Variant) && Variant == ETestLoadoutVariant::Reality);
		TestTrue(TEXT("BrokenDarkness"), WoRTestLoadout::ParseVariant(TEXT("BrokenDarkness"), Variant) && Variant == ETestLoadoutVariant::BrokenDarkness);
		TestFalse(TEXT("not a variant"), WoRTestLoadout::ParseVariant(TEXT("Paladin"), Variant));
		TestTrue(TEXT("Played swaps nothing"), WoRTestLoadout::GetVariantCharacterPath(ETestLoadoutVariant::Played).IsEmpty());
		for (const ETestLoadoutVariant Each : {ETestLoadoutVariant::Generic, ETestLoadoutVariant::Refractor, ETestLoadoutVariant::Resonator,
											   ETestLoadoutVariant::BrokenDarkness, ETestLoadoutVariant::Reality})
		{
			TestFalse(TEXT("a variant has its test character"), WoRTestLoadout::GetVariantCharacterPath(Each).IsEmpty());
		}
	});

	It("picks different crystal elements, never the innate element nor the evolution's", [this]()
	{
		const TArray<ESpellElement> Elements = WoRTestLoadout::PickCrystalElements(ESpellElement::Fire, ESpellElement::Water, WoRTestLoadout::TEST_CRYSTAL_COUNT);
		TestEqual(TEXT("one per piece"), Elements.Num(), WoRTestLoadout::TEST_CRYSTAL_COUNT);
		TestFalse(TEXT("not the innate element"), Elements.Contains(ESpellElement::Fire));
		TestFalse(TEXT("not the evolution's"), Elements.Contains(ESpellElement::Water));
		TestEqual(TEXT("all different"), TSet<ESpellElement>(Elements).Num(), Elements.Num());
	});

	It("picks an evolution that isn't the innate element and has an asset", [this]()
	{
		for (const ESpellElement Innate : {ESpellElement::None, ESpellElement::Water, ESpellElement::Darkness, ESpellElement::Reality})
		{
			const ESpellElement Element = WoRTestLoadout::PickEvolutionElement(Innate);
			TestTrue(TEXT("another element"), Element != Innate);
			TestFalse(TEXT("its asset"), WoRTestLoadout::GetEvolutionPath(Element).IsEmpty());
		}
	});

	It("refuses with no played character", [this]()
	{
		TestTrue(TEXT("refused"), WoRTestLoadout::GiveTestLoadout(nullptr, ETestLoadoutVariant::Played).Contains(TEXT("no played character")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
