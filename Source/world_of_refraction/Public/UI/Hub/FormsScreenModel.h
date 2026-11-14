// FormsScreenModel.h
// B-444 (D-184, D-181; UIScreens #19, mockup screen_mockups/hub/skins/1_forms.png): what the "Spirit of {character}"
// screen shows, with no widget in it, so the specs read it headless. Your own form first, then every character form
// with its meter; a character you haven't met (no meter and none of their pieces owned) is "???" (Q-444). The side
// panel's pieces are the form's reference loadout: owned or worn ones named, the rest "?", all named once unlocked.
// UFormsScreenWidget draws it. Debug: wor.UI.PrintForms logs GetFormsScreenString.

#pragma once

#include "CoreMinimal.h"

class UCharacterFormData;
class UCharacterFormSubsystem;
class UWorld;

struct FFormsScreenPiece
{
	/** The piece's name; "?" while it is unknown. */
	FText Name;
	bool bKnown = false;
	bool bOwned = false;
	bool bWorn = false;
};

struct FFormsScreenRow
{
	/** Null for your own form. */
	UCharacterFormData *Form = nullptr;
	/** The form's name, "your own form", or "???" for a character not met. */
	FText Name;
	/** Under the name: "always yours" for your own form, else empty (the meter shows). */
	FText Line;
	/** 0..1. */
	float Progress = 0.0f;
	bool bUnlocked = false;
	bool bWorn = false;
	bool bMet = true;
	/** The side panel's line under the percent ("Locked. Wear their pieces…"). */
	FText Status;
	TArray<FFormsScreenPiece> Pieces;
};

struct FFormsScreenPage
{
	/** "Spirit of Sera". */
	FText Title;
	/** "Wearing: your own form (base body Quinn)". */
	FText WearingLine;
	TArray<FFormsScreenRow> Rows;
};

namespace WoRFormsScreen
{
	/** The page for CharacterName, from the form subsystem; Owns / Wears answer for one reference piece's asset. */
	WORLD_OF_REFRACTION_API FFormsScreenPage BuildPage(const UCharacterFormSubsystem &Forms, const FText &CharacterName,
													   TFunctionRef<bool(const UObject *)> Owns, TFunctionRef<bool(const UObject *)> Wears);

	/** The page for the local player's character (the party's first member). */
	WORLD_OF_REFRACTION_API FFormsScreenPage BuildPage(UWorld *World);

	/** A: wears Row's form (your own form takes a skin off). Empty on success; else the reason, for the player. */
	WORLD_OF_REFRACTION_API FText Wear(UWorld *World, const FFormsScreenRow &Row);

	/** Y: the other base body (Manny / Quinn). Empty on success; else the reason. */
	WORLD_OF_REFRACTION_API FText SwitchBaseBody(UWorld *World);

	WORLD_OF_REFRACTION_API FString GetFormsScreenString(UWorld *World);
}
