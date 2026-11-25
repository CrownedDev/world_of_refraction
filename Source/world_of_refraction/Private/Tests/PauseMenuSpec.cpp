// PauseMenuSpec.cpp
// B-439 (D-181, D-176), B-632 (mockup menus/pause/v2): the game menu's model. Its seven pages in order, which ones
// open, the header in the hub and in a run, This run's counts and time, and Quit in a run: the run's end pays nothing.
// The screen itself is the PIE sheet (docs/PIE/B-632.md).

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
	constexpr int32 PAUSE_TEST_CHIP_COUNT = 5; // B-543: the place's money, Diamond and the essences (B-550: Dormant, Elemental, Stat)
	/** B-632 (mockup menus/pause/v2): the pages, in order; no Character or Body row. */
	const EPauseMenuPage PAUSE_TEST_ORDER[] = {EPauseMenuPage::Resume, EPauseMenuPage::Loadout, EPauseMenuPage::Inventory, EPauseMenuPage::Party,
											   EPauseMenuPage::Journal, EPauseMenuPage::Settings, EPauseMenuPage::Quit};
	constexpr int32 PAUSE_TEST_ITEMS_SAVED = 4;
	constexpr double PAUSE_TEST_SECONDS_SAVED = 125.0;
	constexpr double PAUSE_TEST_MINUTES_ONLY = 1122.0; // 18:42
	constexpr double PAUSE_TEST_PAST_AN_HOUR = 3909.0; // 1:05:09

	int32 PauseTestIndexOf(const TArray<FPauseMenuPage> &Pages, EPauseMenuPage Page)
	{
		return Pages.IndexOfByPredicate([Page](const FPauseMenuPage &Row) { return Row.Page == Page; });
	}
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

	It("lists the seven pages in order (B-632, mockup v2), Resume first and Quit last, each with its short line", [this]()
	{
		const TArray<FPauseMenuPage> Pages = WoRPauseMenu::BuildPages(false, true);
		if (!TestEqual(TEXT("seven pages"), Pages.Num(), static_cast<int32>(UE_ARRAY_COUNT(PAUSE_TEST_ORDER))))
		{
			return;
		}
		for (int32 Index = 0; Index < Pages.Num(); ++Index)
		{
			TestTrue(TEXT("in the mockup's order"), Pages[Index].Page == PAUSE_TEST_ORDER[Index]);
			TestFalse(TEXT("named"), Pages[Index].Label.IsEmpty());
			TestFalse(TEXT("a short line under the name"), Pages[Index].SubLine.IsEmpty());
			TestFalse(TEXT("previewed"), Pages[Index].Preview.IsEmpty());
			TestTrue(TEXT("opens"), Pages[Index].CanOpen());
		}
		TestEqual(TEXT("no Character row"), PauseTestIndexOf(Pages, EPauseMenuPage::Character), INDEX_NONE);
		TestEqual(TEXT("no Body row"), PauseTestIndexOf(Pages, EPauseMenuPage::Forms), INDEX_NONE);
		TestEqual(TEXT("the loadout page reads Equipment"), Pages[PauseTestIndexOf(Pages, EPauseMenuPage::Loadout)].Label.ToString(), FString(TEXT("Equipment")));
		const FText Tracked = FText::FromString(TEXT("A quest: its step"));
		const TArray<FPauseMenuPage> Tracking = WoRPauseMenu::BuildPages(false, true, Tracked);
		TestEqual(TEXT("B-463b: the Journal row previews the tracked quest"),
				  Tracking[PauseTestIndexOf(Tracking, EPauseMenuPage::Journal)].Preview.ToString(), FString(TEXT("Tracking: A quest: its step")));
		const TArray<FPauseMenuPage> InRun = WoRPauseMenu::BuildPages(true, true);
		TestEqual(TEXT("in a run, Quit reads Leave trial"), InRun.Last().Label.ToString(), FString(TEXT("Leave trial")));
		TestFalse(TEXT("outside one it doesn't"), Pages.Last().Label.EqualTo(InRun.Last().Label));
	});

	It("refuses Quit to a guest in a run, and only then", [this]()
	{
		const auto QuitOpens = [](bool bInRun, bool bCanEndRun)
		{
			const TArray<FPauseMenuPage> Pages = WoRPauseMenu::BuildPages(bInRun, bCanEndRun);
			return Pages[PauseTestIndexOf(Pages, EPauseMenuPage::Quit)].CanOpen();
		};
		TestFalse(TEXT("guest in a run"), QuitOpens(true, false));
		TestTrue(TEXT("host in a run"), QuitOpens(true, true));
		TestTrue(TEXT("guest in the hub"), QuitOpens(false, false));
	});

	It("warns that quitting a run loses everything (D-176)", [this]()
	{
		const FText InRun = WoRPauseMenu::GetQuitWarning(true);
		TestFalse(TEXT("a warning in a run"), InRun.IsEmpty());
		TestFalse(TEXT("not the hub's line"), InRun.EqualTo(WoRPauseMenu::GetQuitWarning(false)));
		const TArray<FPauseMenuPage> Pages = WoRPauseMenu::BuildPages(true, true);
		TestTrue(TEXT("Quit's preview is the warning"), Pages[PauseTestIndexOf(Pages, EPauseMenuPage::Quit)].Preview.EqualTo(InRun));
	});

	It("sums up the run: fights won, items found and its time, kept by a save (B-632)", [this]()
	{
		if (!TestNotNull(TEXT("run"), Run))
		{
			return;
		}
		Run->RecordItemFound();
		const FPauseMenuRunSummary Hub = WoRPauseMenu::BuildRunSummary(TestWorld->GetWorld());
		TestFalse(TEXT("hub: not in a run"), Hub.bInRun);
		TestEqual(TEXT("hub: its name"), Hub.Place.ToString(), FString(TEXT("The Hub")));
		TestEqual(TEXT("an item outside a run isn't counted"), Run->GetRunItemsFound(), 0);

		StartRun();
		Run->MarkEncounterBeaten(TEXT("FightOne"), EEncounterRank::Normal);
		Run->RecordItemFound();
		Run->RecordItemFound();
		const FPauseMenuRunSummary InRun = WoRPauseMenu::BuildRunSummary(TestWorld->GetWorld());
		TestTrue(TEXT("run: in a run"), InRun.bInRun);
		TestFalse(TEXT("run: the trial's name, not the hub's"), InRun.Place.EqualTo(Hub.Place));
		TestEqual(TEXT("run: one fight won"), InRun.FightsWon, 1);
		TestEqual(TEXT("run: two items found"), InRun.ItemsFound, 2);
		TestTrue(TEXT("run: its clock runs"), InRun.Seconds >= 0.0);

		Run->RestoreRunStats(PAUSE_TEST_ITEMS_SAVED, PAUSE_TEST_SECONDS_SAVED); // as USaveSubsystem::RestoreRun
		TestEqual(TEXT("a save's items come back"), Run->GetRunItemsFound(), PAUSE_TEST_ITEMS_SAVED);
		TestTrue(TEXT("its time runs on from the saved time"), Run->GetRunSeconds() >= PAUSE_TEST_SECONDS_SAVED);
		StartRun();
		TestEqual(TEXT("a new run starts at none"), Run->GetRunItemsFound(), 0);
		TestTrue(TEXT("and its clock at zero"), Run->GetRunSeconds() < PAUSE_TEST_SECONDS_SAVED);
	});

	It("writes the run's time as the clock reads it (B-632)", [this]()
	{
		TestEqual(TEXT("nothing yet"), WoRPauseMenu::FormatRunTime(0.0).ToString(), FString(TEXT("0:00")));
		TestEqual(TEXT("minutes and seconds"), WoRPauseMenu::FormatRunTime(PAUSE_TEST_MINUTES_ONLY).ToString(), FString(TEXT("18:42")));
		TestEqual(TEXT("past an hour"), WoRPauseMenu::FormatRunTime(PAUSE_TEST_PAST_AN_HOUR).ToString(), FString(TEXT("1:05:09")));
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
			TestEqual(TEXT("starts on Resume"), Menu->GetSelectedIndex(), PauseTestIndexOf(Menu->GetPages(), EPauseMenuPage::Resume));
			Menu->MoveSelection(-1);
			TestEqual(TEXT("up from the top: Quit"), Menu->GetSelectedIndex(), PauseTestIndexOf(Menu->GetPages(), EPauseMenuPage::Quit));
			Menu->MoveSelection(1);
			TestEqual(TEXT("down from the bottom: Resume"), Menu->GetSelectedIndex(), PauseTestIndexOf(Menu->GetPages(), EPauseMenuPage::Resume));
			Menu->PressSelected();
			TestFalse(TEXT("Resume closes the menu"), Menu->IsActivated());
		});

		It("notes the lit page, always, and shows the hub with no run rows (B-632: no Help toggle)", [this]()
		{
			UPauseMenuWidget *Menu = OpenMenu();
			if (!Menu)
			{
				return;
			}
			TestFalse(TEXT("in the hub: no This run"), Menu->GetRunSummary().bInRun);
			Menu->SelectIndex(PauseTestIndexOf(Menu->GetPages(), EPauseMenuPage::Loadout));
			TestEqual(TEXT("the note's heading is the page"), Menu->GetNoteHeading().ToString(), FString(TEXT("Equipment")));
			TestFalse(TEXT("and it says what the page is"), Menu->GetNoteText().IsEmpty());
		});

		It("asks before quitting, holds only from the second press, and stops when let go (D-176)", [this]()
		{
			UPauseMenuWidget *Menu = OpenMenu();
			if (!Menu)
			{
				return;
			}
			Menu->SelectIndex(PauseTestIndexOf(Menu->GetPages(), EPauseMenuPage::Quit));
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
			Menu->SelectIndex(PauseTestIndexOf(Menu->GetPages(), EPauseMenuPage::Quit));
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
			Menu->SelectIndex(PauseTestIndexOf(Menu->GetPages(), EPauseMenuPage::Quit));
			Menu->PressSelected();
			Menu->MoveSelection(-1);
			TestFalse(TEXT("moved away: no question"), Menu->IsConfirmingQuit());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
