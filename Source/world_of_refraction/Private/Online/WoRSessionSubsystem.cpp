// WoRSessionSubsystem.cpp

#include "Online/WoRSessionSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h" // SEARCH_LOBBIES
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "WoRLog.h"

namespace
{
	constexpr int32 SESSION_SEARCH_MAX_RESULTS = 20;
	const FName SESSION_NULL_SUBSYSTEM(TEXT("NULL"));
	/** The platform's default friends list (EFriendsLists::Default spells it "default"). */
	const FString FRIENDS_LIST_NAME(TEXT("default"));
}

FOnlineSessionSettings WoRSession::MakeHostSettings(bool bLAN)
{
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = MAX_PLAYERS;
	Settings.bIsLANMatch = bLAN;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bUsesPresence = !bLAN;
	Settings.bUseLobbiesIfAvailable = !bLAN;
	Settings.bAllowInvites = true;
	return Settings;
}

IOnlineSessionPtr UWoRSessionSubsystem::GetSessionInterface() const
{
	const IOnlineSubsystem *Online = Online::GetSubsystem(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	return Online ? Online->GetSessionInterface() : nullptr;
}

bool UWoRSessionSubsystem::IsLAN() const
{
	const IOnlineSubsystem *Online = Online::GetSubsystem(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	return !Online || Online->GetSubsystemName() == SESSION_NULL_SUBSYSTEM;
}

bool UWoRSessionSubsystem::HostSession()
{
	EnsureInviteBinding();
	IOnlineSessionPtr Sessions = GetSessionInterface();
	const ULocalPlayer *Player = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!Sessions.IsValid() || !Player)
	{
		UE_LOG(LogWoR, Warning, TEXT("[Session] Host: no session interface or no local player"));
		return false;
	}
	if (CreateHandle.IsValid())
	{
		UE_LOG(LogWoR, Warning, TEXT("[Session] Host: already starting one"));
		return false;
	}
	// Review fix: one session at a time; leave first (on EOS a destroy finishes later, so destroy-then-create fails).
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		UE_LOG(LogWoR, Warning, TEXT("[Session] Host: a session already exists (wor.Session.Leave first)"));
		return false;
	}
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UWoRSessionSubsystem::HandleCreateComplete));
	const FOnlineSessionSettings Settings = WoRSession::MakeHostSettings(IsLAN());
	if (!Sessions->CreateSession(Player->GetControllerId(), NAME_GameSession, Settings))
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		UE_LOG(LogWoR, Warning, TEXT("[Session] Host: CreateSession refused"));
		return false;
	}
	return true;
}

void UWoRSessionSubsystem::HandleCreateComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	}
	UWorld *World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	UE_LOG(LogWoR, Log, TEXT("[Session] Hosted %s: %s"), *SessionName.ToString(), bWasSuccessful ? TEXT("yes") : TEXT("no"));
	if (!bWasSuccessful || !World)
	{
		OnSessionCreated.Broadcast(false);
		return;
	}
	bHosting = true;
	OnSessionCreated.Broadcast(true); // B-234b review: before the reopen, so a hand-over's run waits for the new world
	// The current level again as a listen server, so friends can join (the hub, D-103).
	// Review fix: OpenLevel, not ServerTravel: the game modes use seamless travel, which opens no listen socket.
	UGameplayStatics::OpenLevel(World, FName(*UGameplayStatics::GetCurrentLevelName(World, /*bRemovePrefixString=*/true)),
								/*bAbsolute=*/true, TEXT("listen"));
}

