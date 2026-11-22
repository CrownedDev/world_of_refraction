// AgentPlayHoldSpec.cpp
// B-573, headless: taking the hold swaps in the agent's pointer (resting over nothing) and turns the background
// throttle off; releasing puts the real application and the throttle back exactly; both are safe to repeat.
// Unproven here: hover, click and wheel in a PIE game while the real pointer moves (the brain's live check).

#if WITH_DEV_AUTOMATION_TESTS

#include "AgentPlay/AgentPlayHold.h"
#include "Editor/EditorPerformanceSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/ICursor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

BEGIN_DEFINE_SPEC(FAgentPlayHoldSpec, "WoR.AgentPlay.Hold", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FAgentPlayHoldSpec)

void FAgentPlayHoldSpec::Define()
{
	It("takes its own pointer and the throttle, and gives both back as they were", [this]()
	{
		if (!FSlateApplication::IsInitialized())
		{
			AddInfo(TEXT("no Slate application here: skipped"));
			return;
		}
		FSlateApplication &Slate = FSlateApplication::Get();
		UEditorPerformanceSettings *Performance = GetMutableDefault<UEditorPerformanceSettings>();
		const TSharedPtr<GenericApplication> Real = Slate.GetPlatformApplication();
		const TSharedPtr<ICursor> RealCursor = Real.IsValid() ? Real->Cursor : nullptr;
		const bool bThrottledBefore = Performance->bThrottleCPUWhenNotForeground;
		Performance->bThrottleCPUWhenNotForeground = true;
		ON_SCOPE_EXIT
		{
			WoRAgentPlayHold::Release();
			Performance->bThrottleCPUWhenNotForeground = bThrottledBefore;
		};

		TestFalse(TEXT("not taken at first"), WoRAgentPlayHold::IsTaken());
		WoRAgentPlayHold::Take();
		WoRAgentPlayHold::Take(); // a second take changes nothing
		TestTrue(TEXT("taken"), WoRAgentPlayHold::IsTaken());
		TestTrue(TEXT("the pointer rests over nothing"), WoRAgentPlayHold::IsPointerParked());
		TestTrue(TEXT("the application is the agent's"), Slate.GetPlatformApplication() != Real);
		TestTrue(TEXT("the cursor is the agent's"), Slate.GetPlatformApplication()->Cursor != RealCursor);
		TestFalse(TEXT("a resting pointer is over no window"), Slate.GetPlatformApplication()->IsCursorDirectlyOverSlateWindow());
		TestFalse(TEXT("the throttle is off"), static_cast<bool>(Performance->bThrottleCPUWhenNotForeground));

		Slate.SetCursorPos(FVector2D(100.0, 100.0));
		TestFalse(TEXT("a placed pointer is not resting"), WoRAgentPlayHold::IsPointerParked());
		TestTrue(TEXT("the mouse Slate reads is the agent's, at the point it was put"), FVector2D(Slate.GetCursorPos()) == FVector2D(100.0, 100.0));
		TestTrue(TEXT("a placed pointer counts as over a window"), Slate.GetPlatformApplication()->IsCursorDirectlyOverSlateWindow());

		WoRAgentPlayHold::Release();
		WoRAgentPlayHold::Release(); // a second release changes nothing
		TestFalse(TEXT("released"), WoRAgentPlayHold::IsTaken());
		TestTrue(TEXT("the real application is back"), Slate.GetPlatformApplication() == Real);
		TestTrue(TEXT("the real cursor is back"), Slate.GetPlatformApplication()->Cursor == RealCursor);
		TestTrue(TEXT("the throttle is back on"), static_cast<bool>(Performance->bThrottleCPUWhenNotForeground));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
