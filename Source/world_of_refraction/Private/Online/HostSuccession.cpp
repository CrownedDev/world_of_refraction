// HostSuccession.cpp

#include "Online/HostSuccession.h"
#include "Online/WoRSessionSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "GameFramework/PlayerController.h"
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

bool WoRHostSuccession::CanHandOverFrom(ENetMode NetMode, int32 PlayersKnown)
{
	return NetMode == NM_ListenServer && PlayersKnown >= 2;
}

int32 WoRHostSuccession::NextJoinSequence(const TArray<int32> &Held)
{
	int32 Next = 0;
	for (const int32 Place : Held)
	{
		if (Place != NO_JOIN_SEQUENCE)
		{
			Next = FMath::Max(Next, Place + 1);
		}
	}
	return Next;
}

TArray<FString> WoRHostSuccession::OrderByJoinSequence(TArray<TPair<FString, int32>> Players)
{
	const auto SortKey = [](int32 Place) { return Place == NO_JOIN_SEQUENCE ? MAX_int32 : Place; };
	Players.StableSort([&SortKey](const TPair<FString, int32> &A, const TPair<FString, int32> &B)
	{
		return SortKey(A.Value) < SortKey(B.Value);
	});
	TArray<FString> Order;
	for (const TPair<FString, int32> &Player : Players)
	{
		if (!Player.Key.IsEmpty())
		{
			Order.AddUnique(Player.Key);
		}
	}
	return Order;
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
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	if (UWorld *World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(RetryTimer);
	}
	if (UWoRSessionSubsystem *Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWoRSessionSubsystem>() : nullptr)
	{
		Sessions->OnSessionsFound.Remove(SessionsFoundHandle);
		Sessions->OnSessionCreated.Remove(SessionCreatedHandle);
		Sessions->OnSessionJoined.Remove(SessionJoinedHandle);
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
		bPlanned = false;
		SetStep(EHostHandOverStep::GaveUp);
		return false;
	}
	const bool bTakeOver = NextHost == GetLocalPlayerId();
	RejoinTries = 0;
	bPlanned = false; // HandlePlannedHandOver sets it again after this
	SetStep(bTakeOver ? EHostHandOverStep::TakeOverPending : EHostHandOverStep::Rejoining);
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
	if (!World || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	if (World->GetNetMode() == NM_Client)
	{
		if (Step == EHostHandOverStep::Rejoining)
		{
			FinishRejoin(); // the new host's world is up
		}
		return; // else still connected somewhere
	}
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
	WorldInitHandle.Reset();
	ContinueHandOver(World);
}

void UWoRHostHandOverSubsystem::ContinueHandOver(UWorld *World)
{
	// The old host's session is gone; drop our side of it first (review: HostSession refuses while one exists, and on
	// EOS a destroy finishes late, so hosting waits TAKE_OVER_DELAY_SECONDS).
	if (UWoRSessionSubsystem *Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWoRSessionSubsystem>() : nullptr)
	{
		Sessions->LeaveSession();
	}
	// Not inside world initialisation: the session calls and the level change wait for a timer.
	if (Step == EHostHandOverStep::TakeOverPending)
	{
		World->GetTimerManager().SetTimer(RetryTimer, FTimerDelegate::CreateUObject(this, &UWoRHostHandOverSubsystem::StartHosting),
										  WoRHostSuccession::TAKE_OVER_DELAY_SECONDS, false);
	}
	else if (Step == EHostHandOverStep::Rejoining)
	{
		StartRejoinTimer();
	}
}

void UWoRHostHandOverSubsystem::StartHosting()
{
	UGameInstance *GI = GetGameInstance();
	UWoRSessionSubsystem *Sessions = GI ? GI->GetSubsystem<UWoRSessionSubsystem>() : nullptr;
	if (!Sessions)
	{
		SetStep(EHostHandOverStep::GaveUp);
		return;
	}
	Sessions->OnSessionCreated.Remove(SessionCreatedHandle);
	SessionCreatedHandle = Sessions->OnSessionCreated.AddUObject(this, &UWoRHostHandOverSubsystem::HandleSessionCreated);
	SetStep(EHostHandOverStep::Hosting);
	if (!Sessions->HostSession())
	{
		Sessions->OnSessionCreated.Remove(SessionCreatedHandle);
		SessionCreatedHandle.Reset();
		SetStep(EHostHandOverStep::GaveUp);
		UE_LOG(LogWoR, Warning, TEXT("[HandOver] Could not host; try wor.Session.Host"));
	}
}

void UWoRHostHandOverSubsystem::HandleSessionCreated(bool bSucceeded)
{
	UGameInstance *GI = GetGameInstance();
	if (UWoRSessionSubsystem *Sessions = GI ? GI->GetSubsystem<UWoRSessionSubsystem>() : nullptr)
	{
		Sessions->OnSessionCreated.Remove(SessionCreatedHandle);
	}
	SessionCreatedHandle.Reset();
	if (!bSucceeded)
	{
		SetStep(EHostHandOverStep::GaveUp);
		UE_LOG(LogWoR, Warning, TEXT("[HandOver] Hosting failed; nothing adopted (try wor.Session.Host)"));
		return;
	}
	// Review: adopt only now that hosting works, so a failed host never leaves this player in the old host's game.
	USaveSubsystem *Save = GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
	// B-428 (a): the run's per-player records follow the hosts (the join order's ids are WoRDefeat::PlayerDropKey's form).
	const bool bAdopted = Save && Save->AdoptCheckpoint(OldHost, GetLocalPlayerId());
	SetStep(EHostHandOverStep::None); // done: a later lost host starts a new hand-over
	UE_LOG(LogWoR, Display, TEXT("[HandOver] %s"), bAdopted ? TEXT("Hosting from the checkpoint copy; the run resumes at its last checkpoint")
															: TEXT("Hosting, but no checkpoint copy with a run was kept: the run doesn't resume"));
}

void UWoRHostHandOverSubsystem::StartRejoinTimer()
{
	if (UWorld *World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		World->GetTimerManager().SetTimer(RetryTimer, FTimerDelegate::CreateUObject(this, &UWoRHostHandOverSubsystem::TryRejoin),
										  WoRHostSuccession::REJOIN_RETRY_SECONDS, true);
	}
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
			SetStep(EHostHandOverStep::GaveUp);
			UE_LOG(LogWoR, Warning, TEXT("[HandOver] %s never hosted; staying here (wor.Session.Find to look again)"), *NextHost);
		}
		return;
	}
	if (!SessionsFoundHandle.IsValid())
	{
		SessionsFoundHandle = Sessions->OnSessionsFound.AddUObject(this, &UWoRHostHandOverSubsystem::HandleSessionsFound);
	}
	OnStepChanged.Broadcast(Step); // B-459a: the step stays, but the card shows the search going on
	Sessions->FindSessions();
}

