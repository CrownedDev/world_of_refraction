// HubHUDModel.h
// B-443 (D-181, UIScreens #14, mockup screen_mockups/hub/hud/1_hub_hud_prompt.png): what the hub and trial-map
// HUD shows, with no widget in it, so the specs read it headless. UHubHUDWidget draws it.
//
// Two things beside the party strip (which exists): the floating interact prompt by what the player faces
// (key + verb + what: "X Talk Blacksmith"), and the money chip, shown only near a vendor or just after the
// balance changed.
// B-465b: the small trial HUD (Prisms, wipes left, the tracked quest step), bottom left in a run.
// Debug: wor.UI.PrintHubHUD logs GetHubHUDString.

#pragma once

#include "CoreMinimal.h"
#include "Hub/InteractPrompt.h"

class APawn;
class UWorld;
enum class ECurrencyType : uint8;

struct FHubHUDPrompt
{
	/** False: nothing in reach has a prompt. */
	bool bShown = false;
	/** "Talk", "Rest". */
	FText Verb;
	/** "Blacksmith"; may be empty. */
	FText What;
	/** Where the thing stands; the prompt floats above it. */
	FVector WorldLocation = FVector::ZeroVector;
	/** The nearest thing in reach is a vendor (the money shows). */
	bool bNearVendor = false;
};

struct FHubHUDMoney
{
	ECurrencyType Currency = ECurrencyType(0);
	FText Value;
	FText Label;
};

/** B-465b (UIScreens #28, mockup run/run_end/1_reward_popups.png): the small trial HUD, bottom left, in a run. */
struct FTrialHUDStatus
{
	/** False outside a run: the panel hides. */
	bool bInRun = false;
	/** The run Prisms, as the money chip words them ("1,420", "Prisms"). */
	FHubHUDMoney Prisms;
	int32 WipesLeft = 0;
	int32 WipesMax = 0;
	/** "Wipes left 3 of 5". */
	FText WipesLine;
	/** The tracked quest's step (BuildQuestLine); may be empty. */
	FText QuestLine;
};

namespace WoRHubHUD
{
	/** How long the money stays up after it changed. TODO: tune */
	inline constexpr float MONEY_SHOWN_SECONDS = 4.0f;

	/** How far above the thing's origin the prompt floats. TODO: tune per interactable (a head height) */
	inline constexpr float PROMPT_HEIGHT_CM = 110.0f;

	/** The prompt for the nearest thing in Pawn's reach. A nearest thing with no prompt (a memory spot) shows none,
	 *  and doesn't fall through to a farther one: the key would act on the nearest. */
	WORLD_OF_REFRACTION_API FHubHUDPrompt BuildPrompt(APawn *Pawn);

	/** The interact key's label for the device in use: "X" on a gamepad, "E" on the keyboard (InputMap.md). */
	WORLD_OF_REFRACTION_API FText GetInteractKeyLabel(bool bGamepad);

	/** The money the place spends: Gold in the hub, Prisms in a run (D-159). */
	WORLD_OF_REFRACTION_API FHubHUDMoney BuildMoney(const UWorld *World);

	/** The money shows near a vendor, or for MONEY_SHOWN_SECONDS after it changed. */
	WORLD_OF_REFRACTION_API bool ShouldShowMoney(bool bNearVendor, float SecondsSinceChange);

	/** B-463b (UIScreens #28): in a run, the tracked quest's "Title: current goal"; empty in the hub or with none. */
	WORLD_OF_REFRACTION_API FText BuildQuestLine(const UWorld *World);

	/** B-465b: the trial HUD's Prisms, wipes left and tracked quest step; bInRun false outside a run. */
	WORLD_OF_REFRACTION_API FTrialHUDStatus BuildTrialStatus(const UWorld *World);

	WORLD_OF_REFRACTION_API FString GetHubHUDString(const UWorld *World);
}
