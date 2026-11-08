// HostSuccession.cpp

#include "Online/HostSuccession.h"
#include "Online/WoRSessionSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

FString WoRHostSuccession::PickNextHost(const TArray<FString> &Order, const FString &OldHost)
{
	for (const FString &Player : Order)
	{
		if (!Player.IsEmpty() && Player != OldHost)
		{
			return Player;
		}
	}
	return FString();
}

bool WoRHostSuccession::IsHostLoss(ENetworkFailure::Type FailureType)
{
	return FailureType == ENetworkFailure::ConnectionLost || FailureType == ENetworkFailure::ConnectionTimeout;
}

void UWoRHostHandOverSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UWoRHostHandOverSubsystem::HandleNetworkFailure);
	}
}

void UWoRHostHandOverSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
	if (UWoRSessionSubsystem *Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWoRSessionSubsystem>() : nullptr)
	{
		Sessions->OnSessionsFound.Remove(SessionsFoundHandle);
	}
	Super::Deinitialize();
}

void UWoRHostHandOverSubsystem::NoteHostOrder(const TArray<FString> &Order)
{
	KnownOrder = Order;
}

FString UWoRHostHandOverSubsystem::GetLocalPlayerId() const
{
	const ULocalPlayer *Player = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	return Player ? Player->GetPreferredUniqueNetId().ToString() : FString();
}

bool UWoRHostHandOverSubsystem::BeginHandOver()
{
	if (KnownOrder.Num() < 2 || KnownOrder[0] == GetLocalPlayerId())
	{
		UE_LOG(LogWoR, Log, TEXT("[HandOver] Not started: %s"), KnownOrder.Num() < 2 ? TEXT("no other player known") : TEXT("this machine was the host"));
		return false;
	}
	OldHost = KnownOrder[0];
	NextHost = WoRHostSuccession::PickNextHost(KnownOrder, OldHost);
	if (NextHost.IsEmpty())
	{
		Step = EHostHandOverStep::GaveUp;
		return false;
	}
	const bool bTakeOver = NextHost == GetLocalPlayerId();
	Step = bTakeOver ? EHostHandOverStep::TakeOverPending : EHostHandOverStep::Rejoining;
	RejoinTries = 0;
	UE_LOG(LogWoR, Display, TEXT("[HandOver] Host %s dropped; %s hosts next (%s)"), *OldHost, *NextHost, bTakeOver ? TEXT("this machine") : TEXT("rejoining it"));
	// The lost connection travels to the default map; carry on once it is up.
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
	WorldInitHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UWoRHostHandOverSubsystem::HandleWorldActorsInitialized);
	return true;
}

void UWoRHostHandOverSubsystem::HandleNetworkFailure(UWorld *World, UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString &Error)
{
	if (!World || World->GetGameInstance() != GetGameInstance() || World->GetNetMode() != NM_Client || !WoRHostSuccession::IsHostLoss(FailureType))
	{
		return;
	}
	if (Step != EHostHandOverStep::None && Step != EHostHandOverStep::GaveUp)
	{
		return; // already handing over
	}
	UE_LOG(LogWoR, Warning, TEXT("[HandOver] Lost the host (%s): %s"), ENetworkFailure::ToString(FailureType), *Error);
	BeginHandOver();
}

void UWoRHostHandOverSubsystem::HandleWorldActorsInitialized(const FActorsInitializedParams &Params)
{
	UWorld *World = Params.World;
	if (!World || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance() || World->GetNetMode() == NM_Client)
	{
		return; // still connected somewhere, or not this game's world
	}
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
	WorldInitHandle.Reset();
	ContinueHandOver(World);
}

void UWoRHostHandOverSubsystem::ContinueHandOver(UWorld *World)
{
	// Not inside world initialisation: the session calls and the level change wait for the next tick.
	if (Step == EHostHandOverStep::TakeOverPending)
	{
		World->GetTimerManager().SetTimer(RetryTimer, FTimerDelegate::CreateWeakLambda(this, [this, World]() { TakeOver(World); }),
										  WoRHostSuccession::TAKE_OVER_DELAY_SECONDS, false);
	}
	else if (Step == EHostHandOverStep::Rejoining)
	{
		World->GetTimerManager().SetTimer(RetryTimer, FTimerDelegate::CreateUObject(this, &UWoRHostHandOverSubsystem::TryRejoin),
										  WoRHostSuccession::REJOIN_RETRY_SECONDS, true);
	}
}

