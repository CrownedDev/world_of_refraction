// AgentPlayHold.cpp

#include "AgentPlay/AgentPlayHold.h"
#include "Editor.h"
#include "Editor/EditorPerformanceSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/ICursor.h"
#include "Input/Events.h"

DEFINE_LOG_CATEGORY_STATIC(LogWoRAgentPlayHold, Log, All);

namespace
{
	/** Where the agent's pointer rests: over no window on any monitor (the place Slate's own stand-in cursor starts at). */
	const FVector2D PARKED_POSITION(MIN_int32, MIN_int32);
	constexpr int32 CURSOR_SIZE = 32;

	/** The agent's pointer: a position and nothing else. It never shows, changes shape or traps the real one. */
	class FAgentPlayCursor : public ICursor
	{
	public:
		virtual FVector2D GetPosition() const override { return Position; }
		virtual void SetPosition(const int32 X, const int32 Y) override { Position = FVector2D(X, Y); }
		virtual void SetType(const EMouseCursor::Type InNewCursor) override {}
		virtual EMouseCursor::Type GetType() const override { return EMouseCursor::Default; }
		virtual void GetSize(int32 &Width, int32 &Height) const override { Width = Height = CURSOR_SIZE; }
		virtual void Show(bool bShow) override {}
		virtual void Lock(const RECT *const Bounds) override {}
		virtual void SetTypeShape(EMouseCursor::Type InCursorType, void *CursorHandle) override {}

	private:
		FVector2D Position = PARKED_POSITION;
	};

	/** The platform application with the agent's pointer in place of the real one; everything else is the real
	 *  application's. The engine's Pixel Streaming input does the same for a remote player's mouse. */
	class FAgentPlayApplication : public GenericApplication
	{
	public:
		explicit FAgentPlayApplication(const TSharedRef<GenericApplication> &InReal)
			: GenericApplication(MakeShared<FAgentPlayCursor>()), Real(InReal)
		{
			MessageHandler = InReal->GetMessageHandler();
		}

		// The two answers that are the agent's: a synthesized move reads the pointer as over the game once it is put there.
		virtual bool IsCursorDirectlyOverSlateWindow() const override { return FVector2D(Cursor->GetPosition()) != PARKED_POSITION; }
		virtual TSharedPtr<FGenericWindow> GetWindowUnderCursor() override { return nullptr; } // found by the point, never by the real pointer

		virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler> &InMessageHandler) override
		{
			MessageHandler = InMessageHandler;
			Real->SetMessageHandler(InMessageHandler);
		}
#if WITH_ACCESSIBILITY
		virtual void SetAccessibleMessageHandler(const TSharedRef<FGenericAccessibleMessageHandler> &InHandler) override { Real->SetAccessibleMessageHandler(InHandler); }
#endif
		virtual void PollGameDeviceState(const float TimeDelta) override { Real->PollGameDeviceState(TimeDelta); }
		virtual void PumpMessages(const float TimeDelta) override { Real->PumpMessages(TimeDelta); }
		virtual void ProcessDeferredEvents(const float TimeDelta) override { Real->ProcessDeferredEvents(TimeDelta); }
		virtual void Tick(const float TimeDelta) override { Real->Tick(TimeDelta); }
		virtual TSharedRef<FGenericWindow> MakeWindow() override { return Real->MakeWindow(); }
		virtual void InitializeWindow(const TSharedRef<FGenericWindow> &Window, const TSharedRef<FGenericWindowDefinition> &InDefinition, const TSharedPtr<FGenericWindow> &InParent,
									  const bool bShowImmediately) override
		{
			Real->InitializeWindow(Window, InDefinition, InParent, bShowImmediately);
		}
		virtual void SetCapture(const TSharedPtr<FGenericWindow> &InWindow) override { Real->SetCapture(InWindow); }
		virtual void *GetCapture() const override { return Real->GetCapture(); }
		virtual FModifierKeysState GetModifierKeys() const override { return Real->GetModifierKeys(); }
		virtual bool IsMinimized() const override { return Real->IsMinimized(); }
		virtual void SetHighPrecisionMouseMode(const bool Enable, const TSharedPtr<FGenericWindow> &InWindow) override { Real->SetHighPrecisionMouseMode(Enable, InWindow); }
		virtual bool IsUsingHighPrecisionMouseMode() const override { return Real->IsUsingHighPrecisionMouseMode(); }
		virtual bool IsUsingTrackpad() const override { return Real->IsUsingTrackpad(); }
		virtual bool IsMouseAttached() const override { return Real->IsMouseAttached(); }
		virtual bool IsGamepadAttached() const override { return Real->IsGamepadAttached(); }
		virtual void RegisterConsoleCommandListener(const FOnConsoleCommandListener &InListener) override { Real->RegisterConsoleCommandListener(InListener); }
		virtual void AddPendingConsoleCommand(const FString &InCommand) override { Real->AddPendingConsoleCommand(InCommand); }
		virtual FPlatformRect GetWorkArea(const FPlatformRect &CurrentWindow) const override { return Real->GetWorkArea(CurrentWindow); }
		virtual bool TryCalculatePopupWindowPosition(const FPlatformRect &InAnchor, const FVector2D &InSize, const FVector2D &ProposedPlacement,
													 const EPopUpOrientation::Type Orientation, FVector2D *const CalculatedPopUpPosition) const override
		{
			return Real->TryCalculatePopupWindowPosition(InAnchor, InSize, ProposedPlacement, Orientation, CalculatedPopUpPosition);
		}
		virtual void GetInitialDisplayMetrics(FDisplayMetrics &OutDisplayMetrics) const override { Real->GetInitialDisplayMetrics(OutDisplayMetrics); }
		virtual EWindowTitleAlignment::Type GetWindowTitleAlignment() const override { return Real->GetWindowTitleAlignment(); }
		virtual EWindowTransparency GetWindowTransparencySupport() const override { return Real->GetWindowTransparencySupport(); }
		virtual void DestroyApplication() override { Real->DestroyApplication(); }
		virtual IInputInterface *GetInputInterface() override { return Real->GetInputInterface(); }
		virtual ITextInputMethodSystem *GetTextInputMethodSystem() override { return Real->GetTextInputMethodSystem(); }
		virtual void SendAnalytics(IAnalyticsProvider *Provider) override { Real->SendAnalytics(Provider); }
		virtual bool SupportsSystemHelp() const override { return Real->SupportsSystemHelp(); }
		virtual bool RequiresPerWindowTooltips() const override { return Real->RequiresPerWindowTooltips(); }
		virtual void ShowSystemHelp() override { Real->ShowSystemHelp(); }
		virtual bool ApplicationLicenseValid(FPlatformUserId PlatformUser = PLATFORMUSERID_NONE) override { return Real->ApplicationLicenseValid(PlatformUser); }
		virtual bool IsAllowedToRender() const override { return Real->IsAllowedToRender(); }
		virtual void FinishedInputThisFrame() override { Real->FinishedInputThisFrame(); }

