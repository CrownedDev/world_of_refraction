// WoRSessionSubsystem.cpp

#include "Online/WoRSessionSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
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
	IOnlineSessionPtr Sessions = GetSessionInterface();
	const ULocalPlayer *Player = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!Sessions.IsValid() || !Player || !LastSearch.IsValid() || !LastSearch->SearchResults.IsValidIndex(Index))
	{
		UE_LOG(LogWoR, Warning, TEXT("[Session] Join: no session %d (run wor.Session.Find first)"), Index);
		return false;
	}
	if (JoinHandle.IsValid())
	{
		UE_LOG(LogWoR, Warning, TEXT("[Session] Join: already joining"));
		return false;
	}
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UWoRSessionSubsystem::HandleJoinComplete));
	if (!Sessions->JoinSession(Player->GetControllerId(), NAME_GameSession, LastSearch->SearchResults[Index]))
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
	FString Out = FString::Printf(TEXT(" %s, %s, %d found"), IsLAN() ? TEXT("LAN (Null)") : TEXT("online"),
								  bHosting ? TEXT("hosting") : TEXT("not hosting"), GetFoundSessionCount());
	for (int32 Index = 0; LastSearch.IsValid() && Index < LastSearch->SearchResults.Num(); ++Index)
	{
		const FOnlineSessionSearchResult &Result = LastSearch->SearchResults[Index];
		Out += FString::Printf(TEXT("\n  [%d] %s, %d / %d open, ping %d"), Index, *Result.Session.OwningUserName,
							   Result.Session.NumOpenPublicConnections, Result.Session.SessionSettings.NumPublicConnections, Result.PingInMs);
	}
	return Out;
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

	FAutoConsoleCommandWithWorld GSessionPrintCmd(TEXT("wor.Session.Print"), TEXT("DEBUG (B-233): the session state and the last search's results."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			if (const UWoRSessionSubsystem *S = FindSessionSubsystem(World)) { UE_LOG(LogWoR, Display, TEXT("[Session]%s"), *S->GetSessionString()); }
		}));
}
