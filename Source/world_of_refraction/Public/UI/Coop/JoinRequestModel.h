// JoinRequestModel.h
// B-459c (D-150, D-200; UIScreens #32, mockup screen_mockups/coop/session/2_join_request.png): what the host's join
// request card shows, with no widget in it, so the specs read it headless. The first waiting request: who asks, with
// which character; the party's AI seats (A gives the lit one), X refuses. UJoinRequestCardWidget draws it in the corner
// of the host's screen. Debug: wor.UI.PrintJoinRequest logs GetCardString.

#pragma once

#include "CoreMinimal.h"

class UCharacterData;
class UWorld;

struct FJoinRequestSeatRow
{
	/** The party slot the seat is (never the leader's). */
	int32 Slot = INDEX_NONE;
	FText Name;
	/** "AI seat · give" / "AI seat · held · give". */
	FText Status;
};

struct FJoinRequestCard
{
	bool bShown = false;
	/** Whose request the card answers (FRunJoinRequest::PlayerId). */
	int32 PlayerId = INDEX_NONE;
	/** "Kestrel wants to join". */
	FText Title;
	/** "with Vael · Wind Caster". */
	FText WithLine;
	TArray<FJoinRequestSeatRow> Seats;
	/** "Vael replaces the seat's character for this run." */
	FText ReplaceLine;
	/** Empty, or why a seat can't be given now / how many more wait. */
	FText Note;
	/** "Hold View to answer · then A Give this seat  X Refuse" / "B Close  A Give this seat  X Refuse". */
	FText Hints;
};

namespace WoRJoinRequestCard
{
	/** One AI seat's status: held for a friend who left, or free. */
	WORLD_OF_REFRACTION_API FText GetSeatStatus(bool bHeld);

	/** The card from its parts (the specs build it so). MoreWaiting: requests after this one. bSeatOpen: a new joiner can
	 *  be seated now (RunJoinRules::CanSeatJoiner); bInFight: seats are fixed until the fight ends. */
	WORLD_OF_REFRACTION_API FJoinRequestCard BuildCard(int32 PlayerId, const FString &PlayerName, const UCharacterData *Character,
													   TArray<FJoinRequestSeatRow> Seats, int32 MoreWaiting, bool bSeatOpen,
													   bool bInFight, bool bFocused);

	/** The card for World (the host's run component's first request); bShown false with none, or off the host. */
	WORLD_OF_REFRACTION_API FJoinRequestCard BuildCard(UWorld *World, bool bFocused);

	/** A: PlayerId's request takes AI seat Slot. Empty on success; else the reason, for the player. */
	WORLD_OF_REFRACTION_API FText Give(UWorld *World, int32 PlayerId, int32 Slot);

	/** X: drop PlayerId's request. Empty on success; else the reason. */
	WORLD_OF_REFRACTION_API FText Refuse(UWorld *World, int32 PlayerId);

	WORLD_OF_REFRACTION_API FString GetCardString(UWorld *World);
}
