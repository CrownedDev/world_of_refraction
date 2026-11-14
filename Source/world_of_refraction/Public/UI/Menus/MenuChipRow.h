// MenuChipRow.h
// B-482 (Crown, 2026-11-14): a row of framed chips between its two key hints, the shared piece for tabs, characters
// and filters on the code-built menus: "LB  [Weapons] [Skills] [Apparel] [Cosmetics] later  RB". Each chip is its own
// frame; the current one is filled, the one under the mouse is lit, one that isn't there yet is greyed with a small
// note after it. A screen keeps an FChipRow, builds it with WoRMenuStyle::MakeChipRow, fills it with SetChips on every
// refresh and asks FindChipUnder on a click or a mouse move (the builders are in Private/UI/Menus/MenuScreenStyle.h).

#pragma once

#include "CoreMinimal.h"

class UBorder;
class UTextBlock;

namespace WoRMenuStyle
{
	/** What one chip shows. */
	struct FChipState
	{
		FText Name;
		/** Not there yet: greyed, and a click only says so. */
		bool bGreyed = false;
		/** A small note after the chip ("later"); empty for none. */
		FText Note;
	};

	/** The chips of one row. The widgets live in the screen's widget tree. */
	struct FChipRow
	{
		TArray<TWeakObjectPtr<UBorder>> Chips;
		TArray<TWeakObjectPtr<UTextBlock>> Labels;
		TArray<TWeakObjectPtr<UTextBlock>> Notes;
	};
}
