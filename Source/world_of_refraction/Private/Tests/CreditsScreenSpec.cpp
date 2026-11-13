// CreditsScreenSpec.cpp
// B-470: the credits' model. Every section has a heading and at least one line; the maker comes first; holding Select
// scrolls faster. The screen itself is the PIE sheet (docs/PIE/B-470.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/Menus/CreditsScreenModel.h"

namespace
{
	constexpr float CREDITS_TEST_SECOND = 1.0f;
}

BEGIN_DEFINE_SPEC(FCreditsScreenSpec, "WoR.UI.Credits", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FCreditsScreenSpec)

void FCreditsScreenSpec::Define()
{
	It("names the game, then sections that each have a heading and at least one line", [this]()
	{
		TestFalse(TEXT("the game's name"), WoRCredits::GetGameTitle().IsEmpty());
		const TArray<FCreditsSection> Sections = WoRCredits::BuildSections();
		TestTrue(TEXT("sections"), Sections.Num() > 0);
		for (const FCreditsSection &Section : Sections)
		{
			TestFalse(TEXT("a heading"), Section.Heading.IsEmpty());
			TestTrue(TEXT("at least one line"), Section.Lines.Num() > 0);
			for (const FText &Line : Section.Lines)
			{
				TestFalse(TEXT("a line says something"), Line.IsEmpty());
			}
		}
		TestEqual(TEXT("the maker first"), Sections[0].Lines[0].ToString(), FString(TEXT("Crown")));
		TestTrue(TEXT("the debug string has every heading"), WoRCredits::GetCreditsString().Contains(Sections.Last().Heading.ToString()));
	});

	It("scrolls at its speed, and faster while Select is held", [this]()
	{
		TestEqual(TEXT("a second's scroll"), WoRCredits::GetScrollStep(CREDITS_TEST_SECOND, false), WoRCredits::SCROLL_SPEED);
		TestEqual(TEXT("held"), WoRCredits::GetScrollStep(CREDITS_TEST_SECOND, true), WoRCredits::SCROLL_SPEED * WoRCredits::FASTER_SCALE);
		TestEqual(TEXT("no time, no scroll"), WoRCredits::GetScrollStep(0.0f, true), 0.0f);
	});
}

#endif