void UWoRHostHandOverSubsystem::SetStep(EHostHandOverStep NewStep)
{
	if (Step != NewStep)
	{
		Step = NewStep;
		OnStepChanged.Broadcast(Step);
	}
}

void UWoRHostHandOverSubsystem::StopWaiting()
{
	if ((Step != EHostHandOverStep::Rejoining && Step != EHostHandOverStep::GaveUp) || bPlannedLeavePending)
	{
		return; // this machine is taking over, or still has to leave: the others wait on it
	}
	if (UWorld *World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(RetryTimer);
	}
	if (UWoRSessionSubsystem *Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWoRSessionSubsystem>() : nullptr)
	{
		Sessions->OnSessionsFound.Remove(SessionsFoundHandle);
		Sessions->OnSessionJoined.Remove(SessionJoinedHandle); // a join already under way still travels; it isn't tracked
	}
	SessionsFoundHandle.Reset();
	SessionJoinedHandle.Reset();
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
	WorldInitHandle.Reset();
	UE_LOG(LogWoR, Display, TEXT("[HandOver] Stopped waiting for %s (wor.Session.Find to look again)"), *NextHost);
	SetStep(EHostHandOverStep::None);
}

void UWoRHostHandOverSubsystem::HandleSessionsFound()
{
	UGameInstance *GI = GetGameInstance();
	UWoRSessionSubsystem *Sessions = GI ? GI->GetSubsystem<UWoRSessionSubsystem>() : nullptr;
	if (!Sessions || Step != EHostHandOverStep::Rejoining || SessionJoinedHandle.IsValid())
	{
		return;
	}
	const int32 Index = Sessions->FindSessionIndexByOwner(NextHost);
	if (Index == INDEX_NONE)
	{
		return; // the next retry searches again
	}
	SessionJoinedHandle = Sessions->OnSessionJoined.AddUObject(this, &UWoRHostHandOverSubsystem::HandleSessionJoined);
	if (!Sessions->JoinFoundSession(Index))
	{
		Sessions->OnSessionJoined.Remove(SessionJoinedHandle);
		SessionJoinedHandle.Reset();
		return;
	}
	// Review: no more searches while the join runs; a failed join starts them again.
	if (UWorld *World = GI->GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryTimer);
	}
	UE_LOG(LogWoR, Display, TEXT("[HandOver] Joining %s's session"), *NextHost);
}