		const TSharedRef<GenericApplication> Real;
	};

	TSharedPtr<FAgentPlayApplication> GApplication;
	bool bGThrottledBefore = false;
	FDelegateHandle GEndPIEHandle;
}

void WoRAgentPlayHold::Take()
{
	if (GApplication.IsValid() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	FSlateApplication &Slate = FSlateApplication::Get();
	const TSharedPtr<GenericApplication> Real = Slate.GetPlatformApplication();
	if (!Real.IsValid())
	{
		return;
	}
	GApplication = MakeShared<FAgentPlayApplication>(Real.ToSharedRef());
	Slate.OverridePlatformApplication(GApplication);
	// One move at the resting place: whatever the real pointer was over in the game lets go of it.
	Slate.ProcessMouseMoveEvent(
		FPointerEvent(Slate.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, PARKED_POSITION, PARKED_POSITION, TSet<FKey>(), EKeys::Invalid, 0.0f, FModifierKeysState()));

	UEditorPerformanceSettings *Performance = GetMutableDefault<UEditorPerformanceSettings>();
	bGThrottledBefore = Performance->bThrottleCPUWhenNotForeground;
	Performance->bThrottleCPUWhenNotForeground = false; // in memory only: a background editor at 3 fps can't time an input

	GEndPIEHandle = FEditorDelegates::EndPIE.AddLambda([](bool) { WoRAgentPlayHold::Release(); });
	UE_LOG(LogWoRAgentPlayHold, Log, TEXT("[AgentPlay] hold taken: the agent has its own pointer, the background throttle is off (it was %s)"), bGThrottledBefore ? TEXT("on") : TEXT("off"));
}

void WoRAgentPlayHold::Release()
{
	if (!GApplication.IsValid())
	{
		return;
	}
	FEditorDelegates::EndPIE.Remove(GEndPIEHandle);
	GetMutableDefault<UEditorPerformanceSettings>()->bThrottleCPUWhenNotForeground = bGThrottledBefore;
	// Only our own stand-in is taken out: if something wrapped the application after us, it stays as it is.
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetPlatformApplication() == GApplication)
	{
		FSlateApplication::Get().OverridePlatformApplication(GApplication->Real);
	}
	GApplication.Reset();
	UE_LOG(LogWoRAgentPlayHold, Log, TEXT("[AgentPlay] hold released: the real pointer and the background throttle are as they were"));
}

bool WoRAgentPlayHold::IsTaken()
{
	return GApplication.IsValid();
}

bool WoRAgentPlayHold::IsPointerParked()
{
	return !GApplication.IsValid() || FVector2D(GApplication->Cursor->GetPosition()) == PARKED_POSITION;
}
