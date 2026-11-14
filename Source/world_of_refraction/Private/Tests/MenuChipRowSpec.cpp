// MenuChipRowSpec.cpp
// B-482: the shared chip row (WoRMenuStyle::MakeChipRow / SetChips / FindChipUnder). One chip per state and the rest
// hidden; the current one filled, a greyed one never; a note only where one is given; nothing under the mouse before
// the row is drawn.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRTestScreen.h"
#include "WoRTestWorld.h"

namespace
{
	constexpr int32 CHIP_TEST_COUNT = 4;
	constexpr int32 CHIP_TEST_FIRST = 0;
	constexpr int32 CHIP_TEST_GREYED = 1;
	constexpr int32 CHIP_TEST_HIDDEN = 3;
}

BEGIN_DEFINE_SPEC(FMenuChipRowSpec, "WoR.UI.MenuChipRow", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FMenuChipRowSpec)

void FMenuChipRowSpec::Define()
{
	It("shows one chip per state, fills the current one, greys one that isn't there yet and notes it", [this]()
	{
		using namespace WoRMenuStyle;
		FWoRTestWorld World;
		if (!TestTrue(TEXT("world"), World.Create()))
		{
			return;
		}
		UWoRTestScreen *Screen = CreateWidget<UWoRTestScreen>(World.GetWorld(), UWoRTestScreen::StaticClass());
		if (!TestNotNull(TEXT("a screen to own the chips"), Screen) || !TestNotNull(TEXT("its tree"), Screen->WidgetTree.Get()))
		{
			return;
		}
		FChipRow Row;
		UHorizontalBox *Box = MakeChipRow(Screen->WidgetTree, CHIP_TEST_COUNT, FText::FromString(TEXT("{TabPrev}")), FText::FromString(TEXT("{TabNext}")), Row);
		TestNotNull(TEXT("the row"), Box);
		if (!TestEqual(TEXT("its chips"), Row.Chips.Num(), CHIP_TEST_COUNT))
		{
			return;
		}
		const TArray<FChipState> States = {FChipState{FText::FromString(TEXT("Weapons"))},
										   FChipState{FText::FromString(TEXT("Skills")), true, FText::FromString(TEXT("later"))},
										   FChipState{FText::FromString(TEXT("Apparel"))}};
		SetChips(Row, States, CHIP_TEST_FIRST);
		TestEqual(TEXT("named"), Row.Labels[CHIP_TEST_FIRST]->GetText().ToString(), FString(TEXT("Weapons")));
		TestTrue(TEXT("the current one is filled"), Row.Chips[CHIP_TEST_FIRST]->GetBrushColor() == FocusColor);
		TestTrue(TEXT("a greyed chip's name is greyed"), Row.Labels[CHIP_TEST_GREYED]->GetColorAndOpacity().GetSpecifiedColor() == OffColor);
		TestTrue(TEXT("its note shows"), Row.Notes[CHIP_TEST_GREYED]->GetVisibility() != ESlateVisibility::Collapsed);
		TestTrue(TEXT("no note on the others"), Row.Notes[CHIP_TEST_FIRST]->GetVisibility() == ESlateVisibility::Collapsed);
		TestTrue(TEXT("a chip with no state is hidden"), Row.Chips[CHIP_TEST_HIDDEN]->GetVisibility() == ESlateVisibility::Collapsed);
		TestEqual(TEXT("the debug string reads as the row does"), GetChipRowString(Row), FString(TEXT("[Weapons] Skills (later) Apparel")));

		SetChips(Row, States, CHIP_TEST_GREYED, CHIP_TEST_GREYED);
		TestTrue(TEXT("a greyed chip is never filled or lit"), Row.Chips[CHIP_TEST_GREYED]->GetBrushColor() == PlateColor);
		SetChips(Row, States, CHIP_TEST_FIRST, CHIP_TEST_FIRST + 2);
		TestTrue(TEXT("the one under the mouse is lit"), Row.Chips[CHIP_TEST_FIRST + 2]->GetBrushColor() == HoverColor);
		TestEqual(TEXT("nothing under the mouse before the row is drawn"), FindChipUnder(Row, FVector2D::ZeroVector), static_cast<int32>(INDEX_NONE));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
