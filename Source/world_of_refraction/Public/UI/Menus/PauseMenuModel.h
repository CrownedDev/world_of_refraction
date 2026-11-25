// PauseMenuModel.h
// B-439 (D-181, UIScreens #1): what the game menu shows and what each page does, with no widget in it,
// so the specs read it headless. UPauseMenuWidget draws it.
//
// Debug: wor.UI.PrintPause logs GetPauseMenuString (header, pages, party); wor.UI.Pause opens the menu.

#pragma once

#include "CoreMinimal.h"
#include "UI/WoRIcon.h"

class APlayerController;
class UCharacterData;
class UWorld;

/** The menu's pages, in list order. */
enum class EPauseMenuPage : uint8
{
	Resume,
	Loadout,
	Inventory,
	/** OBSOLETE (B-632, mockup menus/pause/v2: no row; delete after Crown's PIE pass): the Character sheet's page. */
	Character,
	/** OBSOLETE (B-632, as Character): the Body page (B-536); the hub Mirror still opens it. */
	Forms,
	Party,
	Journal,
	Settings,
	Quit,
	/** B-562 (D-229, stage 5): a co-op guest joins the host's party as one of its own characters (listed before Quit). */
	Join
};

struct FPauseMenuPage
{
	EPauseMenuPage Page = EPauseMenuPage::Resume;
	FText Label;
	/** B-632: the short line under the label on the row: "Gear, skills, apparel". */
	FText SubLine;
	/** The side panel's text while the row is focused. */
	FText Preview;
	/** Why the page can't open; empty when it can. A refused row stays focusable to show this (UI_Theme §5). */
	FText Refusal;

	bool CanOpen() const { return Refusal.IsEmpty(); }
};

/** One value on the top line: "1,240 Prisms". */
struct FPauseMenuChip
{
	FText Value;
	FText Label;
};

struct FPauseMenuHeader
{
	bool bInRun = false;
	FText Title;
	FText Subtitle;
	TArray<FPauseMenuChip> Chips;
};

struct FPauseMenuMember
{
	FText Name;
	/** The party list's line under the name: element and class (D-254: no Spirit of ... / Body of ...). */
	FText ClassLine;
	/** B-632: the class alone ("Caster"), beside the element's badge on the Pause menu's Mind · Body · Spirit rows. */
	FText ClassName;
	FWoRIcon ElementIcon;
	/** The Character sheet's line: a player character's Spirit of ... / Body of ... (D-241), else ClassLine. */
	FText SheetLine;
	/** The Combat Tier letter; empty when the member isn't spawned in this level. */
	FText Tier;
	int32 HP = 0;
	int32 MaxHP = 0;
	int32 EP = 0;
	int32 MaxEP = 0;
	int32 Mind = 0;
	int32 Body = 0;
	int32 Spirit = 0;
};

/** B-632 (mockup menus/pause/v2): the title, and the right column's "This run" rows. */
struct FPauseMenuRunSummary
{
	bool bInRun = false;
	/** The trial's name in a run, the hub's outside one: under "Paused" and under "This run". */
	FText Place;
	int32 FightsWon = 0;
	/** The run's Gold (D-159: the run's money, never banked). */
	int32 RunGold = 0;
	/** Pieces, skills and crystals from chests and fight drops (not fight items). */
	int32 ItemsFound = 0;
	double Seconds = 0.0;
};

namespace WoRPauseMenu
{
	/** D-176: Quit asks for a hold this long (the mockups' destructive confirm). TODO: tune */
	inline constexpr float QUIT_HOLD_SECONDS = 2.0f;

	/** B-632: the pages (Resume, Equipment, Inventory, Party, Journal, Settings, then Leave trial or Quit). bCanEndRun: false for a co-op guest in a run (only the host ends it). TrackedLine (B-463b): the
	 *  tracked quest's step, the Journal row's preview when not empty. */
	WORLD_OF_REFRACTION_API TArray<FPauseMenuPage> BuildPages(bool bInRun, bool bCanEndRun, const FText &TrackedLine = FText::GetEmpty(), bool bHasFrontEnd = false);
	/** B-562: as above, with a Join row before Quit when bCanJoin (a co-op guest: a client); refused while bSeated (it
	 *  already plays a member). */
	WORLD_OF_REFRACTION_API TArray<FPauseMenuPage> BuildPages(bool bInRun, bool bCanEndRun, const FText &TrackedLine, bool bHasFrontEnd,
															  bool bCanJoin, bool bSeated);

	/** B-463b: the tracked quest's "Title: current goal"; empty when nothing is tracked. */
	WORLD_OF_REFRACTION_API FText GetTrackedLine(const UWorld *World);

	/** What Quit's confirm says: in a run, that everything from it is lost (D-176); outside one, back to the title when
	 *  the menu level exists (B-537), else that the game closes. */
	WORLD_OF_REFRACTION_API FText GetQuitWarning(bool bInRun, bool bHasFrontEnd = false);

	/** In a run: said under every page, the one way out with rewards (D-172). */
	WORLD_OF_REFRACTION_API FText GetExtractNote();

	WORLD_OF_REFRACTION_API bool IsInRun(const UWorld *World);

	/** The place and its money: a run's Prisms and wipes left, or the hub's Gold and essences. */
	WORLD_OF_REFRACTION_API FPauseMenuHeader BuildHeader(const UWorld *World);

	/** B-632: the run's place and counts; outside a run only the hub's name. */
	WORLD_OF_REFRACTION_API FPauseMenuRunSummary BuildRunSummary(const UWorld *World);

	/** B-632: a run's time as the clock reads it: "18:42", "1:05:09" past an hour. */
	WORLD_OF_REFRACTION_API FText FormatRunTime(double Seconds);

	/** The local party, in order. */
	WORLD_OF_REFRACTION_API TArray<FPauseMenuMember> BuildParty(const UWorld *World);

	/** D-254: a member's line on the party list: "Fire Caster", or the class alone with no innate element. The same
	 *  for a player character and a recruit. */
	WORLD_OF_REFRACTION_API FText GetClassLine(const UCharacterData *Data);

	/** D-254 / D-241: a member's line on the Character sheet (the player's profile): a player character's
	 *  Spirit of ... / Body of ... (WoRPlayerBuild::GetDetailLine), else GetClassLine. */
	WORLD_OF_REFRACTION_API FText GetSheetLine(const UCharacterData *Data, const FGuid &MemberId, const UWorld *World);

	/** Quit, after the hold: in a run the run is abandoned (nothing kept, back to the hub); outside one the
	 *  game saves and goes back to the menu level's title (B-537), or closes while there is no menu level. */
	WORLD_OF_REFRACTION_API void Quit(UWorld *World, APlayerController *PC);

	WORLD_OF_REFRACTION_API FString GetPauseMenuString(const UWorld *World);
}
