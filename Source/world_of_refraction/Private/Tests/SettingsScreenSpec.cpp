// SettingsScreenSpec.cpp
// B-452 (D-190, D-181): the settings screen's model. Six pages; Gameplay and Accessibility list every approved row;
// Turn View and the End Turn button change and read back; a row whose setting isn't built says so and doesn't
// change; the Controls page lists the built keys by section. The screen itself is the PIE sheet (docs/PIE/B-452.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Combat/Camera/CombatCameraTypes.h"
#include "Save/SaveSubsystem.h"
#include "UI/Menus/SettingsScreenModel.h"
#include "UI/Menus/SettingsScreenWidget.h"
#include "WoRTestWorld.h"

namespace
{
	constexpr int32 SETTINGS_TEST_GAMEPLAY_ROWS = 7;
	constexpr int32 SETTINGS_TEST_ACCESSIBILITY_ROWS = 7;
	constexpr int32 SETTINGS_TEST_KEY_SECTIONS = 3;
	constexpr int32 SETTINGS_TEST_TURN_VIEW_ROW = 1;
	constexpr int32 SETTINGS_TEST_END_TURN_ROW = 2;
	constexpr int32 SETTINGS_TEST_UNBUILT_ROW = 0;
	/** B-453a: Accessibility's rows: battle controls, text size, camera movement, then screen shake. */
	constexpr int32 SETTINGS_TEST_SCREEN_SHAKE_ROW = 3;
}

