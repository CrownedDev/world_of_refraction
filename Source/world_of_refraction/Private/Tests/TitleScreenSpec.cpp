// TitleScreenSpec.cpp
// B-467 (D-202, D-060): the title screen's model. B-546 (D-229): the character slots (a filled one with its character,
// then the + while a slot is free), then the Settings, Credits and Quit buttons; starting over and deleting say what
// goes; the reset notice names both versions. The screen itself is the PIE sheet (docs/PIE/B-546.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/ECharacterClass.h"
#include "Character/Forms/CharacterFormData.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Save/WoRSaveGame.h"
#include "Save/WoRSaveTypes.h"
#include "UI/Menus/TitleScreenModel.h"

namespace
{
	/** The + and the three buttons. */
	constexpr int32 TITLE_TEST_ROWS_WITHOUT_SAVE = 4;
	constexpr int32 TITLE_TEST_BUTTONS = 3;
	constexpr double TITLE_TEST_FOURTEEN_AND_A_HALF_HOURS = 14.5 * 3600.0;
	constexpr double TITLE_TEST_FIFTY_NINE_MINUTES = 59.0 * 60.0 + 30.0;
	constexpr int32 TITLE_TEST_OLD_VERSION = 31;

	FTitleSaveSummary MakeSave()
	{
		FTitleSaveSummary Save;
		Save.bHasSave = true;
		Save.Place = FText::FromString(TEXT("The Hub"));
		Save.PartyNames = {FText::FromString(TEXT("Sera")), FText::FromString(TEXT("Garrick")), FText::FromString(TEXT("Bram"))};
		Save.PlaySeconds = TITLE_TEST_FOURTEEN_AND_A_HALF_HOURS;
		Save.LeaderName = FText::FromString(TEXT("Sera"));
		Save.bHasRunLevels = true;
		Save.RunMind = 3;
		Save.RunBody = 2;
		Save.RunSpirit = 1;
		return Save;
	}
}

BEGIN_DEFINE_SPEC(FTitleScreenSpec, "WoR.UI.TitleScreen", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTitleScreenSpec)

