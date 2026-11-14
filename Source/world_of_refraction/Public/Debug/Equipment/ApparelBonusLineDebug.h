// ApparelBonusLineDebug.h
// B-475: wor.Apparel.PrintBonusLines lists every owned apparel piece with its rolled bonus line (D-208), so two copies
// can be compared without the inventory screen.

#pragma once

#include "CoreMinimal.h"

struct FApparelEntry;
class UWorld;

namespace ApparelBonusLineDebug
{
	/** "Leather Coat (C) a1b2...: +2 Luck" or "...: no bonus line". */
	WORLD_OF_REFRACTION_API FString GetBonusLineString(const FApparelEntry &Entry);

	/** Logs and shows on screen each owned piece's line. */
	WORLD_OF_REFRACTION_API void PrintBonusLines(UWorld *World);
}
