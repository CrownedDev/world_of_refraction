// GameInputSpec.cpp
// B-578, headless: a menu-only input mode still on the game viewport when a level change starts is replaced by game
// input (UWoRGameInputSubsystem), for this game's viewport only.
// Unproven here: the real level change (OpenLevel after a fight's results screen) and keyboard focus, which need a
// viewport widget and a local player (docs/PIE/B-578.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Player/WoRGameInputSubsystem.h"
#include "WoRTestWorld.h"

namespace
{
	/** What FInputModeUIOnly::ApplyInputMode leaves on the viewport. Set by hand: the mode itself needs a viewport
	 *  widget, which a headless run has none of. */
	void LeaveMenuOnly(UGameViewportClient &Viewport)
	{
		Viewport.SetIgnoreInput(true);
		Viewport.SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
	}
}

BEGIN_DEFINE_SPEC(FGameInputSpec, "WoR.Input.LevelChange", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FGameInputSpec)

void FGameInputSpec::Define()
{
	It("gives game input back when a level change starts with a menu-only mode still on (a fight's results screen)", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("test world created"), TestWorld.Create())) return;
		UWoRGameInputSubsystem *GameInput = TestWorld.GetSubsystem<UWoRGameInputSubsystem>();
		if (!TestNotNull(TEXT("the game input subsystem"), GameInput)) return;
		FWorldContext &Context = GEngine->GetWorldContextFromWorldChecked(TestWorld.GetWorld());
		UGameViewportClient *Viewport = NewObject<UGameViewportClient>(GEngine);
		Context.GameViewport = Viewport;
		ON_SCOPE_EXIT { Context.GameViewport = nullptr; };

		LeaveMenuOnly(*Viewport);
		TestTrue(TEXT("the viewport ignores game input, as under a menu"), Viewport->IgnoreInput());

		TestTrue(TEXT("the level change found the menu-only mode"), GameInput->RestoreGameInput(Context));
		TestFalse(TEXT("the viewport takes game input again"), Viewport->IgnoreInput());
		TestTrue(TEXT("the mouse is the game's again"), Viewport->GetMouseCaptureMode() == EMouseCaptureMode::CapturePermanently);
		TestTrue(TEXT("and locks only while captured"), Viewport->GetMouseLockMode() == EMouseLockMode::LockOnCapture);

		TestFalse(TEXT("a level change with game input already on finds nothing"), GameInput->RestoreGameInput(Context));
		TestFalse(TEXT("and leaves game input on"), Viewport->IgnoreInput());
	});

	It("leaves another game's viewport alone, and does nothing with no viewport", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("test world created"), TestWorld.Create())) return;
		UWoRGameInputSubsystem *GameInput = TestWorld.GetSubsystem<UWoRGameInputSubsystem>();
		if (!TestNotNull(TEXT("the game input subsystem"), GameInput)) return;

		FWorldContext OtherGame; // a second PIE window's: not this subsystem's game instance
		UGameViewportClient *OtherViewport = NewObject<UGameViewportClient>(GEngine);
		OtherGame.GameViewport = OtherViewport;
		LeaveMenuOnly(*OtherViewport);
		TestFalse(TEXT("another game's level change is not ours"), GameInput->RestoreGameInput(OtherGame));
		TestTrue(TEXT("its viewport is as it was"), OtherViewport->IgnoreInput());

		const FWorldContext &Context = GEngine->GetWorldContextFromWorldChecked(TestWorld.GetWorld());
		TestNull(TEXT("no viewport here (as on a dedicated server)"), Context.GameViewport.Get());
		TestFalse(TEXT("nothing to give back"), GameInput->RestoreGameInput(Context));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
