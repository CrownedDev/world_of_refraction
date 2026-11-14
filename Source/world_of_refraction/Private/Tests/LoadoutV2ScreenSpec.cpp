// LoadoutV2ScreenSpec.cpp
// B-476b: the v2 loadout screen, headless. With no party it says so, offers no line, opens no window and Confirm
// does nothing; Back closes it. (Its lines come from WoRLoadoutV2, which WoR.UI.LoadoutV2 covers.)
// B-476c: X switches the full / simple view and the screen remembers it; Y opens no Options and L3 sets nothing
// active without a character; A on nothing opens no window.

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

	It("switches the full / simple view and remembers it; Options and Set active need a character (B-476c)", [this]()
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
		const bool bWasSimple = ULoadoutV2ScreenWidget::IsSimpleView();
		Screen->ActivateWidget();
		Screen->PressCompare();
		TestNotEqual(TEXT("X switches the view"), ULoadoutV2ScreenWidget::IsSimpleView(), bWasSimple);
		ULoadoutV2ScreenWidget *Again = CreateWidget<ULoadoutV2ScreenWidget>(TestWorld.GetWorld(), ULoadoutV2ScreenWidget::StaticClass());
		if (!TestNotNull(TEXT("a second screen"), Again))
		{
			Screen->PressCompare(); // back as it was
			return;
		}
		Again->ActivateWidget();
		TestNotEqual(TEXT("a new screen keeps it"), ULoadoutV2ScreenWidget::IsSimpleView(), bWasSimple);
		Again->PressCompare(); // back as it was, for the next test
		TestEqual(TEXT("X again switches back"), ULoadoutV2ScreenWidget::IsSimpleView(), bWasSimple);

		Screen->PressConfirm();
		TestTrue(TEXT("no Options without a character"), Screen->GetMode() == ELoadoutV2Mode::List);
		Screen->PressSetActive();
		TestFalse(TEXT("Set active says why"), Screen->GetMessage().IsEmpty());
		TestTrue(TEXT("no details"), Screen->GetDetails().IsEmpty());
		Screen->PressBack();
		Again->PressBack();
	});

	It("takes the mouse's clicks: a tab chip turns the tab, a greyed one says later; nothing to click with no party (B-482)", [this]()
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
		Screen->ClickTabChip(LoadoutV2Chip_Apparel);
		TestTrue(TEXT("the Apparel chip"), Screen->GetTab() == ELoadoutV2Tab::Apparel);
		Screen->ClickTabChip(LoadoutV2Chip_Apparel);
		TestTrue(TEXT("again: stays"), Screen->GetTab() == ELoadoutV2Tab::Apparel);
		Screen->ClickTabChip(LoadoutV2Chip_Skills);
		TestTrue(TEXT("the Skills chip (B-476d): its entries"), Screen->GetTab() == ELoadoutV2Tab::Skills && Screen->GetMode() == ELoadoutV2Mode::SkillEntries);
		Screen->ClickTabChip(LoadoutV2Chip_Weapons);
		TestTrue(TEXT("the Weapons chip"), Screen->GetTab() == ELoadoutV2Tab::Weapons);
		Screen->ClickTabChip(LoadoutV2Chip_Cosmetics);
		TestTrue(TEXT("Cosmetics is not built either"), Screen->GetTab() == ELoadoutV2Tab::Weapons);
		Screen->ClickMemberChip(1);
		Screen->ClickLine(0);
		Screen->ClickLine(INDEX_NONE);
		TestTrue(TEXT("no character, no line: nothing opens"), Screen->GetMode() == ELoadoutV2Mode::List && !Screen->IsAdding());
		Screen->PressBack();
		TestFalse(TEXT("Back closes it"), Screen->IsActivated());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
