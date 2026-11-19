// LoadoutV2GridSpec.cpp
// D-248 step 1: the Equipment tab's picture grid, headless: which filter tabs show, where the keys go from a tile, the
// "Type (Tier)" line under a piece's name, the grid as text; and on the screen with no party the grid is closed and
// its keys do nothing. Unproven here: the grid on screen with a character's weapons (PIE sheet D-248-grid-step1).

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Misc/AutomationTest.h"
#include "UI/Menus/LoadoutV2GridModel.h"
#include "UI/Menus/LoadoutV2ScreenWidget.h"
#include "WoRGameNames.h"
#include "WoRTestWorld.h"

BEGIN_DEFINE_SPEC(FLoadoutV2GridSpec, "WoR.UI.LoadoutV2Grid", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FLoadoutV2GridSpec)

void FLoadoutV2GridSpec::Define()
{
	It("lists a filter tab for each weapon type present, once, in the types' order", [this]()
	{
		const TArray<EWeaponType> Present = WoRLoadoutV2Grid::GetTypesPresent({EWeaponType::Shield, EWeaponType::Sword, EWeaponType::Shield, EWeaponType::Sword, EWeaponType::Greatsword});
		if (TestEqual(TEXT("three types"), Present.Num(), 3))
		{
			TestTrue(TEXT("in the enum's order"), static_cast<uint8>(Present[0]) < static_cast<uint8>(Present[1]) && static_cast<uint8>(Present[1]) < static_cast<uint8>(Present[2]));
			TestTrue(TEXT("each of them"), Present.Contains(EWeaponType::Sword) && Present.Contains(EWeaponType::Shield) && Present.Contains(EWeaponType::Greatsword));
		}
		TestTrue(TEXT("nothing owned: only All"), WoRLoadoutV2Grid::GetTypesPresent({}).IsEmpty());
	});

	It("moves tile to tile: left and right step one, up and down keep the column, a short last row is reached from above", [this]()
	{
		constexpr int32 Columns = 7;
		constexpr int32 Count = 17; // rows of 7, 7 and 3
		TestEqual(TEXT("right"), WoRLoadoutV2Grid::Move(0, Count, Columns, 1, 0), 1);
		TestEqual(TEXT("right off a row's end: the next row's first"), WoRLoadoutV2Grid::Move(6, Count, Columns, 1, 0), 7);
		TestEqual(TEXT("left off a row's start: the row before's last"), WoRLoadoutV2Grid::Move(7, Count, Columns, -1, 0), 6);
		TestEqual(TEXT("left off the first tile: stays"), WoRLoadoutV2Grid::Move(0, Count, Columns, -1, 0), 0);
		TestEqual(TEXT("right off the last tile: stays"), WoRLoadoutV2Grid::Move(16, Count, Columns, 1, 0), 16);
		TestEqual(TEXT("down keeps the column"), WoRLoadoutV2Grid::Move(2, Count, Columns, 0, 1), 9);
		TestEqual(TEXT("up keeps the column"), WoRLoadoutV2Grid::Move(9, Count, Columns, 0, -1), 2);
		TestEqual(TEXT("up off the top row: stays"), WoRLoadoutV2Grid::Move(2, Count, Columns, 0, -1), 2);
		TestEqual(TEXT("down into the short row's own column"), WoRLoadoutV2Grid::Move(8, Count, Columns, 0, 1), 15);
		TestEqual(TEXT("down past the short row's end: its last tile"), WoRLoadoutV2Grid::Move(12, Count, Columns, 0, 1), 16);
		TestEqual(TEXT("down off the last row: stays"), WoRLoadoutV2Grid::Move(15, Count, Columns, 0, 1), 15);
		TestEqual(TEXT("an empty grid"), WoRLoadoutV2Grid::Move(0, 0, Columns, 1, 0), static_cast<int32>(INDEX_NONE));
	});

	It("writes a piece's type with its tier in brackets, and the grid as text", [this]()
	{
		const FString Letter = WoRGameNames::GetTierLetter(EItemTier::F_Tier).ToString();
		TestEqual(TEXT("Sword (F)"), WoRLoadoutV2Grid::GetTypeTierLine(FText::FromString(TEXT("Sword")), EItemTier::F_Tier).ToString(),
				  FString::Printf(TEXT("Sword (%s)"), *Letter));
		TestEqual(TEXT("no type word: the tier alone"), WoRLoadoutV2Grid::GetTypeTierLine(FText::GetEmpty(), EItemTier::F_Tier).ToString(), Letter);

		TArray<FLoadoutV2GridTile> Tiles;
		Tiles.AddDefaulted(3);
		Tiles[0].Icon.Name = FText::FromString(TEXT("Sword"));
		Tiles[0].bWorn = true;
		Tiles[1].Icon.Name = FText::FromString(TEXT("Sword"));
		Tiles[1].Count = 2;
		Tiles[2].Icon.Name = FText::FromString(TEXT("Bow"));
		Tiles[2].bLocked = true;
		TestEqual(TEXT("two to a row, the second selected"), WoRLoadoutV2Grid::GetGridString(Tiles, 2, 1), FString(TEXT("Sword (worn)  [Sword x2]\nBow (locked)")));
		TestEqual(TEXT("no tiles"), WoRLoadoutV2Grid::GetGridString({}, 2, 0), FString(TEXT("(no tiles)")));
	});

	It("on the screen with no party the grid is closed and its keys do nothing (headless)", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("world"), TestWorld.Create()))
		{
			return;
		}
		ULoadoutV2ScreenWidget *Screen = CreateWidget<ULoadoutV2ScreenWidget>(TestWorld.GetWorld(), ULoadoutV2ScreenWidget::StaticClass());
		if (!TestNotNull(TEXT("screen"), Screen))
		{
			return;
		}
		Screen->ActivateWidget();
		TestFalse(TEXT("no grid"), Screen->IsGridOpen());
		Screen->MoveInGrid(1, 0);
		Screen->CycleGridFilter(1);
		Screen->ClickGridTab(1);
		TestTrue(TEXT("still the list, no tiles, no tabs"), Screen->GetMode() == ELoadoutV2Mode::List && Screen->GetGridTiles().IsEmpty() && Screen->GetGridTypes().IsEmpty());
		Screen->PressBack();
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
