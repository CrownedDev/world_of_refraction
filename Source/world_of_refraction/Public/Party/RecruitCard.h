// RecruitCard.h
// B-471 (D-179; UIScreens.md #29, mockup hub/hud/3_recruit_card.png), first part: the recruit card's model. A character
// met in a trial offers to join for this run; the card says who they are, the rule in one line, who plays them (NPC or
// Player) and, with a full party, which NPC can step out. A real player is never swapped out; if every other slot is a
// player the offer is refused. The leader's slot (yours) never steps out. The card's widget is B-471b.

#pragma once

#include "CoreMinimal.h"

struct FPartyMember;
class UCharacterData;

enum class ERecruitPlayedBy : uint8
{
	NPC,
	Player
};

/** A party member who could step out for the recruit (an NPC slot). */
struct FRecruitStepOutChoice
{
	int32 SlotIndex = INDEX_NONE;
	FText Name;
};

struct FRecruitCard
{
	FText Name;
	FText ClassName;
	/** "Joins for this run only and uses what your party carries." (D-026, B-361) */
	FText RuleLine;
	/** The recruit's own "Played by" (an NPC when met; a friend may take the seat later, D-150). */
	ERecruitPlayedBy PlayedBy = ERecruitPlayedBy::NPC;
	FText PlayedByText;
	bool bPartyFull = false;
	/** With a full party: the NPC slots that can step out (empty when the party has room). */
	TArray<FRecruitStepOutChoice> StepOut;
	/** Why the offer can't be taken (a full party of players, no recruit); empty when it can. */
	FText Refusal;
};

namespace WoRRecruitCard
{
	/** "NPC" or "Player". */
	WORLD_OF_REFRACTION_API FText GetPlayedByText(ERecruitPlayedBy PlayedBy);

	/** The card for Recruit with the party's Members. IsPlayerSlot says which slots a real player holds (the leader's is
	 *  always treated as one: it never steps out). */
	WORLD_OF_REFRACTION_API FRecruitCard Build(const UCharacterData *Recruit, const TArray<FPartyMember> &Members,
											   TFunctionRef<bool(int32 SlotIndex)> IsPlayerSlot);

	/** Debug: the card as text (wor.Party.PrintRecruitCard). */
	WORLD_OF_REFRACTION_API FString GetCardString(const FRecruitCard &Card);
}
