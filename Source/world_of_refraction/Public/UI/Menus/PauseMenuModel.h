// PauseMenuModel.h
// B-439 (D-181, UIScreens #1): what the game menu shows and what each page does, with no widget in it,
// so the specs read it headless. UPauseMenuWidget draws it.
//
// Debug: wor.UI.PrintPause logs GetPauseMenuString (header, pages, party); wor.UI.Pause opens the menu.

#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UWorld;

/** The menu's pages, in list order. */
enum class EPauseMenuPage : uint8
{
	Resume,
	Loadout,
	Inventory,
	Character,
	Party,
	Journal,
	Settings,
	Quit
};

struct FPauseMenuPage
{
	EPauseMenuPage Page = EPauseMenuPage::Resume;
	FText Label;
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
	FText ClassLine;
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

namespace WoRPauseMenu
{
	/** D-176: Quit asks for a hold this long (the mockups' destructive confirm). TODO: tune */
	inline constexpr float QUIT_HOLD_SECONDS = 2.0f;

	/** The eight pages. bCanEndRun: false for a co-op guest in a run (only the host ends it). */
	WORLD_OF_REFRACTION_API TArray<FPauseMenuPage> BuildPages(bool bInRun, bool bCanEndRun);

	/** What Quit's confirm says: in a run, that everything from it is lost (D-176). */
	WORLD_OF_REFRACTION_API FText GetQuitWarning(bool bInRun);

	/** In a run: said under every page, the one way out with rewards (D-172). */
	WORLD_OF_REFRACTION_API FText GetExtractNote();

	WORLD_OF_REFRACTION_API bool IsInRun(const UWorld *World);

	/** The place and its money: a run's Prisms and wipes left, or the hub's Gold and essences. */
	WORLD_OF_REFRACTION_API FPauseMenuHeader BuildHeader(const UWorld *World);

	/** The local party, in order. */
	WORLD_OF_REFRACTION_API TArray<FPauseMenuMember> BuildParty(const UWorld *World);

	/** Quit, after the hold: in a run the run is abandoned (nothing kept, back to the hub); outside one the
	 *  game saves and closes. TODO: both go to the title screen once it exists (UIScreens #9). */
	WORLD_OF_REFRACTION_API void Quit(UWorld *World, APlayerController *PC);

	WORLD_OF_REFRACTION_API FString GetPauseMenuString(const UWorld *World);
}
