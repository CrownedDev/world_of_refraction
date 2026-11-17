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
enum class EPlayerBaseForm : uint8;

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
	/** Null for your own spirit. */
	UCharacterFormData *Form = nullptr;
	/** D-234: one of your own spirits (Male Nobody / Female Nobody, the first two rows): A takes any form off and
	 *  sets this base body. */
	bool bBaseBody = false;
	EPlayerBaseForm BaseBody = EPlayerBaseForm(0);
	/** The form's name, Male Nobody / Female Nobody, or "???" for a character not met. */
	FText Name;
	/** Under the name: "always yours" for your own spirits, else empty (the meter shows). */
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
	/** The page for CharacterName, from the form subsystem, wearing Worn (null: its own spirit) over Body; Owns / Wears
	 *  answer for one reference piece's asset. B-558: the unlocks are the player's, the worn spirit the character's. */
	WORLD_OF_REFRACTION_API FFormsScreenPage BuildPage(const UCharacterFormSubsystem &Forms, const FText &CharacterName,
													   const UCharacterFormData *Worn, EPlayerBaseForm Body,
													   TFunctionRef<bool(const UObject *)> Owns, TFunctionRef<bool(const UObject *)> Wears);

	/** The player's page: the form subsystem's worn spirit and body. */
	WORLD_OF_REFRACTION_API FFormsScreenPage BuildPage(const UCharacterFormSubsystem &Forms, const FText &CharacterName,
													   TFunctionRef<bool(const UObject *)> Owns, TFunctionRef<bool(const UObject *)> Wears);

	/** B-558: the party member at MemberIndex (0: the local player); a recruit (D-238) wears its own spirit. */
	WORLD_OF_REFRACTION_API FFormsScreenPage BuildPage(UWorld *World, int32 MemberIndex = 0);

	/** B-558: how many party members the screen steps through. */
	WORLD_OF_REFRACTION_API int32 GetMemberCount(UWorld *World);

	/** A: MemberIndex wears Row's form (one of your own spirits takes a skin off and sets that body). Empty on success;
	 *  else the reason, for the player. A member that is neither the player nor a recruit can't change spirit. */
	WORLD_OF_REFRACTION_API FText Wear(UWorld *World, const FFormsScreenRow &Row, int32 MemberIndex = 0);

	/** Y: the other base body (Manny / Quinn). Empty on success; else the reason. */
	WORLD_OF_REFRACTION_API FText SwitchBaseBody(UWorld *World, int32 MemberIndex = 0);

	WORLD_OF_REFRACTION_API FString GetFormsScreenString(UWorld *World);
}
