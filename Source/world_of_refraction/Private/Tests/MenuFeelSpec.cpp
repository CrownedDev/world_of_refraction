// MenuFeelSpec.cpp
// UI_Theme 5a: the menus' feel, headless. Our own hold-to-repeat fires nothing until 350 ms, then once, then every
// 90 ms, however the time is cut into frames; the ease starts fast and ends on its mark. On the merged screen a
// direction acts through one door (MoveInDirection), wherever the keys are.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "UI/Menus/LoadoutV2ScreenWidget.h"
#include "UI/Menus/MenuFeel.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "WoRTestWorld.h"

BEGIN_DEFINE_SPEC(FMenuFeelSpec, "WoR.UI.MenuFeel", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FMenuFeelSpec)

void FMenuFeelSpec::Define()
{
	It("repeats a held direction after 350 ms, then every 90 ms, however the frames fall", [this]()
	{
		FWoRHoldRepeat Hold;
		Hold.Start();
		TestEqual(TEXT("nothing at 300 ms"), Hold.Advance(0.30f), 0);
		TestEqual(TEXT("the first repeat once 350 ms have passed"), Hold.Advance(0.06f), 1); // 360 ms
		TestEqual(TEXT("nothing 50 ms on"), Hold.Advance(0.05f), 0);						 // 410 ms
		TestEqual(TEXT("the second at 440 ms"), Hold.Advance(0.04f), 1);					 // 450 ms
		TestEqual(TEXT("a long frame owes every repeat it spans"), Hold.Advance(0.20f), 2);	 // 650 ms: 530 and 620
		Hold.Start();
		TestEqual(TEXT("a new press starts the wait again"), Hold.Advance(0.34f), 0);

		// Sixty frames a second for a second: one repeat at 350 ms and then one every 90 ms.
		FWoRHoldRepeat Frames;
		Frames.Start();
		int32 Total = 0;
		constexpr int32 FRAMES_IN_A_SECOND = 60;
		for (int32 Frame = 0; Frame < FRAMES_IN_A_SECOND; ++Frame)
		{
			Total += Frames.Advance(1.0f / FRAMES_IN_A_SECOND);
		}
		const int32 Expected = 1 + FMath::FloorToInt32((1.0f - WoRMenuFeel::HOLD_REPEAT_FIRST_SECONDS) / WoRMenuFeel::HOLD_REPEAT_NEXT_SECONDS);
		TestTrue(TEXT("a second held: the first repeat and those after it"), FMath::Abs(Total - Expected) <= 1); // a frame's rounding at the last one
	});

	It("eases out: fast at first, on its mark at the end", [this]()
	{
		TestEqual(TEXT("starts at 0"), WoRMenuFeel::EaseOut(0.0f), 0.0f);
		TestEqual(TEXT("ends at 1"), WoRMenuFeel::EaseOut(1.0f), 1.0f);
		TestTrue(TEXT("past half way at half time (decelerating, not linear)"), WoRMenuFeel::EaseOut(0.5f) > 0.5f);
		TestEqual(TEXT("clamped past the end"), WoRMenuFeel::EaseOut(2.0f), 1.0f);
	});

	It("on the merged screen a direction with nothing to do is not taken (headless, no party)", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("world"), TestWorld.Create()))
		{
			return;
		}
		ULoadoutV2ScreenWidget *Screen = CreateWidget<ULoadoutV2ScreenWidget>(TestWorld.GetWorld(), ULoadoutV2ScreenWidget::StaticClass());
		if (!TestNotNull(TEXT("screen"), Screen))
		{
			return;
		}
		Screen->ActivateWidget();
		TestTrue(TEXT("on the inventory's side every direction is the grid's"), Screen->MoveInDirection(1, 0) && Screen->MoveInDirection(0, 1));
		TestTrue(TEXT("a move that moves nothing (an empty grid) is silent"), Screen->GetLastCue() == EWoRMenuCue::None);
		Screen->ToggleLoadout();
		TestTrue(TEXT("to the Equipment page: the Confirm cue"), Screen->GetLastCue() == EWoRMenuCue::Confirm);
		TestFalse(TEXT("Left on the loadout's list has nothing to do"), Screen->MoveInDirection(-1, 0));
		TestFalse(TEXT("nor Right (B-598c: no inventory side beside it)"), Screen->MoveInDirection(1, 0));
		TestTrue(TEXT("Down is the list's"), Screen->MoveInDirection(0, 1));
		TestTrue(TEXT("still no Move cue: nothing moved"), Screen->GetLastCue() == EWoRMenuCue::Confirm);
		Screen->ToggleLoadout();
		TestTrue(TEXT("back on the Inventory page"), Screen->IsOnInventoryPage());
		TestTrue(TEXT("Up off the top row: the filter row"), Screen->MoveInDirection(0, -1) && Screen->IsOnInventoryFilters());
		TestTrue(TEXT("and that is heard: the Move cue"), Screen->GetLastCue() == EWoRMenuCue::Move);
		Screen->PressBack();
		TestTrue(TEXT("Back: the Back cue"), Screen->GetLastCue() == EWoRMenuCue::Back);
	});

	It("B-598: a menu icon has no hover tooltip and lets the mouse through to its row", [this]()
	{
		UImage *Icon = NewObject<UImage>(GetTransientPackage());
		UTexture2D *Texture = NewObject<UTexture2D>(GetTransientPackage());
		TestTrue(TEXT("the icon shows"), WoRMenuStyle::SetIcon(Icon, Texture));
		TestTrue(TEXT("no tooltip"), Icon->GetToolTipText().IsEmpty() && !Icon->GetToolTip());
		TestEqual(TEXT("not hit-testable"), Icon->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		TestFalse(TEXT("no texture: hidden"), WoRMenuStyle::SetIcon(Icon, nullptr));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
