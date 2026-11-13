// TitleScreenSpec.cpp
// B-467 (D-202, D-060): the title screen's model. Continue is first and only with a save, with where it is, who is
// in the party and the time played; the rows not built yet say so; New Game's warning says what starting over
// replaces; the reset notice names both versions. The screen itself is the PIE sheet (docs/PIE/B-467.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Save/WoRSaveGame.h"
#include "Save/WoRSaveTypes.h"
#include "UI/Menus/TitleScreenModel.h"

namespace
{
	constexpr int32 TITLE_TEST_ROWS_WITHOUT_SAVE = 5;
	constexpr double TITLE_TEST_FOURTEEN_AND_A_HALF_HOURS = 14.5 * 3600.0;
	constexpr double TITLE_TEST_FIFTY_NINE_MINUTES = 59.0 * 60.0 + 30.0;
	constexpr int32 TITLE_TEST_OLD_VERSION = 31;

	FTitleSaveSummary MakeSave()
	{
		FTitleSaveSummary Save;
		Save.bHasSave = true;
		Save.Place = FText::FromString(TEXT("Void Realm"));
		Save.PartyNames = {FText::FromString(TEXT("Sera")), FText::FromString(TEXT("Garrick")), FText::FromString(TEXT("Bram"))};
		Save.PlaySeconds = TITLE_TEST_FOURTEEN_AND_A_HALF_HOURS;
		return Save;
	}
}

BEGIN_DEFINE_SPEC(FTitleScreenSpec, "WoR.UI.TitleScreen", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTitleScreenSpec)

void FTitleScreenSpec::Define()
{
	It("with no save lists New Game first, no Continue, and the rows not built yet say so", [this]()
	{
		const TArray<FTitleScreenRow> Rows = WoRTitleScreen::BuildRows(FTitleSaveSummary());
		TestEqual(TEXT("rows"), Rows.Num(), TITLE_TEST_ROWS_WITHOUT_SAVE);
		TestTrue(TEXT("New Game first"), Rows[0].Id == ETitleScreenRow::NewGame);
		TestTrue(TEXT("Quit last"), Rows.Last().Id == ETitleScreenRow::Quit);
		for (const FTitleScreenRow &Row : Rows)
		{
			TestFalse(TEXT("named"), Row.Label.IsEmpty());
			TestTrue(TEXT("no Continue"), Row.Id != ETitleScreenRow::Continue);
			const bool bNotBuilt = Row.Id == ETitleScreenRow::JoinFriend || Row.Id == ETitleScreenRow::Credits;
			TestEqual(TEXT("only Join a friend and Credits are refused"), Row.CanUse(), !bNotBuilt);
		}
	});

	It("with a save puts Continue first, with the place, the party and the time played", [this]()
	{
		const TArray<FTitleScreenRow> Rows = WoRTitleScreen::BuildRows(MakeSave());
		TestEqual(TEXT("one row more"), Rows.Num(), TITLE_TEST_ROWS_WITHOUT_SAVE + 1);
		TestTrue(TEXT("Continue first"), Rows[0].Id == ETitleScreenRow::Continue);
		TestTrue(TEXT("usable"), Rows[0].CanUse());
		const FString Line = Rows[0].Line.ToString();
		TestTrue(TEXT("the place"), Line.Contains(TEXT("Void Realm")));
		TestTrue(TEXT("the party"), Line.Contains(TEXT("Sera, Garrick, Bram")));
		TestTrue(TEXT("whole hours"), Line.Contains(TEXT("14 h")));
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

	It("names both versions on the reset notice", [this]()
	{
		TestTrue(TEXT("the old one"), WoRTitleScreen::GetOldVersionText(TITLE_TEST_OLD_VERSION).ToString().Contains(TEXT("31")));
		TestTrue(TEXT("the one needed"), WoRTitleScreen::GetNeededVersionText().ToString().Contains(FString::FromInt(WoRSaveVersion::MIN_SUPPORTED)));
	});
}

#endif