void UWoRHostHandOverSubsystem::TakeOver(UWorld *World)
{
	UGameInstance *GI = GetGameInstance();
	USaveSubsystem *Save = GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
	UWoRSessionSubsystem *Sessions = GI ? GI->GetSubsystem<UWoRSessionSubsystem>() : nullptr;
	if (!Save || !Sessions || !Save->AdoptCheckpoint())
	{
		Step = EHostHandOverStep::GaveUp;
		UE_LOG(LogWoR, Warning, TEXT("[HandOver] Can't host from the checkpoint copy (no copy kept?); the run stays where the last save left it"));
		return;
	}
	Sessions->LeaveSession(); // the old host's session is gone; drop our side of it before hosting
	Step = Sessions->HostSession() ? EHostHandOverStep::Hosting : EHostHandOverStep::GaveUp;
	UE_LOG(LogWoR, Display, TEXT("[HandOver] %s"), Step == EHostHandOverStep::Hosting
		   ? TEXT("Hosting from the checkpoint copy; the run resumes at its last checkpoint")
		   : TEXT("Could not host; try wor.Session.Host"));
}

void UWoRHostHandOverSubsystem::TryRejoin()
{
	UGameInstance *GI = GetGameInstance();
	UWoRSessionSubsystem *Sessions = GI ? GI->GetSubsystem<UWoRSessionSubsystem>() : nullptr;
	UWorld *World = GI ? GI->GetWorld() : nullptr;
	if (!Sessions || Step != EHostHandOverStep::Rejoining || ++RejoinTries > WoRHostSuccession::REJOIN_MAX_TRIES)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(RetryTimer);
		}
		if (Step == EHostHandOverStep::Rejoining)
		{
			Step = EHostHandOverStep::GaveUp;
			UE_LOG(LogWoR, Warning, TEXT("[HandOver] %s never hosted; staying here (wor.Session.Find to look again)"), *NextHost);
		}
		return;
	}
	if (!SessionsFoundHandle.IsValid())
	{
		SessionsFoundHandle = Sessions->OnSessionsFound.AddUObject(this, &UWoRHostHandOverSubsystem::HandleSessionsFound);
	}
	if (RejoinTries == 1)
	{
		Sessions->LeaveSession(); // the joined session is gone with its host
	}
	Sessions->FindSessions();
}

void UWoRHostHandOverSubsystem::HandleSessionsFound()
{
	UGameInstance *GI = GetGameInstance();
	UWoRSessionSubsystem *Sessions = GI ? GI->GetSubsystem<UWoRSessionSubsystem>() : nullptr;
	if (!Sessions || Step != EHostHandOverStep::Rejoining)
	{
		return;
	}
	const int32 Index = Sessions->FindSessionIndexByOwner(NextHost);
	if (Index == INDEX_NONE || !Sessions->JoinFoundSession(Index))
	{
		return; // the next retry searches again
	}
	if (UWorld *World = GI->GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryTimer);
	}
	Sessions->OnSessionsFound.Remove(SessionsFoundHandle);
	SessionsFoundHandle.Reset();
	Step = EHostHandOverStep::None;
	UE_LOG(LogWoR, Display, TEXT("[HandOver] Rejoining %s's session"), *NextHost);
}

FString UWoRHostHandOverSubsystem::GetSuccessionString() const
{
	FString Out = FString::Printf(TEXT("[HandOver] me %s, step %d, next host '%s', old host '%s'; join order (%d):"), *GetLocalPlayerId(),
								  static_cast<int32>(Step), *NextHost, *OldHost, KnownOrder.Num());
	for (int32 Index = 0; Index < KnownOrder.Num(); ++Index)
	{
		Out += FString::Printf(TEXT("\n  %d. %s%s"), Index + 1, *KnownOrder[Index], Index == 0 ? TEXT(" (host)") : TEXT(""));
	}
	return Out;
}

static FAutoConsoleCommandWithWorld GCoopPrintSuccessionCommand(
	TEXT("wor.Coop.PrintSuccession"),
	TEXT("B-234b: the join order this machine knows, who hosts next if the host drops, and the hand-over's step."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		const UWoRHostHandOverSubsystem *HandOver = GI ? GI->GetSubsystem<UWoRHostHandOverSubsystem>() : nullptr;
		UE_LOG(LogWoR, Display, TEXT("%s"), HandOver ? *HandOver->GetSuccessionString() : TEXT("[HandOver] no subsystem"));
	}));

static FAutoConsoleCommandWithWorld GCoopSimulateHostDropCommand(
	TEXT("wor.Coop.SimulateHostDrop"),
	TEXT("B-234b DEBUG (client): act as if the host dropped: disconnect, then hand over (the next player hosts from its copy)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		UWoRHostHandOverSubsystem *HandOver = GI ? GI->GetSubsystem<UWoRHostHandOverSubsystem>() : nullptr;
		if (!HandOver || World->GetNetMode() != NM_Client || !HandOver->BeginHandOver())
		{
			UE_LOG(LogWoR, Warning, TEXT("[HandOver] SimulateHostDrop: only on a client that knows another player"));
			return;
		}
		// Leave like a lost connection does: back to the default map, where the hand-over carries on.
		GEngine->Exec(World, TEXT("disconnect"));
	}));
