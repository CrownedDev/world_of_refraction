// PauseMenuSpec.cpp
// B-439 (D-181, D-176): the game menu's model. Its nine pages in order, which ones open, the header in the hub
// and in a run, and Quit in a run: the run's end pays nothing. The screen itself is the PIE sheet (docs/PIE/B-439.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/CharacterData.h"
#include "Character/PlayerBuild.h"
#include "Trial/DefeatRules.h"
#include "Trial/RunRewards.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "UI/Menus/PauseMenuModel.h"
#include "UI/Menus/PauseMenuWidget.h"
#include "Currency/CurrencyTypes.h"
#include "WoRGameNames.h"
#include "WoRTestWorld.h"

namespace
{
	constexpr int32 PAUSE_TEST_SEED = 23;
	constexpr int32 PAUSE_TEST_PAGE_COUNT = 9; // B-536: Forms
	constexpr int32 PAUSE_TEST_CHIP_COUNT = 5; // B-543: the place's money, Diamond and the essences (B-550: Dormant, Elemental, Stat)
}

BEGIN_DEFINE_SPEC(FPauseMenuSpec, "WoR.UI.PauseMenu", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTrialRunSubsystem *Run = nullptr;

	void StartRun()
	{
		Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), PAUSE_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
	}

	UPauseMenuWidget *OpenMenu()
	{
		UPauseMenuWidget *Menu = CreateWidget<UPauseMenuWidget>(TestWorld->GetWorld(), UPauseMenuWidget::StaticClass());
		if (TestNotNull(TEXT("menu created"), Menu))
		{
			Menu->ActivateWidget();
			TestTrue(TEXT("activated"), Menu->IsActivated());
		}
		return Menu;
	}
END_DEFINE_SPEC(FPauseMenuSpec)

void FPauseMenuSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
	});
	AfterEach([this]()
	{
		Run = nullptr;
		TestWorld.Reset();
	});

	It("lists the nine pages in order (the mockup's, plus Forms), Resume first and Quit last", [this]()
	{
		const TArray<FPauseMenuPage> Pages = WoRPauseMenu::BuildPages(false, true);
		if (!TestEqual(TEXT("nine pages"), Pages.Num(), PAUSE_TEST_PAGE_COUNT))
		{
			return;
		}
		for (int32 Index = 0; Index < Pages.Num(); ++Index)
		{
			TestEqual(TEXT("in enum order"), static_cast<int32>(Pages[Index].Page), Index);
			TestFalse(TEXT("named"), Pages[Index].Label.IsEmpty());
			TestFalse(TEXT("previewed"), Pages[Index].Preview.IsEmpty());
		}
		TestTrue(TEXT("Resume opens"), Pages[static_cast<int32>(EPauseMenuPage::Resume)].CanOpen());
		const FText Tracked = FText::FromString(TEXT("A quest: its step"));
		TestEqual(TEXT("B-463b: the Journal row previews the tracked quest"),
				  WoRPauseMenu::BuildPages(false, true, Tracked)[static_cast<int32>(EPauseMenuPage::Journal)].Preview.ToString(),
				  FString(TEXT("Tracking: A quest: its step")));
		TestTrue(TEXT("Inventory opens (its placeholder)"), Pages[static_cast<int32>(EPauseMenuPage::Inventory)].CanOpen());
		TestTrue(TEXT("Quit opens"), Pages[static_cast<int32>(EPauseMenuPage::Quit)].CanOpen());
		TestTrue(TEXT("Loadout opens (B-440a)"), Pages[static_cast<int32>(EPauseMenuPage::Loadout)].CanOpen());
		TestTrue(TEXT("Character opens (B-442)"), Pages[static_cast<int32>(EPauseMenuPage::Character)].CanOpen());
		TestTrue(TEXT("Settings opens (B-452)"), Pages[static_cast<int32>(EPauseMenuPage::Settings)].CanOpen());
		TestTrue(TEXT("Forms opens (B-536)"), Pages[static_cast<int32>(EPauseMenuPage::Forms)].CanOpen());
	});

	It("refuses Quit to a guest in a run, and only then", [this]()
	{
		const int32 Quit = static_cast<int32>(EPauseMenuPage::Quit);
		TestFalse(TEXT("guest in a run"), WoRPauseMenu::BuildPages(true, false)[Quit].CanOpen());
		TestTrue(TEXT("host in a run"), WoRPauseMenu::BuildPages(true, true)[Quit].CanOpen());
		TestTrue(TEXT("guest in the hub"), WoRPauseMenu::BuildPages(false, false)[Quit].CanOpen());
	});

	It("warns that quitting a run loses everything (D-176)", [this]()
	{
		const FText InRun = WoRPauseMenu::GetQuitWarning(true);
		TestFalse(TEXT("a warning in a run"), InRun.IsEmpty());
		TestFalse(TEXT("not the hub's line"), InRun.EqualTo(WoRPauseMenu::GetQuitWarning(false)));
		TestTrue(TEXT("Quit's preview is the warning"), WoRPauseMenu::BuildPages(true, true)[static_cast<int32>(EPauseMenuPage::Quit)].Preview.EqualTo(InRun));
	});

	It("keeps Spirit and Body off the party list and on the Character sheet (D-254)", [this]()
	{
		UPackage *Package = GetTransientPackage();
		UCharacterData *Player = NewObject<UCharacterData>(
			Package, MakeUniqueObjectName(Package, UCharacterData::StaticClass(), *(FString(WoRPlayerBuild::BUILT_NAME) + TEXT("_PartyListSpec"))));
		Player->CharacterClass = ECharacterClass::Caster;
		Player->InnateElement = ESpellElement::Fire;
		UCharacterData *Recruit = NewObject<UCharacterData>(Package);
		Recruit->CharacterClass = ECharacterClass::Caster;
		Recruit->InnateElement = ESpellElement::Water;
		UCharacterData *NoElement = NewObject<UCharacterData>(Package);
		NoElement->CharacterClass = ECharacterClass::Generic;
		if (!TestTrue(TEXT("a player character and two that are not"), WoRPlayerBuild::IsPlayerCharacter(Player) &&
																		 !WoRPlayerBuild::IsPlayerCharacter(Recruit) && !WoRPlayerBuild::IsPlayerCharacter(NoElement)))
		{
			return;
		}
		const FString Identity = WoRPlayerBuild::GetDetailLine(Player, FGuid(), nullptr).ToString();
		const FString ListLine = WoRPauseMenu::GetClassLine(Player).ToString();
		TestTrue(TEXT("the identity reads Spirit of ..."), Identity.StartsWith(TEXT("Spirit of")));
		TestFalse(TEXT("party list: no Spirit or Body for a player character"), ListLine.Contains(TEXT("Spirit of")) || ListLine.Contains(TEXT("Body of")));
		TestEqual(TEXT("party list: its element and class, as for anyone"), ListLine,
				  FText::Format(INVTEXT("{0} {1}"), WoRGameNames::GetElementName(ESpellElement::Fire),
								WoRGameNames::GetCharacterClassName(ECharacterClass::Caster)).ToString());
		TestEqual(TEXT("party list: the class alone with no innate element"), WoRPauseMenu::GetClassLine(NoElement).ToString(),
				  WoRGameNames::GetCharacterClassName(ECharacterClass::Generic).ToString());
		TestEqual(TEXT("Character sheet: a player character's Spirit / Body lines"), WoRPauseMenu::GetSheetLine(Player, FGuid(), nullptr).ToString(), Identity);
		TestEqual(TEXT("Character sheet: anyone else's class line"), WoRPauseMenu::GetSheetLine(Recruit, FGuid(), nullptr).ToString(),
				  WoRPauseMenu::GetClassLine(Recruit).ToString());
		TestTrue(TEXT("no character: nothing"), WoRPauseMenu::GetClassLine(nullptr).IsEmpty() && WoRPauseMenu::GetSheetLine(nullptr, FGuid(), nullptr).IsEmpty());
	});

	It("shows the hub outside a run and the run's wipes left inside one", [this]()
	{
		if (!TestNotNull(TEXT("run"), Run))
		{
			return;
		}
		const FPauseMenuHeader Hub = WoRPauseMenu::BuildHeader(TestWorld->GetWorld());
		TestFalse(TEXT("hub: not in a run"), Hub.bInRun);
		TestEqual(TEXT("hub: The Hub, once (D-236)"), Hub.Title.ToString(), FString(TEXT("The Hub")));
		TestTrue(TEXT("hub: no subtitle"), Hub.Subtitle.IsEmpty());
		TestEqual(TEXT("hub: every balance"), Hub.Chips.Num(), PAUSE_TEST_CHIP_COUNT);
		TestTrue(TEXT("hub: Gold first"), Hub.Chips.Num() > 0 && Hub.Chips[0].Label.EqualTo(WoRGameNames::GetCurrencyName(ECurrencyType::Prisms)));

		StartRun();
		Run->RecordWipe(EEncounterRank::Normal);
		const FPauseMenuHeader InRun = WoRPauseMenu::BuildHeader(TestWorld->GetWorld());
		TestTrue(TEXT("run: in a run"), InRun.bInRun);
		TestTrue(TEXT("run: Prisms first"), InRun.Chips.Num() > 0 && InRun.Chips[0].Label.EqualTo(WoRGameNames::GetCurrencyName(ECurrencyType::Gold)));
		if (TestEqual(TEXT("run: every balance and the wipes left"), InRun.Chips.Num(), PAUSE_TEST_CHIP_COUNT + 1))
		{
			const FString WipesLeft = InRun.Chips.Last().Value.ToString();
			TestTrue(TEXT("one wipe used"), WipesLeft.StartsWith(FString::FromInt(DefeatRules::MAX_WIPES_PER_RUN - 1)));
		}
		TestFalse(TEXT("the title differs from the hub's"), InRun.Title.EqualTo(Hub.Title));
	});

	It("pays nothing for a run that was quit (D-176)", [this]()
	{
		if (!TestNotNull(TEXT("run"), Run))
		{
			return;
		}
		StartRun();
		Run->RecordChestReward();
		TestFalse(TEXT("something earned"), Run->GetRunEndPayout().IsEmpty());
		Run->AbandonRun(); // the hub travel is next tick; the world isn't ticked here
		if (TestTrue(TEXT("quit"), Run->IsAbandoned()))
		{
			TestTrue(TEXT("the end pays nothing"), Run->GetRunEndPayout().IsEmpty());
		}
	});

	// The screen, headless: no widget tree is built, so these check what the keys do, not what is drawn.
	Describe("the screen", [this]()
	{
		It("starts on Resume, wraps at both ends, and Resume closes it", [this]()
		{
			UPauseMenuWidget *Menu = OpenMenu();
			if (!Menu)
			{
				return;
			}
			TestEqual(TEXT("starts on Resume"), Menu->GetSelectedIndex(), static_cast<int32>(EPauseMenuPage::Resume));
			Menu->MoveSelection(-1);
			TestEqual(TEXT("up from the top: Quit"), Menu->GetSelectedIndex(), static_cast<int32>(EPauseMenuPage::Quit));
			Menu->MoveSelection(1);
			TestEqual(TEXT("down from the bottom: Resume"), Menu->GetSelectedIndex(), static_cast<int32>(EPauseMenuPage::Resume));
			Menu->PressSelected();
			TestFalse(TEXT("Resume closes the menu"), Menu->IsActivated());
		});

		It("hides the page descriptions until Help, and Help toggles them (B-543)", [this]()
		{
			UPauseMenuWidget *Menu = OpenMenu();
			if (!Menu)
			{
				return;
			}
			TestFalse(TEXT("off at first"), Menu->IsHelpShown());
			Menu->ToggleHelp();
			TestTrue(TEXT("Help shows them"), Menu->IsHelpShown());
			Menu->ToggleHelp();
			TestFalse(TEXT("Help again hides them"), Menu->IsHelpShown());
		});

		It("asks before quitting, holds only from the second press, and stops when let go (D-176)", [this]()
		{
			UPauseMenuWidget *Menu = OpenMenu();
			if (!Menu)
			{
				return;
			}
			Menu->SelectIndex(static_cast<int32>(EPauseMenuPage::Quit));
			Menu->PressSelected();
			TestTrue(TEXT("first press: the question"), Menu->IsConfirmingQuit());
			TestFalse(TEXT("no hold yet"), Menu->IsHoldingQuit());
			Menu->ReleaseSelected();
			Menu->PressSelected();
			TestTrue(TEXT("second press: the hold starts"), Menu->IsHoldingQuit());
			Menu->ReleaseSelected();
			TestFalse(TEXT("let go: the hold stops"), Menu->IsHoldingQuit());
			TestTrue(TEXT("the menu is still open"), Menu->IsActivated());
		});

		It("leaves the quit question on Back, then closes on the next Back", [this]()
		{
			UPauseMenuWidget *Menu = OpenMenu();
			if (!Menu)
			{
				return;
			}
			Menu->SelectIndex(static_cast<int32>(EPauseMenuPage::Quit));
			Menu->PressSelected();
			Menu->PressBack();
			TestFalse(TEXT("Back leaves the question"), Menu->IsConfirmingQuit());
			TestTrue(TEXT("and keeps the menu"), Menu->IsActivated());
			Menu->PressBack();
			TestFalse(TEXT("Back again closes it"), Menu->IsActivated());
		});

		It("drops the quit question when the selection moves", [this]()
		{
			UPauseMenuWidget *Menu = OpenMenu();
			if (!Menu)
			{
				return;
			}
			Menu->SelectIndex(static_cast<int32>(EPauseMenuPage::Quit));
			Menu->PressSelected();
			Menu->MoveSelection(-1);
			TestFalse(TEXT("moved away: no question"), Menu->IsConfirmingQuit());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
