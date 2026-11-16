// FrontEndSpec.cpp
// B-537 (D-232): the title is its own level. The menu level exists once the default map is not the hub; Play pressed
// straight in the hub or a trial with no save goes there; the game menu's Quit goes there outside a run. The travel
// itself and the title opening in the level are the PIE sheet (docs/PIE/B-537.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Player/WoRFrontEndGameMode.h"
#include "UI/Menus/PauseMenuModel.h"

namespace
{
	const FString FRONT_END_TEST_MENU = TEXT("/Game/Level/L_FrontEnd");
	const FString FRONT_END_TEST_HUB = TEXT("/Game/Level/TestLevel_Nav");
}

BEGIN_DEFINE_SPEC(FFrontEndSpec, "WoR.UI.FrontEnd", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FFrontEndSpec)

void FFrontEndSpec::Define()
{
	It("has a menu level only once the default map is not the hub", [this]()
	{
		TestFalse(TEXT("default map is the hub (before the ini step)"), WoRFrontEnd::HasFrontEnd(FRONT_END_TEST_HUB, FRONT_END_TEST_HUB));
		TestFalse(TEXT("no default map"), WoRFrontEnd::HasFrontEnd(FString(), FRONT_END_TEST_HUB));
		TestTrue(TEXT("default map is the menu level"), WoRFrontEnd::HasFrontEnd(FRONT_END_TEST_MENU, FRONT_END_TEST_HUB));
	});

	It("sends Play with no save and no leader to the menu level, never otherwise", [this]()
	{
		TestTrue(TEXT("no save, no leader"), WoRFrontEnd::ShouldFallBackToFrontEnd(false, false, true, false));
		TestFalse(TEXT("a save: its leader, or the class default as before"), WoRFrontEnd::ShouldFallBackToFrontEnd(true, false, true, false));
		TestFalse(TEXT("a leader (a new game not written yet)"), WoRFrontEnd::ShouldFallBackToFrontEnd(false, true, true, false));
		TestFalse(TEXT("no menu level yet"), WoRFrontEnd::ShouldFallBackToFrontEnd(false, false, false, false));
		TestFalse(TEXT("under tests"), WoRFrontEnd::ShouldFallBackToFrontEnd(false, false, true, true));
	});

	It("sends the game menu's Quit to the hub in a run, the title outside one, and closes with no menu level", [this]()
	{
		TestTrue(TEXT("in a run"), WoRFrontEnd::GetQuitTarget(true, true) == WoRFrontEnd::EQuitTo::Hub);
		TestTrue(TEXT("outside a run"), WoRFrontEnd::GetQuitTarget(false, true) == WoRFrontEnd::EQuitTo::FrontEnd);
		TestTrue(TEXT("no menu level yet"), WoRFrontEnd::GetQuitTarget(false, false) == WoRFrontEnd::EQuitTo::Desktop);
		TestFalse(TEXT("the warning says where it goes"),
				  WoRPauseMenu::GetQuitWarning(false, true).EqualTo(WoRPauseMenu::GetQuitWarning(false, false)));
	});
}

#endif
