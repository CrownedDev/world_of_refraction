// RunEndModel.h
// B-465 (D-187, D-188, D-189; UIScreens.md #26, #27, mockups run/run_end/2_wipe.png, 4_run_over.png), first part: what
// the wipe card and the run-end page show, with no widget in it, so the specs read it headless. URunEndCardWidget draws
// both.
// - The wipe card: "Your possibilities end here" (D-188), the opponent that dealt the last blow by name (D-187: never a
//   rank), the run Prisms dropped where the party fell (D-160) and an earlier drop lost, where the party wakes, "the
//   enemy is still there", the wipes as pips (used of 5), and one button, "Get up".
// - The run-end page: "Trial cleared" (the boss beaten) or "The trial ends", the share the ending pays in words
//   (D-189: full, half and the run's upgrades, a quarter of its currencies, nothing), one centred line listing what the
//   run gave, and "Return" (D-187: no Kept / Lost columns).
// Debug: GetWipeCardString / GetRunEndString; wor.UI.WipeCard and wor.UI.RunEnd preview them (URunEndCardWidget).

#pragma once

#include "CoreMinimal.h"
#include "Trial/RunRewards.h"

namespace WoRDefeat
{
	struct FWipeResult;
}

/** B-465: how a run ended (D-189's four endings). */
enum class ERunEnding : uint8
{
	/** The trial's boss beaten: full payout. */
	BossBeaten,
	/** Extracted at a rest spot (D-186): half, and the run's upgrades kept. */
	Extracted,
	/** Out of wipes: a quarter of the run's currencies, no upgrades. */
	OutOfWipes,
	/** Quit from the game menu: nothing. */
	Quit
};

struct FWipeCard
{
	FText Title;
	/** "Felled by Kael." Empty when no foe dealt the last blow (a status tick). */
	FText LastBlowLine;
	/** What the wipe cost and what comes next, one line each. */
	TArray<FText> Lines;
	int32 WipesUsed = 0;
	int32 WipesMax = 0;
	/** "Wipes 3 of 5". */
	FText WipesLine;
	FText ButtonText;
	/** This wipe ended the run: the run-end page follows "Get up". */
	bool bRunEnds = false;
};

struct FRunEndPage
{
	ERunEnding Ending = ERunEnding::BossBeaten;
	FText Title;
	/** Gold title for a cleared trial, rose otherwise. */
	bool bCleared = false;
	/** What the ending pays, in words ("Full payout"). */
	FText ShareLine;
	/** One centred line: what the run gave ("+1200 Gold · +30 Gear Essence"), or that it gave nothing. */
	FText PayoutLine;
	FText ButtonText;
};

namespace WoRRunEnd
{
	/** The wipe card for Wipe. bHasRestStand: the party rested this visit, so it wakes there (else where the trial
	 *  began). */
	WORLD_OF_REFRACTION_API FWipeCard BuildWipeCard(const WoRDefeat::FWipeResult &Wipe, bool bHasRestStand);

	/** The run-end page for Ending; Payout is what the end pays (UTrialRunSubsystem::GetRunEndPayout before it pays). */
	WORLD_OF_REFRACTION_API FRunEndPage BuildRunEnd(ERunEnding Ending, const FRunPayout &Payout);

	/** The payout as one line: each currency it gives, "+n Name", joined; "Nothing" when it gives none. */
	WORLD_OF_REFRACTION_API FText FormatPayoutLine(const FRunPayout &Payout);

	WORLD_OF_REFRACTION_API FString GetWipeCardString(const FWipeCard &Card);
	WORLD_OF_REFRACTION_API FString GetRunEndString(const FRunEndPage &Page);
}
