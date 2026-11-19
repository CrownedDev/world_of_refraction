// LoadoutV2GridModel.cpp

#include "UI/Menus/LoadoutV2GridModel.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "WoRLoadoutV2Grid"

TArray<EWeaponType> WoRLoadoutV2Grid::GetTypesPresent(TConstArrayView<EWeaponType> Types)
{
	TArray<EWeaponType> Present;
	for (const EWeaponType Type : Types)
	{
		Present.AddUnique(Type);
	}
	Present.Sort([](EWeaponType A, EWeaponType B) { return static_cast<uint8>(A) < static_cast<uint8>(B); });
	return Present;
}

int32 WoRLoadoutV2Grid::Move(int32 Index, int32 Count, int32 Columns, int32 DeltaX, int32 DeltaY)
{
	if (Count <= 0 || Columns <= 0)
	{
		return INDEX_NONE;
	}
	Index = FMath::Clamp(Index, 0, Count - 1);
	if (DeltaX != 0)
	{
		const int32 Stepped = Index + (DeltaX > 0 ? 1 : -1);
		return Stepped >= 0 && Stepped < Count ? Stepped : Index;
	}
	if (DeltaY != 0)
	{
		const int32 Stepped = Index + (DeltaY > 0 ? Columns : -Columns);
		if (Stepped < 0)
		{
			return Index;
		}
		if (Stepped < Count)
		{
			return Stepped;
		}
		// Below is the short last row, or nothing: its last tile when this row isn't the last already.
		const int32 LastRow = (Count - 1) / Columns;
		return Index / Columns < LastRow ? Count - 1 : Index;
	}
	return Index;
}

FText WoRLoadoutV2Grid::GetTypeTierLine(const FText &TypeName, EItemTier Tier)
{
	const FText Letter = WoRGameNames::GetTierLetter(Tier);
	return TypeName.IsEmpty() ? Letter : FText::Format(LOCTEXT("TypeTierLine", "{0} ({1})"), TypeName, Letter);
}

FVector2D WoRLoadoutV2Grid::GetTilePosition(int32 Index, int32 Columns, float RowPitch, float Padding, float ScrollOffset)
{
	if (Index < 0 || Columns <= 0)
	{
		return FVector2D::ZeroVector;
	}
	return FVector2D((Index % Columns) * RowPitch + Padding, (Index / Columns) * RowPitch + Padding - ScrollOffset);
}

float WoRLoadoutV2Grid::ScrollToRow(float Offset, int32 Row, float RowPitch, float ViewHeight)
{
	const float RowTop = FMath::Max(Row, 0) * RowPitch;
	const float RowBottom = RowTop + RowPitch;
	if (RowTop < Offset)
	{
		return RowTop;
	}
	if (RowBottom > Offset + ViewHeight)
	{
		return FMath::Max(RowBottom - ViewHeight, 0.0f);
	}
	return Offset;
}

WoRLoadoutV2Grid::FCardPlace WoRLoadoutV2Grid::PlaceCard(const FVector2D &TilePosition, float TileSize, const FVector2D &CardSize, const FVector2D &ViewSize, float Gap)
{
	FCardPlace Place;
	Place.Position.X = TilePosition.X + TileSize + Gap;
	if (Place.Position.X + CardSize.X > ViewSize.X)
	{
		Place.bLeft = true;
		Place.Position.X = TilePosition.X - Gap - CardSize.X;
	}
	Place.Position.Y = TilePosition.Y;
	if (Place.Position.Y + CardSize.Y > ViewSize.Y)
	{
		Place.bUp = true;
		Place.Position.Y = TilePosition.Y + TileSize - CardSize.Y;
	}
	Place.Position.X = FMath::Clamp(Place.Position.X, 0.0, FMath::Max(ViewSize.X - CardSize.X, 0.0));
	Place.Position.Y = FMath::Clamp(Place.Position.Y, 0.0, FMath::Max(ViewSize.Y - CardSize.Y, 0.0));
	return Place;
}

FString WoRLoadoutV2Grid::GetGridString(const TArray<FLoadoutV2GridTile> &Tiles, int32 Columns, int32 Selected)
{
	FString Out;
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		const FLoadoutV2GridTile &Tile = Tiles[Index];
		FString Text = Tile.Icon.Name.ToString();
		if (Tile.Count > 1)
		{
			Text += FString::Printf(TEXT(" x%d"), Tile.Count);
		}
		if (Tile.bWorn)
		{
			Text += TEXT(" (worn)");
		}
		if (Tile.bLocked)
		{
			Text += TEXT(" (locked)");
		}
		Out += Index == 0 ? TEXT("") : Columns > 0 && Index % Columns == 0 ? TEXT("\n") : TEXT("  ");
		Out += Index == Selected ? FString::Printf(TEXT("[%s]"), *Text) : Text;
	}
	return Out.IsEmpty() ? FString(TEXT("(no tiles)")) : Out;
}

#undef LOCTEXT_NAMESPACE