bool UWoRSessionSubsystem::FindSessions()
{
	EnsureInviteBinding();
	IOnlineSessionPtr Sessions = GetSessionInterface();
	const ULocalPlayer *Player = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!Sessions.IsValid() || !Player)
	{
		UE_LOG(LogWoR, Warning, TEXT("[Session] Find: no session interface or no local player"));
		return false;
	}
	if (FindHandle.IsValid())
	{
		UE_LOG(LogWoR, Warning, TEXT("[Session] Find: a search is still running"));
		return false;
	}
	LastSearch = MakeShared<FOnlineSessionSearch>();
	LastSearch->bIsLanQuery = IsLAN();
	LastSearch->MaxSearchResults = SESSION_SEARCH_MAX_RESULTS;
	if (!IsLAN())
	{
		LastSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UWoRSessionSubsystem::HandleFindComplete));
	if (!Sessions->FindSessions(Player->GetControllerId(), LastSearch.ToSharedRef()))
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		UE_LOG(LogWoR, Warning, TEXT("[Session] Find: FindSessions refused"));
		return false;
	}
	return true;
}

void UWoRSessionSubsystem::HandleFindComplete(bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	}
	UE_LOG(LogWoR, Display, TEXT("[Session] Find %s:%s"), bWasSuccessful ? TEXT("done") : TEXT("failed"), *GetSessionString());
	OnSessionsFound.Broadcast(); // B-234b: a hand-over looks for the next host here
}

bool UWoRSessionSubsystem::JoinFoundSession(int32 Index)
{
	if (!LastSearch.IsValid() || !LastSearch->SearchResults.IsValidIndex(Index))
	{
		UE_LOG(LogWoR, Warning, TEXT("[Session] Join: no session %d (run wor.Session.Find first)"), Index);
		return false;
	}
	return JoinResult(LastSearch->SearchResults[Index]);
}

bool UWoRSessionSubsystem::JoinResult(const FOnlineSessionSearchResult &Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	const ULocalPlayer *Player = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!Sessions.IsValid() || !Player || !Result.IsValid())
	{
		UE_LOG(LogWoR, Warning, TEXT("[Session] Join: no session interface, no local player or no session"));
		return false;
	}
	if (JoinHandle.IsValid())
	{
		UE_LOG(LogWoR, Warning, TEXT("[Session] Join: already joining"));
		return false;
	}
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UWoRSessionSubsystem::HandleJoinComplete));
	if (!Sessions->JoinSession(Player->GetControllerId(), NAME_GameSession, Result))
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle); // review fix: no stale handler on the next join
		UE_LOG(LogWoR, Warning, TEXT("[Session] Join: JoinSession refused"));
		return false;
	}
	return true;
}

void UWoRSessionSubsystem::HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}
	FString Address;
	APlayerController *PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions.IsValid() || !Sessions->GetResolvedConnectString(SessionName, Address) || !PC)
	{
		UE_LOG(LogWoR, Warning, TEXT("[Session] Join failed (%d)"), static_cast<int32>(Result));
		OnSessionJoined.Broadcast(false);
		return;
	}
	UE_LOG(LogWoR, Log, TEXT("[Session] Joined; travelling to %s"), *Address);
	OnSessionJoined.Broadcast(true);
	PC->ClientTravel(Address, TRAVEL_Absolute);
}

void UWoRSessionSubsystem::Deinitialize()
{
	// Review fix: nothing calls back into a gone subsystem, and the session doesn't outlive the game instance.
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
		if (Sessions->GetNamedSession(NAME_GameSession))
		{
			Sessions->DestroySession(NAME_GameSession);
		}
	}
	Super::Deinitialize();
}

void UWoRSessionSubsystem::LeaveSession()
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->DestroySession(NAME_GameSession);
	}
	bHosting = false;
}

int32 UWoRSessionSubsystem::GetFoundSessionCount() const
{
	return LastSearch.IsValid() ? LastSearch->SearchResults.Num() : 0;
}

