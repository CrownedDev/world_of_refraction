// ElementPaletteSpec.cpp
// B-294 (D-106): the element palette stays readable for colour-blind players. No two of the nine element colours fall
// under CIEDE2000 10 in normal, protan, deutan or tritan vision; Ice and Physical have their own colours; Darkness
// text is outlined.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Infusion/ElementColors.h"
#include "Infusion/ElementColorVision.h"

namespace
{
	/** analysis/ElementPalette_2026-10-29.md section 3a: the closest pair is Water / Void under protan, 10.8. */
	constexpr float PALETTE_SPEC_CLOSEST = 10.8f;
	constexpr float PALETTE_SPEC_TOLERANCE = 0.3f;
	constexpr int32 PALETTE_SPEC_PAIRS = 36; // 9 choose 2
}

BEGIN_DEFINE_SPEC(FElementPaletteSpec, "WoR.Elements.Palette",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FElementPaletteSpec)

void FElementPaletteSpec::Define()
{
	using namespace ElementColorVision;

	It("keeps every element pair readable in every simulated vision", [this]()
	{
		const TArray<FPairDistance> Pairs = ElementPairsClosestFirst();
		TestEqual(TEXT("every pair checked"), Pairs.Num(), PALETTE_SPEC_PAIRS);
		for (const FPairDistance &Pair : Pairs)
		{
			if (Pair.DeltaE < READABLE_DELTA_E)
			{
				AddError(FString::Printf(TEXT("%s / %s reads %.1f apart"), *UEnum::GetValueAsString(Pair.A),
										 *UEnum::GetValueAsString(Pair.B), Pair.DeltaE));
			}
		}
	});

	It("measures as the palette analysis did", [this]()
	{
		const TArray<FPairDistance> Pairs = ElementPairsClosestFirst();
		if (!TestTrue(TEXT("pairs"), Pairs.Num() > 0)) return;
		const FPairDistance &Closest = Pairs[0];
		TestTrue(TEXT("Water / Void is the closest pair"),
				 (Closest.A == ESpellElement::Water && Closest.B == ESpellElement::Void) ||
					 (Closest.A == ESpellElement::Void && Closest.B == ESpellElement::Water));
		TestEqual(TEXT("under protan"), Closest.Where, EColorVision::Protan);
		TestEqual(TEXT("at the analysis' distance"), Closest.DeltaE, PALETTE_SPEC_CLOSEST, PALETTE_SPEC_TOLERANCE);
		TestEqual(TEXT("a colour is no distance from itself"), DeltaE2000(ElementColors::Fire, ElementColors::Fire), 0.0f, KINDA_SMALL_NUMBER);
	});

	It("gives Ice and Physical their own colours and outlines Darkness text", [this]()
	{
		TestNotEqual(TEXT("Ice is not Water"), ElementColors::Ice, ElementColors::Water);
		TestEqual(TEXT("Physical is white (Q-111)"), ElementColors::Physical, FLinearColor::White);
		TestTrue(TEXT("Darkness text is outlined"), ElementColors::NeedsTextOutline(ESpellElement::Darkness));
		TestFalse(TEXT("Fire text is not"), ElementColors::NeedsTextOutline(ESpellElement::Fire));
		TestTrue(TEXT("the debug print lists every element"), GetElementPaletteString().Contains(TEXT("Reality")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
