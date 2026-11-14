// InviteScreenModel.h
// B-459b (D-200, D-181; UIScreens #31, mockup screen_mockups/coop/session/1_invite.png): what the "Play together"
// screen shows, with no widget in it, so the specs read it headless. Left: the friends list (A invites; one invited
// this visit reads "invited · waiting"); the Null subsystem (LAN) has no list and says how a friend finds the session.
// Right: the party's seats (who plays each: you, a friend by name, an AI seat, one held for a friend who left), then
// Host a session / Stop hosting. UInviteScreenWidget draws it. Debug: wor.UI.PrintInvite logs GetInviteScreenString.

#pragma once

#include "CoreMinimal.h"

class UWorld;

struct FInviteFriendRow
{
	FText Name;
	/** "invite" / "invited · waiting". */
	FText Status;
	bool bInvited = false;
};

struct FInviteSeatRow
{
	FText Name;
	/** "You (host)", "You", "Kestrel", "AI seat", "AI seat · held". */
	FText Who;
	bool bYours = false;
};

struct FInviteScreenPage
{
	/** "Play together". */
	FText Title;
	/** "You host a session: friends can join" / "No session: host one so friends can join". */
	FText Subtitle;
	bool bHosting = false;
	TArray<FInviteFriendRow> Friends;
	/** Under the friends: the empty-list line (LAN), then how seating works. */
	FText FriendsNote;
	TArray<FInviteSeatRow> Seats;
	/** The last right-hand row: "Stop hosting" / "Host a session". */
	FText SessionAction;
};

namespace WoRInviteScreen
{
	/** One seat's "who": the leader is yours (the host's while hosting); an AI seat says so (held for a friend who
	 *  left); a member a friend claims shows their name; any other member is yours. */
	WORLD_OF_REFRACTION_API FText GetSeatWho(bool bLeader, bool bAISeat, bool bHeld, const FString &ClaimantName, bool bHosting);

	/** The page from its parts (the specs build it so). */
	WORLD_OF_REFRACTION_API FInviteScreenPage BuildPage(const TArray<FString> &FriendNames, const TSet<int32> &Invited, bool bHosting,
														TArray<FInviteSeatRow> Seats);

	/** The page for World: the session's friends and the local party's seats. */
	WORLD_OF_REFRACTION_API FInviteScreenPage BuildPage(UWorld *World, const TSet<int32> &Invited);

	/** A on a friend: invites them. Empty on success; else the reason, for the player. */
	WORLD_OF_REFRACTION_API FText Invite(UWorld *World, int32 FriendIndex);

	/** A on the session row: hosts (the level reopens for friends) or stops hosting. Empty on success; else the reason. */
	WORLD_OF_REFRACTION_API FText ToggleHosting(UWorld *World);

	/** Asks the session for the friends list again (the screen refreshes when it lands). */
	WORLD_OF_REFRACTION_API void ReadFriends(UWorld *World);

	WORLD_OF_REFRACTION_API FString GetInviteScreenString(UWorld *World);
}
