// WoRActivatableWidget.cpp

#include "UI/WoRActivatableWidget.h"

#include "Engine/EngineBaseTypes.h"

TOptional<FUIInputConfig> UWoRActivatableWidget::GetDesiredInputConfig() const
{
	// UE 5.8 signature (Input/UIActionBindingHandle.h:105):
	//   FUIInputConfig(ECommonInputMode InInputMode,
	//                  EMouseCaptureMode InMouseCaptureMode,
	//                  EMouseLockMode InMouseLockMode,
	//                  bool bInHideCursorDuringViewportCapture = true);
	//
	// The four-argument overload is used deliberately over the three-argument
	// one so the lock mode is stated rather than defaulted — a cursor that is
	// visible but locked to the viewport is a different experience, and
	// defaulting it would make that an accident rather than a decision.
	//
	// Menu     — UI receives input; the world keeps TICKING (nothing here pauses,
	//            and CommonUI has no pause concept). Switch to ECommonInputMode::All
	//            in an override if a screen should let the player move while open;
	//            that also re-opens the "walk to a merchant with the menu up" case,
	//            so it is a per-screen decision, not a default.
	// NoCapture — the cursor stays free of the viewport, which is what makes it
	//            usable for clicking menu widgets.
	// DoNotLock — cursor may leave the viewport bounds; correct for windowed play.
	// false     — do NOT hide the cursor. The parameter is named for the capture
	//            case, but leaving it at its `true` default is the usual reason a
	//            menu opens with no visible cursor.
	return FUIInputConfig(ECommonInputMode::Menu,
						  EMouseCaptureMode::NoCapture,
						  EMouseLockMode::DoNotLock,
						  /*bHideCursorDuringViewportCapture=*/false);
}