void FTitleScreenSpec::Define()
{
	It("with no save shows the + as the only slot, then the three buttons; no New Game or Join a friend row", [this]()
	{
		const TArray<FTitleScreenRow> Rows = WoRTitleScreen::BuildRows(FTitleSaveSummary());
		TestEqual(TEXT("rows"), Rows.Num(), TITLE_TEST_ROWS_WITHOUT_SAVE);
		TestTrue(TEXT("the + first"), Rows[0].Id == ETitleScreenRow::NewGame && !Rows[0].bButton);
		TestTrue(TEXT("Quit last"), Rows.Last().Id == ETitleScreenRow::Quit);
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			TestFalse(TEXT("named"), Rows[Index].Label.IsEmpty());
			TestTrue(TEXT("usable"), Rows[Index].CanUse());
			TestTrue(TEXT("no Continue, no Join a friend"), Rows[Index].Id != ETitleScreenRow::Continue && Rows[Index].Id != ETitleScreenRow::JoinFriend);
			TestEqual(TEXT("the last three are buttons"), Rows[Index].bButton, Index >= Rows.Num() - TITLE_TEST_BUTTONS);
		}
	});

	It("with a save shows its character as slot 1 (the name and the last run's levels only) and the + as slot 2", [this]()
	{
		const TArray<FTitleScreenRow> Rows = WoRTitleScreen::BuildRows(MakeSave(), 2);
		TestEqual(TEXT("one row more"), Rows.Num(), TITLE_TEST_ROWS_WITHOUT_SAVE + 1);
		TestTrue(TEXT("the filled slot first"), Rows[0].Id == ETitleScreenRow::Continue && Rows[0].CanUse());
		TestEqual(TEXT("named for its character"), Rows[0].Label.ToString(), FString(TEXT("Sera")));
		const FString Line = Rows[0].Line.ToString();
		TestTrue(TEXT("the run's three levels"), Line.Contains(TEXT("Mind 3")) && Line.Contains(TEXT("Body 2")) && Line.Contains(TEXT("Spirit 1")));
		TestFalse(TEXT("nothing else: no place, no time"), Line.Contains(TEXT("Hub")) || Line.Contains(TEXT("14 h")));
		TestTrue(TEXT("the + second"), Rows[1].Id == ETitleScreenRow::NewGame && !Rows[1].bButton);
		FTitleSaveSummary NeverRan = MakeSave();
		NeverRan.bHasRunLevels = false;
		TestFalse(TEXT("no run: a dash, not a level"), WoRTitleScreen::GetRunLevelsText(NeverRan).ToString().Contains(TEXT("3")));
		TestTrue(TEXT("no run: dashes"), WoRTitleScreen::GetRunLevelsText(NeverRan).ToString().Contains(TEXT("Mind -")));
	});

	It("shows no + once the slots are full, and defaults the most slots to the class choices (B-546)", [this]()
	{
		const TArray<FTitleScreenRow> Rows = WoRTitleScreen::BuildRows(MakeSave(), 1);
		TestEqual(TEXT("the slot and the buttons"), Rows.Num(), 1 + TITLE_TEST_BUTTONS);
		TestFalse(TEXT("no +"), Rows.ContainsByPredicate([](const FTitleScreenRow &Row) { return Row.Id == ETitleScreenRow::NewGame; }));
		TestTrue(TEXT("at least one slot"), WoRTitleScreen::GetMaxSlots() > 0);
	});

	It("names the character a delete takes, and what goes with it (B-546)", [this]()
	{
		const FString Warning = WoRTitleScreen::GetDeleteWarning(MakeSave()).ToString();
		TestTrue(TEXT("whose"), Warning.Contains(TEXT("Sera")));
		TestTrue(TEXT("the time"), Warning.Contains(TEXT("14 h")));
		TestTrue(TEXT("for good"), Warning.Contains(TEXT("undone")));
	});

	It("says time under an hour in minutes", [this]()
	{
		TestEqual(TEXT("minutes"), WoRTitleScreen::GetPlayTimeText(TITLE_TEST_FIFTY_NINE_MINUTES).ToString(), FString(TEXT("59 min")));
		TestEqual(TEXT("hours"), WoRTitleScreen::GetPlayTimeText(TITLE_TEST_FOURTEEN_AND_A_HALF_HOURS).ToString(), FString(TEXT("14 h")));
	});

	It("warns what starting over replaces", [this]()
	{
		const FString Warning = WoRTitleScreen::GetNewGameWarning(MakeSave()).ToString();
		TestTrue(TEXT("the time"), Warning.Contains(TEXT("14 h")));
		TestTrue(TEXT("the characters"), Warning.Contains(TEXT("3 characters")));
		TestTrue(TEXT("the home chest"), Warning.Contains(TEXT("home chest")));
	});

	It("summarizes a save with no run as the hub, and names only resolved characters", [this]()
	{
		FWoRSaveData Data;
		Data.Party.bHasParty = true;
		Data.Party.Members.AddDefaulted(); // no character asset: not named
		const FTitleSaveSummary Summary = WoRTitleScreen::Summarize(Data, true, TITLE_TEST_FIFTY_NINE_MINUTES);
		TestTrue(TEXT("has a save"), Summary.bHasSave);
		TestFalse(TEXT("a place"), Summary.Place.IsEmpty());
		TestEqual(TEXT("no names"), Summary.PartyNames.Num(), 0);
		TestFalse(TEXT("no save, no summary"), WoRTitleScreen::Summarize(Data, false, 0.0).bHasSave);
	});

	It("names a character made from picks by its body, with Spirit of over Body of on its slot (D-241)", [this]()
	{
		FWoRSaveData Data;
		Data.Party.bHasParty = true;
		FSavedPartyMember &Member = Data.Party.Members.AddDefaulted_GetRef();
		Member.Picks.Name = TEXT("Fire Refractor");
		Member.Picks.Class = ECharacterClass::Caster;
		Member.Picks.Element = ESpellElement::Fire;
		Data.Forms.BaseForm = static_cast<uint8>(EPlayerBaseForm::Quinn);
		FTitleSaveSummary Summary = WoRTitleScreen::Summarize(Data, true, TITLE_TEST_FIFTY_NINE_MINUTES);
		TestEqual(TEXT("named by its body"), Summary.LeaderName.ToString(), FString(TEXT("Female Nobody")));
		TestEqual(TEXT("its spirit"), Summary.LeaderSpirit.ToString(), FString(TEXT("Fire")));
		const TArray<FTitleScreenRow> Rows = WoRTitleScreen::BuildSlotRows({Summary}, 2);
		TestEqual(TEXT("first line"), Rows[0].Label.ToString(), FString(TEXT("Spirit of Fire")));
		TestTrue(TEXT("second line, over the levels"), Rows[0].Line.ToString().StartsWith(TEXT("Body of Female Nobody")) && Rows[0].Line.ToString().Contains(TEXT("Mind")));
	});

	It("names both versions on the reset notice", [this]()
	{
		TestTrue(TEXT("the old one"), WoRTitleScreen::GetOldVersionText(TITLE_TEST_OLD_VERSION).ToString().Contains(TEXT("31")));
		TestTrue(TEXT("the one needed"), WoRTitleScreen::GetNeededVersionText().ToString().Contains(FString::FromInt(WoRSaveVersion::MIN_SUPPORTED)));
	});

	It("opens at start only on the default map, unless switched off or under tests (B-535)", [this]()
	{
		const FString Hub = TEXT("/Game/Level/TestLevel_Nav");
		const FString Trial = TEXT("/Game/Level/TestLevel_Trial");
		TestTrue(TEXT("The hub: opens"), WoRTitleScreen::ShouldOpenAtStart(true, false, Hub, Hub));
		TestFalse(TEXT("A trial test level: doesn't"), WoRTitleScreen::ShouldOpenAtStart(true, false, Trial, Hub));
		TestFalse(TEXT("wor.UI.TitleAtStart 0: doesn't"), WoRTitleScreen::ShouldOpenAtStart(false, false, Hub, Hub));
		TestFalse(TEXT("Under tests: doesn't"), WoRTitleScreen::ShouldOpenAtStart(true, true, Hub, Hub));
		TestFalse(TEXT("No default map set: doesn't"), WoRTitleScreen::ShouldOpenAtStart(true, false, FString(), FString()));
	});
}

#endif
