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
