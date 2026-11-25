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
	/** Sharma, Wu & Dalal 2005, test data pair 1. */
	const FVector SHARMA_PAIR1_A(50.0, 2.6772, -79.7751);
	const FVector SHARMA_PAIR1_B(50.0, 0.0, -82.7485);
	constexpr float SHARMA_PAIR1_DELTA_E = 2.0425f;
	constexpr float SHARMA_TOLERANCE = 0.0001f;

	/** B-636: Crown's hue bands (degrees on the sRGB colour wheel): red Fire, orange Lightning, yellow Earth, green Wind,
	 *  blue Water (DriveFacts.md section 2). */
	constexpr float HUE_RED_MAX = 15.0f;
	constexpr float HUE_RED_WRAP_MIN = 345.0f;
	constexpr float HUE_ORANGE_MAX = 40.0f;
	constexpr float HUE_YELLOW_MAX = 65.0f;
	constexpr float HUE_GREEN_MIN = 90.0f;
	constexpr float HUE_GREEN_MAX = 170.0f;
	constexpr float HUE_BLUE_MIN = 190.0f;
	constexpr float HUE_BLUE_MAX = 250.0f;

	float GetDisplayHue(const FLinearColor &Linear)
	{
		const FColor Display = Linear.ToFColor(/*bSRGB=*/true);
		return FLinearColor(Display.R / 255.0f, Display.G / 255.0f, Display.B / 255.0f).LinearRGBToHSV().R;
	}
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
		TestEqual(TEXT("Sharma's reference pair 1"), DeltaE2000Lab(SHARMA_PAIR1_A, SHARMA_PAIR1_B), SHARMA_PAIR1_DELTA_E, SHARMA_TOLERANCE);
		TestEqual(TEXT("a colour is no distance from itself"), DeltaE2000(ElementColors::Fire, ElementColors::Fire), 0.0f, KINDA_SMALL_NUMBER);
	});

	It("keeps Crown's element hues: red Fire, orange Lightning, yellow Earth, green Wind, blue Water (B-636)", [this]()
	{
		const float Fire = GetDisplayHue(ElementColors::Fire);
		TestTrue(FString::Printf(TEXT("Fire is red (%.0f)"), Fire), Fire <= HUE_RED_MAX || Fire >= HUE_RED_WRAP_MIN);
		const float Lightning = GetDisplayHue(ElementColors::Lightning);
		TestTrue(FString::Printf(TEXT("Lightning is orange (%.0f)"), Lightning), Lightning > HUE_RED_MAX && Lightning <= HUE_ORANGE_MAX);
		const float Earth = GetDisplayHue(ElementColors::Earth);
		TestTrue(FString::Printf(TEXT("Earth is yellow (%.0f)"), Earth), Earth > HUE_ORANGE_MAX && Earth <= HUE_YELLOW_MAX);
		const float Wind = GetDisplayHue(ElementColors::Wind);
		TestTrue(FString::Printf(TEXT("Wind is green (%.0f)"), Wind), Wind >= HUE_GREEN_MIN && Wind <= HUE_GREEN_MAX);
		const float Water = GetDisplayHue(ElementColors::Water);
		TestTrue(FString::Printf(TEXT("Water is blue (%.0f)"), Water), Water >= HUE_BLUE_MIN && Water <= HUE_BLUE_MAX);
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
