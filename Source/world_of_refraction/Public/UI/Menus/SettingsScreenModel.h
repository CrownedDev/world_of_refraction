// SettingsScreenModel.h
// B-452 (D-190, D-181, UIScreens #8, mockups screen_mockups/menus/settings/): what the settings screen shows and
// what changing a row does, with no widget in it, so the specs read it headless. USettingsScreenWidget draws it.
//
// Pages: Gameplay, Camera, Display, Audio, Controls (a read-only key list: fixed keys, D-063), Accessibility.
// Every approved row is listed. The ones the code has work (Turn View, End Turn button, screen shake: B-453a);
// the rest stay focusable and say they aren't built (B-453 adds them one at a time; the battle control model is
// B-436l). Difficulty is chosen at the Gate, not here (D-190).
// Debug: wor.UI.PrintSettings logs GetSettingsString.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/** The pages, in tab order. */
enum class ESettingsPage : uint8
{
	Gameplay,
	Camera,
	Display,
	Audio,
	Controls,
	Accessibility
};

enum class ESettingId : uint8
{
	BattleControls,
	TurnView,
	EndTurnButton,
	CornerHints,
	BossCinematics,
	AIFinishesTurn,
	TutorialTips,
	TextSize,
	FightCameraMovement,
	ScreenShake,
	HoldToConfirm,
	ElementLetters,
	Subtitles
};

struct FSettingsRow
{
	ESettingId Id = ESettingId::TurnView;
	FText Label;
	/** The current value, as the row shows it: "Centred, wide", "On". Empty for a row that isn't built. */
	FText Value;
	/** The side panel's plain-words explanation. */
	FText Explanation;
	/** Why the row can't be changed (not built yet); empty when it can. */
	FText Refusal;

	bool CanChange() const { return Refusal.IsEmpty(); }
};

/** One line of the Controls page: an action and its key on each device. */
struct FSettingsKeyRow
{
	/** "Field", "Battle", "Menus": a heading row when Action is empty. */
	FText Section;
	FText Action;
	FText Keyboard;
	FText Gamepad;

	bool IsHeading() const { return Action.IsEmpty(); }
};

namespace WoRSettingsScreen
{
	inline constexpr int32 PAGE_COUNT = 6;

	WORLD_OF_REFRACTION_API FText GetPageName(ESettingsPage Page);

	/** The page's rows; none for Camera, Display, Audio and Controls (see GetPageNote and BuildKeyRows). */
	WORLD_OF_REFRACTION_API TArray<FSettingsRow> BuildRows(const UWorld *World, ESettingsPage Page);

	/** What a page with no rows says; empty for a page with rows. */
	WORLD_OF_REFRACTION_API FText GetPageNote(ESettingsPage Page);

	/** The Controls page: the built keys, by section (InputMap.md). */
	WORLD_OF_REFRACTION_API TArray<FSettingsKeyRow> BuildKeyRows();

	/** Steps the setting one value in Direction (+1 / -1; a toggle flips either way). True when it changed.
	 *  The change is live at once and reaches the save file with SaveChanges. */
	WORLD_OF_REFRACTION_API bool ChangeSetting(UWorld *World, ESettingId Id, int32 Direction);

	/** Writes the settings to the save file (the screen calls it on closing after a change). */
	WORLD_OF_REFRACTION_API void SaveChanges(UWorld *World);

	WORLD_OF_REFRACTION_API FString GetSettingsString(const UWorld *World);
}
