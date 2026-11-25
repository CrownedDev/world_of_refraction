// LoadoutV2GridSpec.cpp
// D-248 step 1: the Equipment tab's picture grid, headless: which filter tabs show, where the keys go from a tile, the
// "Type (Tier)" line under a piece's name, the grid as text; and on the screen with no party the grid is closed and
// its keys do nothing. Step 2: a tile's place in the view, the scroll that shows a row, and where the pop-up card goes
// (right of the tile, left or bottoms level near an edge, always inside the view). Unproven here: the grid and the card
// on screen with a character's weapons (PIE sheets D-248-grid-step1, D-248-card-step2).

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

	It("finds a tile's place in the view, and the scroll that shows a row (D-248 step 2)", [this]()
	{
		constexpr int32 Columns = 10;
		constexpr float Pitch = 114.0f;
		constexpr float Padding = 3.0f;
		TestTrue(TEXT("the first tile"), WoRLoadoutV2Grid::GetTilePosition(0, Columns, Pitch, Padding, 0.0f).Equals(FVector2D(3.0, 3.0)));
		TestTrue(TEXT("the fourth in the second row"), WoRLoadoutV2Grid::GetTilePosition(13, Columns, Pitch, Padding, 0.0f).Equals(FVector2D(345.0, 117.0)));
		TestTrue(TEXT("scrolled down a row: it is a row higher"), WoRLoadoutV2Grid::GetTilePosition(13, Columns, Pitch, Padding, Pitch).Equals(FVector2D(345.0, 3.0)));

		constexpr float ViewHeight = 300.0f; // two rows and a part of a third
		TestEqual(TEXT("a row in view: no move"), WoRLoadoutV2Grid::ScrollToRow(0.0f, 1, Pitch, ViewHeight), 0.0f);
		TestEqual(TEXT("a row cut off at the bottom: down until its bottom shows"), WoRLoadoutV2Grid::ScrollToRow(0.0f, 2, Pitch, ViewHeight), 3.0f * Pitch - ViewHeight);
		TestEqual(TEXT("a row above the view: up to its top"), WoRLoadoutV2Grid::ScrollToRow(400.0f, 1, Pitch, ViewHeight), Pitch);
		TestEqual(TEXT("the first row: the top"), WoRLoadoutV2Grid::ScrollToRow(400.0f, 0, Pitch, ViewHeight), 0.0f);
	});

	It("puts the card beside the tile, on its left or with their bottoms level near an edge, and keeps it in the view (D-248 step 2)", [this]()
	{
		constexpr float Tile = 108.0f;
		constexpr float Gap = 6.0f;
		const FVector2D Card(520.0, 400.0);
		const FVector2D View(1156.0, 700.0);

		const WoRLoadoutV2Grid::FCardPlace Right = WoRLoadoutV2Grid::PlaceCard(FVector2D(117.0, 3.0), Tile, Card, View, Gap);
		TestTrue(TEXT("room on the right: there, tops level"), !Right.bLeft && !Right.bUp && Right.Position.Equals(FVector2D(117.0 + Tile + Gap, 3.0)));

		const WoRLoadoutV2Grid::FCardPlace Left = WoRLoadoutV2Grid::PlaceCard(FVector2D(1029.0, 3.0), Tile, Card, View, Gap);
		TestTrue(TEXT("no room on the right: on the left"), Left.bLeft && !Left.bUp && Left.Position.Equals(FVector2D(1029.0 - Gap - Card.X, 3.0)));

		const WoRLoadoutV2Grid::FCardPlace Up = WoRLoadoutV2Grid::PlaceCard(FVector2D(117.0, 573.0), Tile, Card, View, Gap);
		TestTrue(TEXT("it would run past the bottom: bottoms level"), !Up.bLeft && Up.bUp && Up.Position.Equals(FVector2D(117.0 + Tile + Gap, 573.0 + Tile - Card.Y)));

		const WoRLoadoutV2Grid::FCardPlace Corner = WoRLoadoutV2Grid::PlaceCard(FVector2D(1029.0, 573.0), Tile, Card, View, Gap);
		TestTrue(TEXT("the bottom right corner: left and up"), Corner.bLeft && Corner.bUp);

		// A tile scrolled half out of the top: the card stays inside the view.
		const WoRLoadoutV2Grid::FCardPlace Clipped = WoRLoadoutV2Grid::PlaceCard(FVector2D(117.0, -60.0), Tile, Card, View, Gap);
		TestEqual(TEXT("not above the view"), Clipped.Position.Y, 0.0);

		// A card taller and wider than the view starts at the view's corner.
		const WoRLoadoutV2Grid::FCardPlace Huge = WoRLoadoutV2Grid::PlaceCard(FVector2D(117.0, 300.0), Tile, FVector2D(2000.0, 900.0), View, Gap);
		TestTrue(TEXT("a card bigger than the view: its corner"), Huge.Position.Equals(FVector2D(0.0, 0.0)));

		for (const WoRLoadoutV2Grid::FCardPlace &Place : {Right, Left, Up, Corner, Clipped})
		{
			TestTrue(TEXT("inside the view"), Place.Position.X >= 0.0 && Place.Position.Y >= 0.0 && Place.Position.X + Card.X <= View.X && Place.Position.Y + Card.Y <= View.Y);
		}
	});

	It("B-612: the hands and the sockets fill from the picture grid; the other slots don't", [this]()
	{
		TestTrue(TEXT("main hand"), ULoadoutV2ScreenWidget::HasSlotGrid(ELoadoutV2Slot::MainHand));
		TestTrue(TEXT("off hand"), ULoadoutV2ScreenWidget::HasSlotGrid(ELoadoutV2Slot::OffHand));
		TestTrue(TEXT("the crystal socket: its fitting choices, not the inventory's All tab"), ULoadoutV2ScreenWidget::HasSlotGrid(ELoadoutV2Slot::Socket1));
		TestTrue(TEXT("the stone socket"), ULoadoutV2ScreenWidget::HasSlotGrid(ELoadoutV2Slot::Socket2));
		TestFalse(TEXT("the adapter: its list"), ULoadoutV2ScreenWidget::HasSlotGrid(ELoadoutV2Slot::Adapter));
		TestFalse(TEXT("the evolution: its list"), ULoadoutV2ScreenWidget::HasSlotGrid(ELoadoutV2Slot::Evolution));
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