void UWoRHostHandOverSubsystem::HandleSessionJoined(bool bSucceeded)
{
	UGameInstance *GI = GetGameInstance();
	UWoRSessionSubsystem *Sessions = GI ? GI->GetSubsystem<UWoRSessionSubsystem>() : nullptr;
	if (Sessions)
	{
		Sessions->OnSessionJoined.Remove(SessionJoinedHandle);
	}
	SessionJoinedHandle.Reset();
	if (bSucceeded)
	{
		// Desktop review: still Rejoining until the new host's world is up. A travel that fails (the new host not
		// listening yet) lands back on the default map, which leaves the session and searches again (tries still count).
		FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
		WorldInitHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UWoRHostHandOverSubsystem::HandleWorldActorsInitialized);
		UE_LOG(LogWoR, Display, TEXT("[HandOver] Joined %s's session; travelling"), *NextHost);
		return;
	}
	UE_LOG(LogWoR, Warning, TEXT("[HandOver] Joining %s failed; searching again"), *NextHost);
	StartRejoinTimer();
}

bool UWoRHostHandOverSubsystem::BeginPlannedHandOver(UWorld *World)
{
	if (!World || !WoRHostSuccession::CanHandOverFrom(World->GetNetMode(), KnownOrder.Num()) ||
		(Step != EHostHandOverStep::None && Step != EHostHandOverStep::GaveUp))
	{
		UE_LOG(LogWoR, Warning, TEXT("[HandOver] Planned hand-over: only a host with another player, and not mid-hand-over"));
		return false;
	}
	OldHost = GetLocalPlayerId();
	NextHost = WoRHostSuccession::PickNextHost(KnownOrder, OldHost);
	if (NextHost.IsEmpty())
	{
		return false;
	}
	// 1. Everyone gets this moment as their checkpoint copy (sync, so it is written before it is sent). Review: no
	//    copy sent, no hand-over.
	USaveSubsystem *Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
	if (!Save || !Save->SaveNow(World, /*bAsync=*/false))
	{
		UE_LOG(LogWoR, Warning, TEXT("[HandOver] Planned hand-over stopped: the save (and so the copies) failed"));
		return false;
	}
	// 2. Every client starts its hand-over and leaves (the next one hosts from that copy). Review: on each player's
	//    PlayerState, after its copy's chunks on the same actor, so the call can't overtake them.
	const AGameStateBase *GameState = World->GetGameState();
	for (APlayerState *Player : GameState ? GameState->PlayerArray : TArray<TObjectPtr<APlayerState>>())
	{
		AWoRPlayerState *WoRPlayer = Cast<AWoRPlayerState>(Player);
		const APlayerController *PC = WoRPlayer ? Cast<APlayerController>(WoRPlayer->GetOwner()) : nullptr;
		if (PC && !PC->IsLocalController())
		{
			WoRPlayer->Client_BeginPlannedHandOver();
		}
	}
	// 3. This machine leaves once those calls are out, then looks for the next host like any client.
	RejoinTries = 0;
	bPlanned = true;
	SetStep(EHostHandOverStep::Rejoining);
	UE_LOG(LogWoR, Display, TEXT("[HandOver] Planned: %s hosts next; leaving in %.1f s"), *NextHost, WoRHostSuccession::PLANNED_LEAVE_DELAY_SECONDS);
	// Review: if the level changes during the wait the timer goes with it; forget the hand-over then.
	bPlannedLeavePending = true;
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddWeakLambda(this, [this, World](UWorld *Cleaned, bool, bool)
	{
		if (Cleaned == World && bPlannedLeavePending)
		{
			bPlannedLeavePending = false;
			FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle); // desktop review: once only
			WorldCleanupHandle.Reset();
			SetStep(EHostHandOverStep::None);
			UE_LOG(LogWoR, Warning, TEXT("[HandOver] The level changed before leaving: planned hand-over dropped"));
		}
	});
	World->GetTimerManager().SetTimer(RetryTimer, FTimerDelegate::CreateWeakLambda(this, [this, World]()
	{
		bPlannedLeavePending = false;
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
		WorldInitHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UWoRHostHandOverSubsystem::HandleWorldActorsInitialized);
		GEngine->Exec(World, TEXT("disconnect"));
	}), WoRHostSuccession::PLANNED_LEAVE_DELAY_SECONDS, false);
	return true;
}

void UWoRHostHandOverSubsystem::HandlePlannedHandOver(UWorld *World)
{
	if (!World || World->GetNetMode() != NM_Client || !BeginHandOver())
	{
		return;
	}
	bPlanned = true;
	OnStepChanged.Broadcast(Step); // the card's title follows
	// Not inside the RPC: leave on the next tick, as a lost connection would.
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [World]() { GEngine->Exec(World, TEXT("disconnect")); }));
}

void UWoRHostHandOverSubsystem::FinishRejoin()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
	WorldInitHandle.Reset();
	if (UWoRSessionSubsystem *Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWoRSessionSubsystem>() : nullptr)
	{
		Sessions->OnSessionsFound.Remove(SessionsFoundHandle);
	}
	SessionsFoundHandle.Reset();
	SetStep(EHostHandOverStep::None); // done: a later lost host starts a new hand-over
	UE_LOG(LogWoR, Display, TEXT("[HandOver] Rejoined %s"), *NextHost);
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
