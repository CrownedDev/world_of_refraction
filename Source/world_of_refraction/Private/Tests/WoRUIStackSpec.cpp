// WoRUIStackSpec.cpp
// B-103: the CommonUI base layer. A layer stack takes the test screen; Back closes it.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "UI/WoRTestScreen.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

BEGIN_DEFINE_SPEC(FWoRUIStackSpec, "WoR.UI.Stack",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FWoRUIStackSpec)

void FWoRUIStackSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("closes the test screen on Back", [this]()
	{
		UWoRTestScreen *Screen = CreateWidget<UWoRTestScreen>(TestWorld->GetWorld(), UWoRTestScreen::StaticClass());
		if (!TestNotNull(TEXT("test screen created"), Screen))
		{
			return;
		}
		Screen->ActivateWidget();
		TestTrue(TEXT("activated"), Screen->IsActivated());
		Screen->PressBack();
		TestFalse(TEXT("Back deactivates it"), Screen->IsActivated());
	});

	It("pushes the test screen onto a layer stack and removes it again", [this]()
	{
		// A stack lives in a user widget's tree (its widget pool needs that parent), as in the root layout.
		// Any concrete user widget hosts it; the stack set as root wins over the screen's own content.
		// Headless, so nothing is shown: activation through the stack (it needs Slate and the CommonUI
		// input data) is checked in PIE (docs/PIE/B-103.md).
		UUserWidget *Host = CreateWidget<UWoRTestScreen>(TestWorld->GetWorld(), UWoRTestScreen::StaticClass());
		if (!TestTrue(TEXT("host widget with a tree"), Host && Host->WidgetTree))
		{
			return;
		}
		UCommonActivatableWidgetStack *Stack = Host->WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>();
		Host->WidgetTree->RootWidget = Stack;
		UWoRTestScreen *Screen = Stack->AddWidget<UWoRTestScreen>(UWoRTestScreen::StaticClass());
		if (!TestNotNull(TEXT("pushed"), Screen))
		{
			return;
		}
		TestEqual(TEXT("one widget on the stack"), Stack->GetNumWidgets(), 1);
		Stack->RemoveWidget(*Screen);
		TestEqual(TEXT("removed"), Stack->GetNumWidgets(), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
