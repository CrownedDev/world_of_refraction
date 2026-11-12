// WoRSessionSubsystem.h
// B-233 part 1 (D-103, Q-233): host, find and join a co-op session through the online subsystem's session interface.
// The Null subsystem (LAN) until EOS is configured (DefaultEngine.ini [OnlineSubsystem]); the code is the same for
// EOS. Hosting opens the current hub level as a listen server; joining client-travels to the host.
// B-233b (Q-233b): invites from the platform's friends list (ReadFriends / InviteFriend; the Null subsystem has none, so
// LAN friends use Find); an accepted invite joins. The session stays open to joins (a hand-over's rejoin needs it); in a
// run the host seats a new friend only at a rest spot (URunStateComponent::AcceptJoin). EOS config is B-233c.
// Debug: wor.Session.Host / Find / Join <index> / Leave / Print (GetSessionString), wor.Session.Friends / Invite <index>.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "WoRSessionSubsystem.generated.h"

namespace WoRSession
{
	/** D-150: three at most (the host and two friends). */
	constexpr int32 MAX_PLAYERS = 3;

	/** The settings a hosted co-op session advertises. bLAN for the Null subsystem. */
	WORLD_OF_REFRACTION_API FOnlineSessionSettings MakeHostSettings(bool bLAN);
}

/** B-234b review: a host or join attempt finished (true: hosting / travelling to the host). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnWoRSessionResult, bool /*bSucceeded*/);

UCLASS()
class WORLD_OF_REFRACTION_API UWoRSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Creates the session, then reopens the current level as a listen server. False when it can't start, one is
	 *  already starting, or a session exists (leave it first). */
	bool HostSession();

	/** Searches for sessions; the results land in GetFoundSessions (logged). False when it can't start. */
	bool FindSessions();

	/** Joins the found session at Index, then travels to it. False for a bad index or no session interface. */
	bool JoinFoundSession(int32 Index);

	/** Destroys the session this machine hosts or joined. */
	void LeaveSession();

	int32 GetFoundSessionCount() const;

	/** B-234b: the found session OwnerId hosts (its owning user id, as FUniqueNetIdRepl::ToString spells it), or
	 *  INDEX_NONE. */
	int32 FindSessionIndexByOwner(const FString &OwnerId) const;

	/** B-234b: a search finished (found or not); GetFoundSessionCount / FindSessionIndexByOwner read it. */
	FSimpleMulticastDelegate OnSessionsFound;

	/** B-234b review: HostSession's create finished; on success it fires just before the level reopens as a listen
	 *  server (so a hand-over adopts its copy only once hosting works). */
	FOnWoRSessionResult OnSessionCreated;

	/** B-234b review: JoinFoundSession's join finished; on success the client is travelling to the host. */
	FOnWoRSessionResult OnSessionJoined;

	FString GetSessionString() const;

	// ---- B-233b: invites ----

	/** Reads the local player's friends list (OnFriendsRead fires when it lands). False with no friends interface (the
	 *  Null subsystem: LAN friends use Find). */
	bool ReadFriends();
	/** The friends the last read found, by display name. */
	TArray<FString> GetFriendNames() const;
	/** Invites friend Index (GetFriendNames) to the hosted session. False with no session hosted or a bad index. */
	bool InviteFriend(int32 Index);
	/** This machine hosts a live session. */
	bool IsHosting() const { return HostsSession(); }
	FSimpleMulticastDelegate OnFriendsRead;

private:
	IOnlineSessionPtr GetSessionInterface() const;
	bool IsLAN() const;

	void HandleCreateComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindComplete(bool bWasSuccessful);
	void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	/** Joins Result, then travels to it (HandleJoinComplete). Shared by JoinFoundSession and an accepted invite. */
	bool JoinResult(const FOnlineSessionSearchResult &Result);
	/** B-233b: binds the platform's invite-accepted event once, when a session interface exists. */
	void EnsureInviteBinding();
	void HandleInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult &InviteResult);
	void HandleFriendsRead(int32 LocalUserNum, bool bWasSuccessful, const FString &ListName, const FString &ErrorStr);
	bool HostsSession() const;

	TSharedPtr<FOnlineSessionSearch> LastSearch;
	FDelegateHandle CreateHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle InviteAcceptedHandle;
	bool bHosting = false;
	TArray<TSharedRef<FOnlineFriend>> Friends;
};