int32 UWoRSessionSubsystem::FindSessionIndexByOwner(const FString &OwnerId) const
{
	for (int32 Index = 0; LastSearch.IsValid() && !OwnerId.IsEmpty() && Index < LastSearch->SearchResults.Num(); ++Index)
	{
		const FUniqueNetIdPtr &Owner = LastSearch->SearchResults[Index].Session.OwningUserId;
		if (Owner.IsValid() && Owner->ToString() == OwnerId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FString UWoRSessionSubsystem::GetSessionString() const
{
	FString Out = FString::Printf(TEXT(" %s, %s, %d found, %d friend(s)"), IsLAN() ? TEXT("LAN (Null)") : TEXT("online"),
								  bHosting ? TEXT("hosting") : TEXT("not hosting"), GetFoundSessionCount(), Friends.Num());
	for (int32 Index = 0; Index < Friends.Num(); ++Index)
	{
		Out += FString::Printf(TEXT("\n  friend [%d] %s"), Index, *Friends[Index]->GetDisplayName());
	}
	for (int32 Index = 0; LastSearch.IsValid() && Index < LastSearch->SearchResults.Num(); ++Index)
	{
		const FOnlineSessionSearchResult &Result = LastSearch->SearchResults[Index];
		Out += FString::Printf(TEXT("\n  [%d] %s, %d / %d open, ping %d"), Index, *Result.Session.OwningUserName,
							   Result.Session.NumOpenPublicConnections, Result.Session.SessionSettings.NumPublicConnections, Result.PingInMs);
	}
	return Out;
}

bool UWoRSessionSubsystem::HostsSession() const
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	return bHosting && Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

void UWoRSessionSubsystem::EnsureInviteBinding()
{
	if (InviteAcceptedHandle.IsValid())
	{
		return;
	}
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		InviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
			FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UWoRSessionSubsystem::HandleInviteAccepted));
	}
}

void UWoRSessionSubsystem::HandleInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId,
												const FOnlineSessionSearchResult &InviteResult)
{
	UE_LOG(LogWoR, Log, TEXT("[Session] Invite accepted (controller %d): %s"), ControllerId, bWasSuccessful ? TEXT("joining") : TEXT("failed"));
	if (!bWasSuccessful)
	{
		return;
	}
	// One session at a time (as HostSession): a player still in its own session leaves it first.
	if (IOnlineSessionPtr Sessions = GetSessionInterface(); Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession))
	{
		UE_LOG(LogWoR, Warning, TEXT("[Session] Invite: leave your session first (wor.Session.Leave), then accept again"));
		return;
	}
	JoinResult(InviteResult);
}

bool UWoRSessionSubsystem::ReadFriends()
{
	EnsureInviteBinding();
	const IOnlineSubsystem *Online = Online::GetSubsystem(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	IOnlineFriendsPtr FriendsInterface = Online ? Online->GetFriendsInterface() : nullptr;
	const ULocalPlayer *Player = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!FriendsInterface.IsValid() || !Player)
	{
		UE_LOG(LogWoR, Log, TEXT("[Session] Friends: no friends list on %s (LAN friends: wor.Session.Find / Join)"), IsLAN() ? TEXT("LAN") : TEXT("this platform"));
		Friends.Reset();
		OnFriendsRead.Broadcast();
		return false;
	}
	return FriendsInterface->ReadFriendsList(Player->GetControllerId(), FRIENDS_LIST_NAME,
											 FOnReadFriendsListComplete::CreateUObject(this, &UWoRSessionSubsystem::HandleFriendsRead));
}

void UWoRSessionSubsystem::HandleFriendsRead(int32 LocalUserNum, bool bWasSuccessful, const FString &ListName, const FString &ErrorStr)
{
	Friends.Reset();
	const IOnlineSubsystem *Online = Online::GetSubsystem(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	IOnlineFriendsPtr FriendsInterface = Online ? Online->GetFriendsInterface() : nullptr;
	if (bWasSuccessful && FriendsInterface.IsValid())
	{
		FriendsInterface->GetFriendsList(LocalUserNum, ListName, Friends);
	}
	UE_LOG(LogWoR, Log, TEXT("[Session] Friends read: %s, %d friend(s)%s"), bWasSuccessful ? TEXT("yes") : TEXT("no"), Friends.Num(),
		   ErrorStr.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *ErrorStr));
	OnFriendsRead.Broadcast();
}

