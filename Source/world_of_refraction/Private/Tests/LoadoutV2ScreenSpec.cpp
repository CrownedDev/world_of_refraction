// LoadoutV2ScreenSpec.cpp
// B-476b: the v2 loadout screen, headless. With no party it says so, offers no line, opens no window and Confirm
// does nothing; Back closes it. (Its lines come from WoRLoadoutV2, which WoR.UI.LoadoutV2 covers.)

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "UI/Menus/LoadoutV2ScreenWidget.h"
#include "WoRTestWorld.h"

BEGIN_DEFINE_SPEC(FLoadoutV2ScreenSpec, "WoR.UI.LoadoutV2Screen", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FLoadoutV2ScreenSpec)

void FLoadoutV2ScreenSpec::Define()
{
	It("with no party: says so, opens no window, and Back closes it", [this]()
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
		TestFalse(TEXT("says why it is empty"), Screen->GetMessage().IsEmpty());
		TestEqual(TEXT("no entries"), Screen->GetPage().Entries.Num(), 0);
		Screen->PressA();
		TestFalse(TEXT("no Add gear window"), Screen->IsAdding());
		Screen->PressConfirm();
		Screen->MoveMember(1);
		Screen->MoveSelection(1);
		TestEqual(TEXT("nothing to move to"), Screen->GetSelected(), 0);
		Screen->PressBack();
		TestFalse(TEXT("Back closes it"), Screen->IsActivated());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
