// RecruitCardWidgetSpec.cpp
// B-471b (D-179): the recruit card's screen, headless. With no recruit it says so, offers nothing and invites no one;
// Back closes it. (Its choices come from WoRRecruitCard::Build, which WoR.Party.RecruitCard covers.)

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "UI/Party/RecruitCardWidget.h"
#include "WoRTestWorld.h"

BEGIN_DEFINE_SPEC(FRecruitCardWidgetSpec, "WoR.UI.RecruitCard", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FRecruitCardWidgetSpec)

void FRecruitCardWidgetSpec::Define()
{
	It("with no recruit: says so, offers nothing, and Back closes it", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("world"), TestWorld.Create()))
		{
			return;
		}
		URecruitCardWidget *Card = CreateWidget<URecruitCardWidget>(TestWorld.GetWorld(), URecruitCardWidget::StaticClass());
		if (!TestNotNull(TEXT("card"), Card))
		{
			return;
		}
		Card->ActivateWidget();
		Card->SetRecruit(nullptr);
		TestEqual(TEXT("no choices"), Card->GetChoiceCount(), 0);
		TestFalse(TEXT("the reason"), Card->GetCard().Refusal.IsEmpty());
		Card->MoveSelection(1);
		TestEqual(TEXT("nothing to move to"), Card->GetSelectedChoice(), 0);
		Card->PressInvite();
		TestTrue(TEXT("still open: nobody joined"), Card->IsActivated());
		TestFalse(TEXT("it says why"), Card->GetMessage().IsEmpty());
		Card->PressBack();
		TestFalse(TEXT("Back closes it"), Card->IsActivated());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
