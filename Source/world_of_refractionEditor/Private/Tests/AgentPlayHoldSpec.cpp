// AgentPlayHoldSpec.cpp
// B-573, headless: taking the hold swaps in the agent's pointer (resting over nothing) and turns the background
// throttle off; releasing puts the real application and the throttle back exactly; both are safe to repeat.
// Unproven here: hover, click and wheel in a PIE game while the real pointer moves (the brain's live check).

#if WITH_DEV_AUTOMATION_TESTS

#include "AgentPlay/AgentPlayHold.h"
#include "Application/SlateWindowHelper.h"
#include "Editor/EditorPerformanceSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/ICursor.h"
#include "Input/Events.h"
#include "Layout/WidgetPath.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"

namespace
{
	const FVector2D CLICK_TEST_WINDOW_SIZE(200.0, 100.0);
}

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

	// The click fix. Slate ignores a pressed button's ask to hold the pointer while the application is not the active
	// one (FSlateApplication::ProcessReply), and a button released without that hold does not click. The agent plays
	// with the editor in the background, so the hold has to switch that on.
	// Headless limit: the tool's own entry (ProcessMouseButtonDownEvent / UpEvent) finds its target by a hit test on a
	// shown, painted window, which a headless run has none of. So the path to the button is built by hand and the same
	// press and release are routed one step below that entry.
	It("fires a button pressed and released later while the editor is in the background", [this]()
	{
		if (!FSlateApplication::IsInitialized())
		{
			AddInfo(TEXT("no Slate application here: skipped"));
			return;
		}
		// The test puts the application in the background and takes the hold itself. In an editor a person or an agent
		// is using, that would close open menus and end a live hold, so it only runs unattended (Scripts/ue/test.sh).
		if (!FApp::IsUnattended() || WoRAgentPlayHold::IsTaken())
		{
			AddInfo(TEXT("an editor in use (not unattended, or the hold is taken): skipped"));
			return;
		}
		FSlateApplication &Slate = FSlateApplication::Get();
		int32 Clicks = 0;
		const TSharedRef<SButton> Button = SNew(SButton).OnClicked_Lambda([&Clicks]()
		{
			++Clicks;
			return FReply::Handled();
		});
		const TSharedRef<SWindow> Window = SNew(SWindow)
			.AutoCenter(EAutoCenter::None)
			.CreateTitleBar(false)
			.SizingRule(ESizingRule::FixedSize)
			.ClientSize(CLICK_TEST_WINDOW_SIZE)
			[
				Button
			]; // never shown or registered
		FWidgetPath Found;
		if (!TestTrue(TEXT("a path to the button"), FSlateWindowHelper::FindPathToWidget({Window}, Button, Found, EVisibility::All)))
		{
			return;
		}
		// A path a pointer event can be routed along carries a pointer position per widget, as a hit test's does.
		TArray<FWidgetAndPointer> UnderPointer;
		for (int32 Index = 0; Index < Found.Widgets.Num(); ++Index)
		{
			UnderPointer.Emplace(Found.Widgets[Index]);
		}
		const FWidgetPath Path(UnderPointer);

		const TSharedPtr<GenericApplication> Real = Slate.GetPlatformApplication();
		const void *RealCaptureBefore = Real.IsValid() ? Real->GetCapture() : nullptr;
		const bool bWasActive = Slate.IsActive();
		const bool bHandledInactiveBefore = Slate.GetHandleDeviceInputWhenApplicationNotActive();
		const TSharedPtr<SWidget> FocusedBefore = Slate.GetKeyboardFocusedWidget();
		const FGeometry &ButtonGeometry = Path.Widgets.Last().Geometry;
		const FVector2D Point = FVector2D(ButtonGeometry.GetAbsolutePosition()) + FVector2D(ButtonGeometry.GetAbsoluteSize()) * 0.5;
		const int32 MouseUser = Slate.GetUserIndexForMouse();
		const auto MakeEvent = [&Point, MouseUser](const TSet<FKey> &Held, const FKey &Changed)
		{
			return FPointerEvent(MouseUser, FSlateApplication::CursorPointerIndex, Point, Point, Held, Changed, 0.0f, FModifierKeysState());
		};
		ON_SCOPE_EXIT
		{
			Slate.ReleaseAllPointerCapture(); // a failed check between press and release leaves the button holding it
			Slate.RoutePointerMoveEvent(FWidgetPath(), MakeEvent({}, EKeys::Invalid), false); // off the button
			WoRAgentPlayHold::Release();
			Slate.ProcessApplicationActivationEvent(bWasActive);
			if (FocusedBefore.IsValid())
			{
				Slate.SetKeyboardFocus(FocusedBefore, EFocusCause::SetDirectly);
			}
			else
			{
				Slate.ClearKeyboardFocus(EFocusCause::SetDirectly);
			}
		};

		Slate.ProcessApplicationActivationEvent(false); // the person is working in another application
		WoRAgentPlayHold::Take();
		Slate.RoutePointerMoveEvent(Path, MakeEvent({}, EKeys::Invalid), false);
		TestTrue(TEXT("the button is hovered"), Button->IsHovered());
		Slate.RoutePointerDownEvent(Path, MakeEvent({EKeys::LeftMouseButton}, EKeys::LeftMouseButton));
		TestTrue(TEXT("the button is pressed"), Button->IsPressed());
		TestTrue(TEXT("the pressed button holds the agent's pointer"), Button->HasMouseCapture());
		TestEqual(TEXT("no click before the release"), Clicks, 0);
		Slate.RoutePointerUpEvent(Path, MakeEvent({}, EKeys::LeftMouseButton)); // a later frame, in the tool
		TestEqual(TEXT("the release clicks it, once"), Clicks, 1);
		TestFalse(TEXT("the button lets go of the pointer"), Button->HasMouseCapture());
		TestFalse(TEXT("the editor is still in the background"), Slate.IsActive());
		TestTrue(TEXT("the real mouse was not captured"), !Real.IsValid() || Real->GetCapture() == RealCaptureBefore);

		WoRAgentPlayHold::Release();
		TestEqual(TEXT("the release puts Slate's background-input switch back"), Slate.GetHandleDeviceInputWhenApplicationNotActive(), bHandledInactiveBefore);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