BEGIN_DEFINE_SPEC(FSettingsScreenSpec, "WoR.UI.Settings", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	TArray<FSettingsRow> Gameplay() const { return WoRSettingsScreen::BuildRows(TestWorld->GetWorld(), ESettingsPage::Gameplay); }
END_DEFINE_SPEC(FSettingsScreenSpec)

void FSettingsScreenSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
	});
	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("has six named pages; Gameplay and Accessibility list every approved row", [this]()
	{
		for (int32 Index = 0; Index < WoRSettingsScreen::PAGE_COUNT; ++Index)
		{
			TestFalse(TEXT("a page is named"), WoRSettingsScreen::GetPageName(static_cast<ESettingsPage>(Index)).IsEmpty());
		}
		TestEqual(TEXT("Gameplay rows"), Gameplay().Num(), SETTINGS_TEST_GAMEPLAY_ROWS);
		const TArray<FSettingsRow> Accessibility = WoRSettingsScreen::BuildRows(TestWorld->GetWorld(), ESettingsPage::Accessibility);
		TestEqual(TEXT("Accessibility rows"), Accessibility.Num(), SETTINGS_TEST_ACCESSIBILITY_ROWS);
		for (const FSettingsRow &Row : Accessibility)
		{
			TestFalse(TEXT("named"), Row.Label.IsEmpty());
			TestFalse(TEXT("explained"), Row.Explanation.IsEmpty());
		}
		TestEqual(TEXT("the battle controls row is on both pages"), static_cast<int32>(Accessibility[0].Id), static_cast<int32>(Gameplay()[0].Id));
		for (const ESettingsPage Empty : {ESettingsPage::Camera, ESettingsPage::Display, ESettingsPage::Audio})
		{
			TestEqual(TEXT("no rows yet"), WoRSettingsScreen::BuildRows(TestWorld->GetWorld(), Empty).Num(), 0);
			TestFalse(TEXT("and the page says so"), WoRSettingsScreen::GetPageNote(Empty).IsEmpty());
		}
	});

	It("steps the Turn View through its styles and back, and reads it back", [this]()
	{
		UWorld *World = TestWorld->GetWorld();
		const FString First = Gameplay()[SETTINGS_TEST_TURN_VIEW_ROW].Value.ToString();
		TestTrue(TEXT("it can change"), Gameplay()[SETTINGS_TEST_TURN_VIEW_ROW].CanChange());
		TestTrue(TEXT("changed"), WoRSettingsScreen::ChangeSetting(World, ESettingId::TurnView, 1));
		TestNotEqual(TEXT("another style"), Gameplay()[SETTINGS_TEST_TURN_VIEW_ROW].Value.ToString(), First);
		TestTrue(TEXT("back"), WoRSettingsScreen::ChangeSetting(World, ESettingId::TurnView, -1));
		TestEqual(TEXT("the first style again"), Gameplay()[SETTINGS_TEST_TURN_VIEW_ROW].Value.ToString(), First);
		const int32 Styles = static_cast<int32>(StaticEnum<ETurnViewStyle>()->NumEnums()) - 1;
		for (int32 Step = 0; Step < Styles; ++Step)
		{
			WoRSettingsScreen::ChangeSetting(World, ESettingId::TurnView, 1);
		}
		TestEqual(TEXT("a full circle"), Gameplay()[SETTINGS_TEST_TURN_VIEW_ROW].Value.ToString(), First);
	});

	It("toggles the End Turn button and the setting follows", [this]()
	{
		UWorld *World = TestWorld->GetWorld();
		const USaveSubsystem *Save = TestWorld->GetSubsystem<USaveSubsystem>();
		if (!TestNotNull(TEXT("save"), Save))
		{
			return;
		}
		const bool bHiddenBefore = Save->IsEndTurnButtonHidden();
		const FString Before = Gameplay()[SETTINGS_TEST_END_TURN_ROW].Value.ToString();
		TestTrue(TEXT("changed"), WoRSettingsScreen::ChangeSetting(World, ESettingId::EndTurnButton, 1));
		TestEqual(TEXT("the setting flipped"), Save->IsEndTurnButtonHidden(), !bHiddenBefore);
		TestNotEqual(TEXT("the row reads the other value"), Gameplay()[SETTINGS_TEST_END_TURN_ROW].Value.ToString(), Before);
	});

	It("steps the screen shake Full, Reduced, Off and round, and the fight camera scales by it (B-453a)", [this]()
	{
		UWorld *World = TestWorld->GetWorld();
		USaveSubsystem *Save = TestWorld->GetSubsystem<USaveSubsystem>();
		const TArray<FSettingsRow> Before = WoRSettingsScreen::BuildRows(World, ESettingsPage::Accessibility);
		if (!TestNotNull(TEXT("save"), Save) || !TestTrue(TEXT("the row"), Before.IsValidIndex(SETTINGS_TEST_SCREEN_SHAKE_ROW)))
		{
			return;
		}
		const FSettingsRow &Row = Before[SETTINGS_TEST_SCREEN_SHAKE_ROW];
		TestTrue(TEXT("the screen shake row"), Row.Id == ESettingId::ScreenShake);
		TestTrue(TEXT("it can change"), Row.CanChange());
		Save->SetScreenShakeLevel(EScreenShakeLevel::Full);
		TestTrue(TEXT("changed"), WoRSettingsScreen::ChangeSetting(World, ESettingId::ScreenShake, 1));
		TestTrue(TEXT("reduced"), Save->GetScreenShakeLevel() == EScreenShakeLevel::Reduced);
		WoRSettingsScreen::ChangeSetting(World, ESettingId::ScreenShake, 1);
		TestTrue(TEXT("off"), Save->GetScreenShakeLevel() == EScreenShakeLevel::Off);
		WoRSettingsScreen::ChangeSetting(World, ESettingId::ScreenShake, 1);
		TestTrue(TEXT("round to full"), Save->GetScreenShakeLevel() == EScreenShakeLevel::Full);
		WoRSettingsScreen::ChangeSetting(World, ESettingId::ScreenShake, -1);
		TestTrue(TEXT("back to off"), Save->GetScreenShakeLevel() == EScreenShakeLevel::Off);
		TestEqual(TEXT("the row reads it"), WoRSettingsScreen::BuildRows(World, ESettingsPage::Accessibility)[SETTINGS_TEST_SCREEN_SHAKE_ROW].Value.ToString(),
				  FCombatCameraRules::GetScreenShakeLevelName(EScreenShakeLevel::Off).ToString());
		TestEqual(TEXT("off shakes nothing"), FCombatCameraRules::GetScreenShakeScale(EScreenShakeLevel::Off), 0.0f);
		TestEqual(TEXT("full plays the shot's shake"), FCombatCameraRules::GetScreenShakeScale(EScreenShakeLevel::Full), 1.0f);
		const float Reduced = FCombatCameraRules::GetScreenShakeScale(EScreenShakeLevel::Reduced);
		TestTrue(TEXT("reduced is less, not none"), Reduced > 0.0f && Reduced < 1.0f);
		Save->SetScreenShakeLevel(EScreenShakeLevel::Full); // the test world's save is shared; leave the default
	});

	It("says a setting isn't built yet, and doesn't change it", [this]()
	{
		const FSettingsRow Row = Gameplay()[SETTINGS_TEST_UNBUILT_ROW];
		TestFalse(TEXT("not changeable"), Row.CanChange());
		TestFalse(TEXT("with the reason"), Row.Refusal.IsEmpty());
		TestFalse(TEXT("ChangeSetting refuses"), WoRSettingsScreen::ChangeSetting(TestWorld->GetWorld(), Row.Id, 1));
	});

	It("lists the built keys by section, each with a key on both devices", [this]()
	{
		int32 Headings = 0;
		for (const FSettingsKeyRow &Row : WoRSettingsScreen::BuildKeyRows())
		{
			if (Row.IsHeading())
			{
				++Headings;
				TestFalse(TEXT("a section is named"), Row.Section.IsEmpty());
				continue;
			}
			TestFalse(TEXT("keyboard key"), Row.Keyboard.IsEmpty());
			TestFalse(TEXT("gamepad key"), Row.Gamepad.IsEmpty());
		}
		TestEqual(TEXT("Field, Battle, Menus"), Headings, SETTINGS_TEST_KEY_SECTIONS);
		TestFalse(TEXT("the Controls page says keys are fixed"), WoRSettingsScreen::GetPageNote(ESettingsPage::Controls).IsEmpty());
	});

	It("opens on Gameplay, wraps its pages, and marks a change to save (the screen, headless)", [this]()
	{
		USettingsScreenWidget *Screen = CreateWidget<USettingsScreenWidget>(TestWorld->GetWorld(), USettingsScreenWidget::StaticClass());
		if (!TestNotNull(TEXT("screen created"), Screen))
		{
			return;
		}
		Screen->ActivateWidget();
		TestTrue(TEXT("on Gameplay"), Screen->GetPage() == ESettingsPage::Gameplay);
		Screen->MovePage(-1);
		TestTrue(TEXT("back from the first page: the last"), Screen->GetPage() == ESettingsPage::Accessibility);
		Screen->MovePage(1);
		TestTrue(TEXT("and forward again"), Screen->GetPage() == ESettingsPage::Gameplay);

		Screen->SelectRow(SETTINGS_TEST_UNBUILT_ROW);
		Screen->ChangeSelected(1);
		TestFalse(TEXT("an unbuilt row changes nothing"), Screen->HasUnsavedChanges());
		Screen->SelectRow(SETTINGS_TEST_TURN_VIEW_ROW);
		Screen->ChangeSelected(1);
		TestTrue(TEXT("a real change is marked"), Screen->HasUnsavedChanges());
		Screen->PressBack();
		TestFalse(TEXT("Back closes it"), Screen->IsActivated());
		TestFalse(TEXT("and the change was handed to the save"), Screen->HasUnsavedChanges());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