TArray<FString> UWoRSessionSubsystem::GetFriendNames() const
{
	TArray<FString> Names;
	for (const TSharedRef<FOnlineFriend> &Friend : Friends)
	{
		Names.Add(Friend->GetDisplayName());
	}
	return Names;
}

bool UWoRSessionSubsystem::InviteFriend(int32 Index)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	const ULocalPlayer *Player = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!Friends.IsValidIndex(Index) || !Sessions.IsValid() || !Player)
	{
		UE_LOG(LogWoR, Warning, TEXT("[Session] Invite: no friend %d (wor.Session.Friends first)"), Index);
		return false;
	}
	if (!HostsSession())
	{
		UE_LOG(LogWoR, Warning, TEXT("[Session] Invite: host a session first (wor.Session.Host)"));
		return false;
	}
	const bool bSent = Sessions->SendSessionInviteToFriend(Player->GetControllerId(), NAME_GameSession, *Friends[Index]->GetUserId());
	UE_LOG(LogWoR, Log, TEXT("[Session] Invite to %s: %s"), *Friends[Index]->GetDisplayName(), bSent ? TEXT("sent") : TEXT("refused"));
	return bSent;
}

namespace
{
	UWoRSessionSubsystem *FindSessionSubsystem(UWorld *World)
	{
		UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UWoRSessionSubsystem>() : nullptr;
	}

	FAutoConsoleCommandWithWorld GSessionHostCmd(TEXT("wor.Session.Host"), TEXT("DEBUG (B-233): host a co-op session (LAN until EOS) and reopen this level as a listen server."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { if (UWoRSessionSubsystem *S = FindSessionSubsystem(World)) { S->HostSession(); } }));

	FAutoConsoleCommandWithWorld GSessionFindCmd(TEXT("wor.Session.Find"), TEXT("DEBUG (B-233): search for co-op sessions; then wor.Session.Join <index>."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { if (UWoRSessionSubsystem *S = FindSessionSubsystem(World)) { S->FindSessions(); } }));

	FAutoConsoleCommandWithWorldAndArgs GSessionJoinCmd(TEXT("wor.Session.Join"), TEXT("DEBUG (B-233): join the found session at <index> (default 0)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
		{
			if (UWoRSessionSubsystem *S = FindSessionSubsystem(World))
			{
				S->JoinFoundSession(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0);
			}
		}));

	FAutoConsoleCommandWithWorld GSessionLeaveCmd(TEXT("wor.Session.Leave"), TEXT("DEBUG (B-233): leave the session this machine hosts or joined."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { if (UWoRSessionSubsystem *S = FindSessionSubsystem(World)) { S->LeaveSession(); } }));

	FAutoConsoleCommandWithWorld GSessionFriendsCmd(TEXT("wor.Session.Friends"), TEXT("DEBUG (B-233b): read the friends list (none on LAN); then wor.Session.Invite <index>."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { if (UWoRSessionSubsystem *S = FindSessionSubsystem(World)) { S->ReadFriends(); } }));

	FAutoConsoleCommandWithWorldAndArgs GSessionInviteCmd(TEXT("wor.Session.Invite"), TEXT("DEBUG (B-233b): invite friend <index> (wor.Session.Friends) to the session this machine hosts."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
		{
			if (UWoRSessionSubsystem *S = FindSessionSubsystem(World))
			{
				S->InviteFriend(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0);
			}
		}));

	FAutoConsoleCommandWithWorld GSessionPrintCmd(TEXT("wor.Session.Print"), TEXT("DEBUG (B-233): the session state and the last search's results."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			if (const UWoRSessionSubsystem *S = FindSessionSubsystem(World)) { UE_LOG(LogWoR, Display, TEXT("[Session]%s"), *S->GetSessionString()); }
		}));
}
