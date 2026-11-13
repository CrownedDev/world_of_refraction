// TitleScreenModel.h
// B-467 (D-202, D-060; UIScreens #9, #10, mockups screen_mockups/front/title/): what the title screen shows, with no
// widget in it, so the specs read it headless. UTitleScreenWidget draws it.
//
// The title is the main menu (no "press any button"): Continue first, with where the save is (place, party, time
// played), then New Game, Join a friend, Settings, Credits, Quit. One save per player, so no slot screen. A row whose
// screen isn't built yet (Join a friend, B-459; Credits, B-470) stays listed and says so.
// Debug: wor.UI.PrintTitle logs GetTitleScreenString.

#pragma once

#include "CoreMinimal.h"

class UGameInstance;
struct FWoRSaveData;

enum class ETitleScreenRow : uint8
{
	Continue,
	NewGame,
	JoinFriend,
	Settings,
	Credits,
	Quit
};

struct FTitleScreenRow
{
	ETitleScreenRow Id = ETitleScreenRow::NewGame;
	FText Label;
	/** Continue: "Hub · Sera, Garrick, Bram · 14 h". Empty on the others. */
	FText Line;
	/** Why the row can't be used yet; empty when it can. */
	FText Refusal;

	bool CanUse() const { return Refusal.IsEmpty(); }
};

/** What the title says about the one save. */
struct FTitleSaveSummary
{
	bool bHasSave = false;
	/** The trial of a saved run, else the hub. */
	FText Place;
	TArray<FText> PartyNames;
	double PlaySeconds = 0.0;
};

namespace WoRTitleScreen
{
	/** Continue (only with a save), New Game, Join a friend, Settings, Credits, Quit. */
	WORLD_OF_REFRACTION_API TArray<FTitleScreenRow> BuildRows(const FTitleSaveSummary &Save);

	WORLD_OF_REFRACTION_API FTitleSaveSummary Summarize(const FWoRSaveData &Data, bool bHasSave, double PlaySeconds);
	/** The game instance's save (USaveSubsystem). */
	WORLD_OF_REFRACTION_API FTitleSaveSummary SummarizeSave(const UGameInstance *GameInstance);

	/** "14 h", or "25 min" under an hour. */
	WORLD_OF_REFRACTION_API FText GetPlayTimeText(double PlaySeconds);

	/** New Game over a save: what starting over replaces ("14 h, 3 characters, everything in the home chest"). */
	WORLD_OF_REFRACTION_API FText GetNewGameWarning(const FTitleSaveSummary &Save);

	/** D-060: "Your save was reset": the wiped save's version and the one this build needs. */
	WORLD_OF_REFRACTION_API FText GetOldVersionText(int32 WipedFromVersion);
	WORLD_OF_REFRACTION_API FText GetNeededVersionText();

	WORLD_OF_REFRACTION_API FString GetTitleScreenString(const UGameInstance *GameInstance);
}
