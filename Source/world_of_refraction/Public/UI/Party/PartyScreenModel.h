// PartyScreenModel.h
// B-458 (D-200, D-181; UIScreens #7, mockup screen_mockups/menus/party/1_party.png): what the party screen shows,
// with no widget in it, so the specs read it headless. One row per member: name, class, who plays it ("You (leader)",
// "You", or "AI" with no level text, D-200), the row it starts in and its field skill; then the "Invite" row. The side
// panel's 3 x 3 grid is the members' start cells. "When your fighters fall" is left out: B-346 retired it (Q-458).
// UPartyScreenWidget draws it. Debug: wor.UI.PrintParty logs GetPartyScreenString.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Grid/ECombatRow.h"

class UParty;
class UWorld;

struct FPartyScreenMember
{
	FText Name;
	/** "Refractor", "Resonator", "Generic". */
	FText ClassName;
	/** "You (leader)", "You", "AI". */
	FText PlayedBy;
	/** "Starts: Front Row", or "Starts: placed for you" with no cell set. */
	FText StartLine;
	/** "Field skill: Dash". */
	FText FieldSkillLine;
	/** "Weapon guard" / "Unarmed guard". */
	FText GuardLine;
	bool bLeader = false;
	bool bAISeat = false;
	bool bFallen = false;
	/** The start cell; meaningful only with bHasCell. */
	bool bHasCell = false;
	ECombatRow Row = ECombatRow::Middle;
	int32 Column = 0;
};

struct FPartyScreenPage
{
	/** "Party". */
	FText Title;
	/** "Up to 3 fight together". */
	FText Subtitle;
	TArray<FPartyScreenMember> Members;
	/** The Invite row's value: "no session" / "hosting". */
	FText InviteStatus;
};

namespace WoRPartyScreen
{
	/** The page for Party (null: no members). */
	WORLD_OF_REFRACTION_API FPartyScreenPage BuildPage(const UParty *Party, bool bHosting);

	/** The page for the local party. */
	WORLD_OF_REFRACTION_API FPartyScreenPage BuildPage(UWorld *World);

	/** X: the start cell after (Row, Column) in the grid's order; an unplaced member starts at Back, 0. */
	WORLD_OF_REFRACTION_API void GetNextCell(bool bHasCell, ECombatRow Row, int32 Column, ECombatRow &OutRow, int32 &OutColumn);

	/** A on a member: a player's seat becomes the AI's, or back. Empty on success; else the reason, for the player. */
	WORLD_OF_REFRACTION_API FText ToggleSeat(UWorld *World, int32 SlotIndex);

	/** X on a member: moves it to the next start cell (a member already there swaps, SetMemberFormation). */
	WORLD_OF_REFRACTION_API void MoveOnGrid(UWorld *World, int32 SlotIndex);

	WORLD_OF_REFRACTION_API FString GetPartyScreenString(UWorld *World);
}
