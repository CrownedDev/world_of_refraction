// TitleScreenModel.h
// B-467 (D-202, D-060; UIScreens #9, #10, mockups screen_mockups/front/title/): what the title screen shows, with no
// widget in it, so the specs read it headless. UTitleScreenWidget draws it.
//
// The title is the main menu (no "press any button"). B-546 (D-229, stage 1): the character slots, as Destiny 2's
// character select: each filled slot shows its character's name and its last run's Mind / Body / Spirit levels; the
// next empty slot is a +; nothing after it, and no + once the slots are full (GetMaxSlots, one configurable value).
// Settings, Credits and Quit are buttons along the bottom. No New Game row and no Join a friend (joining is from the
// game menu). B-554 (stage 3): one save file per character (USaveSubsystem's slots); a slot row continues its own.
// Debug: wor.UI.PrintTitle logs GetTitleScreenString.

#pragma once

#include "CoreMinimal.h"

class UGameInstance;
struct FWoRSaveData;

enum class ETitleScreenRow : uint8
{
	/** A filled character slot: picking it continues that character. */
	Continue,
	/** The + slot: a new character (the new game screen). */
	NewGame,
	JoinFriend, // OBSOLETE (B-546, D-229): no longer listed; joining is from the game menu
	Settings,
	Credits,
	Quit
};

struct FTitleScreenRow
{
	ETitleScreenRow Id = ETitleScreenRow::NewGame;
	/** A slot: the character's name, or "+"; a button: its word. */
	FText Label;
	/** A filled slot: its last run's levels, "Mind 3  ·  Body 2  ·  Spirit 1" (GetRunLevelsText); the + slot: "New
	 *  character". Empty on a button. */
	FText Line;
	/** B-546: Settings, Credits and Quit sit along the bottom; the slots are the list. */
	bool bButton = false;
	/** Why the row can't be used yet; empty when it can. */
	FText Refusal;
	/** B-554: a filled slot's save slot (USaveSubsystem::GetSlotName); INDEX_NONE on the + and the buttons. */
	int32 Slot = INDEX_NONE;

	bool CanUse() const { return Refusal.IsEmpty(); }
};

/** What the title says about the one save. */
struct FTitleSaveSummary
{
	bool bHasSave = false;
	/** Where Continue starts: always the hub (B-408a: a saved run is never resumed, D-172). */
	FText Place;
	/** B-408a (Q-408): the save held a run when the game closed, so it was settled as a quit. */
	bool bLastRunLost = false;
	TArray<FText> PartyNames;
	double PlaySeconds = 0.0;
	/** B-546: the slot's character: the party leader's name. */
	FText LeaderName;
	/** B-546 (Crown): the leader's Mind / Body / Spirit levels earned in the save's run (FSavedRun::RunLevels). The
	 *  run's levels end with it (D-102), so only a run the save still holds has them; without one they show as dashes. */
	bool bHasRunLevels = false;
	int32 RunMind = 0;
	int32 RunBody = 0;
	int32 RunSpirit = 0;
	/** B-554: the save slot it was read from (INDEX_NONE: not a slot's). */
	int32 Slot = INDEX_NONE;
};

namespace WoRTitleScreen
{
	/** B-535: the title opens on a play session's first map load when it is the game's default map (MapPackage and
	 *  DefaultMapPackage are long package names, PIE prefix removed), unless switched off (wor.UI.TitleAtStart 0) or
	 *  under automation tests. */
	WORLD_OF_REFRACTION_API bool ShouldOpenAtStart(bool bEnabled, bool bAutomationTesting, const FString &MapPackage, const FString &DefaultMapPackage);

	/** B-546 (D-229): the most character slots: MaxCharacterSlots under [WoR.Title] in DefaultGame.ini, else (0 or
	 *  unset) one per class choice (WoRNewGame::GetClassChoiceCount). */
	WORLD_OF_REFRACTION_API int32 GetMaxSlots();

	/** The filled slot (with a save), the + while a slot is free, then the Settings, Credits and Quit buttons.
	 *  MaxSlots INDEX_NONE: GetMaxSlots. */
	WORLD_OF_REFRACTION_API TArray<FTitleScreenRow> BuildRows(const FTitleSaveSummary &Save, int32 MaxSlots = INDEX_NONE);
	/** B-554: a row per filled slot (in slot order, each with its Slot), the + while fewer than MaxSlots, then the
	 *  buttons. */
	WORLD_OF_REFRACTION_API TArray<FTitleScreenRow> BuildSlotRows(const TArray<FTitleSaveSummary> &Slots, int32 MaxSlots = INDEX_NONE);

	/** bRunSettled: the save subsystem already settled a run this session (WasClosedRunSettled), so Data holds none. */
	WORLD_OF_REFRACTION_API FTitleSaveSummary Summarize(const FWoRSaveData &Data, bool bHasSave, double PlaySeconds, bool bRunSettled = false);
	/** The game instance's save (USaveSubsystem). */
	WORLD_OF_REFRACTION_API FTitleSaveSummary SummarizeSave(const UGameInstance *GameInstance);
	/** B-554 (D-229): every filled slot below GetMaxSlots, in slot order (the active one as SummarizeSave says). */
	WORLD_OF_REFRACTION_API TArray<FTitleSaveSummary> SummarizeSlots(const UGameInstance *GameInstance);

	/** "14 h", or "25 min" under an hour. */
	WORLD_OF_REFRACTION_API FText GetPlayTimeText(double PlaySeconds);

	/** The + over a save (one save file until stage 3): what starting over replaces ("14 h, 3 characters, everything in
	 *  the home chest"). */
	WORLD_OF_REFRACTION_API FText GetNewGameWarning(const FTitleSaveSummary &Save);
	/** B-546: a slot's delete asks first: whose, and what goes with it. */
	WORLD_OF_REFRACTION_API FText GetDeleteWarning(const FTitleSaveSummary &Save);
	/** B-546 (Crown): a filled slot's one line: "Mind 3  ·  Body 2  ·  Spirit 1", or a dash for each with no run. */
	WORLD_OF_REFRACTION_API FText GetRunLevelsText(const FTitleSaveSummary &Save);

	/** D-060: "Your save was reset": the wiped save's version and the one this build needs. */
	WORLD_OF_REFRACTION_API FText GetOldVersionText(int32 WipedFromVersion);
	WORLD_OF_REFRACTION_API FText GetNeededVersionText();

	WORLD_OF_REFRACTION_API FString GetTitleScreenString(const UGameInstance *GameInstance);
}
