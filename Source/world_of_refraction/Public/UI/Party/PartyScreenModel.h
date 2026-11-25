// PartyScreenModel.h
// B-458 (D-200, D-181; UIScreens #7, mockup screen_mockups/menus/party/1_party.png): what the party screen shows,
// with no widget in it, so the specs read it headless. One row per member: name, class, who plays it ("You (leader)",
// "You", or "AI" with no level text, D-200), the row it starts in and its field skill; then the "Invite" row. The side
// panel's 3 x 3 grid is the members' start cells. "When your fighters fall" is left out: B-346 retired it (Q-458).
// UPartyScreenWidget draws it. Debug: wor.UI.PrintParty logs GetPartyScreenString.
// B-628 (mockup menus/party/v2, Q-628): each member's active loadout (BuildLoadout) and Dismiss; no Invite row.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Grid/ECombatRow.h"

class UParty;
class UWorld;
struct FLoadoutV2SkillsPage;
struct FLoadoutV2WeaponsPage;

struct FPartyScreenMember
{
	FText Name;
	/** "Refractor", "Resonator", "Generic". */
	FText ClassName;
	/** "You (leader)", "You", "AI". */
	FText PlayedBy;
	/** "Starts: Front Row, top", or "Starts: Auto" with no cell set. */
	FText StartLine;
	/** B-628: the same without "Starts:", for Details: "Front Row, top", "Auto". */
	FText StartValue;
	/** "Field skill: Dash". */
	FText FieldSkillLine;
	/** "Weapon guard" / "Unarmed guard". */
	FText GuardLine;
	/** B-628b (the cards the Pause menu dropped, mockup menus/pause v1): HP and EP now (full with none carried, as the
	 *  party strip) and their maximums; the bars show only when a maximum reads above 0. */
	int32 HP = 0;
	int32 MaxHP = 0;
	int32 EP = 0;
	int32 MaxEP = 0;
	/** B-628b: the live Mind / Body / Spirit (BuildPage(World) only: they need the world). */
	int32 Mind = 0;
	int32 Body = 0;
	int32 Spirit = 0;
	/** B-628b: the active loadout's first piece ("Arming Sword", "Innate spells"); empty with no loadout here. */
	FText Weapon;
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
	/** The Invite row's value: "no session" / "hosting". B-628: the screen shows no Invite row (Q-628); the debug line keeps it. */
	FText InviteStatus;
};

/** B-628 (mockup menus/party/v2): one entry of a member's active loadout, in the middle column. */
struct FPartyScreenLoadoutEntry
{
	/** "Arming Sword", "Innate spells". */
	FText Name;
	/** "Sword (F)": the piece's type and tier; empty for Innate spells. */
	FText TypeLine;
	/** The slotted skills, "Force Strike, Rally"; "No skills slotted"; empty for a piece with no skill slots. */
	FText SkillsLine;
};

/** B-628: the member's active loadout only (Crown). */
struct FPartyScreenLoadout
{
	/** "Bruiser (1 of 2)" (WoRLoadoutScreen::FormatLoadoutTitle); empty with no loadout. */
	FText Title;
	TArray<FPartyScreenLoadoutEntry> Entries;
};

namespace WoRPartyScreen
{
	/** The page for Party (null: no members). */
	WORLD_OF_REFRACTION_API FPartyScreenPage BuildPage(const UParty *Party, bool bHosting);

	/** The page for the local party. */
	WORLD_OF_REFRACTION_API FPartyScreenPage BuildPage(UWorld *World);

	/** X: the start cell after (Row, Column) in the grid's order; an unplaced member starts at Back, 0. */
	WORLD_OF_REFRACTION_API void GetNextCell(bool bHasCell, ECombatRow Row, int32 Column, ECombatRow &OutRow, int32 &OutColumn);

	/** Left / Right on a member (B-628; A before): a player's seat becomes the AI's, or back. Empty on success; else the reason, for the player. */
	WORLD_OF_REFRACTION_API FText ToggleSeat(UWorld *World, int32 SlotIndex);

	/** A on a member (B-628; X before): moves it to the next start cell (a member already there swaps, SetMemberFormation). */
	WORLD_OF_REFRACTION_API void MoveOnGrid(UWorld *World, int32 SlotIndex);

	/** B-628: the loadout's entries: each piece of the Weapons page with its slotted skills, then Innate spells. */
	WORLD_OF_REFRACTION_API FPartyScreenLoadout BuildLoadout(const FText &Title, const FLoadoutV2WeaponsPage &Weapons, const FLoadoutV2SkillsPage &Skills);

	/** B-628: the active loadout of the member in SlotIndex; empty when it has none here. */
	WORLD_OF_REFRACTION_API FPartyScreenLoadout BuildLoadout(UWorld *World, int32 SlotIndex);

	/** B-628: X twice on a member: it leaves the party. Empty on success; else the reason (the leader, a friend's seat,
	 *  during a fight). */
	WORLD_OF_REFRACTION_API FText Dismiss(UWorld *World, int32 SlotIndex);

	WORLD_OF_REFRACTION_API FString GetPartyScreenString(UWorld *World);
}
