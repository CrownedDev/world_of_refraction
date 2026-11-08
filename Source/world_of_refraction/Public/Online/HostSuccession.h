// HostSuccession.h
// B-234b part 1 (D-103, Q-234b): when the host drops, the next player in join order hosts from their checkpoint copy
// (B-234) and the others find and rejoin that player's session. The host replicates the join order
// (AWoRGameState::HostOrder); every client keeps the last order it saw here, so it still knows it once the host is
// gone. On a lost connection the game travels to the default map; when that world is up, everyone leaves the dead
// session; the successor then hosts (UWoRSessionSubsystem::HostSession) and, once hosting works, adopts its copy
// (USaveSubsystem::AdoptCheckpoint; the hub's run resume reopens the trial at the last checkpoint, so the fight is
// exited); the others search for its session and join it. A planned hand-over at a rest spot
// (B-234c) runs the same path: the host saves, tells everyone, leaves and rejoins the next host as a client. The
// old host's claimed seats are AI until it rejoins (the copy's party has no claims).
// Debug: wor.Coop.PrintSuccession (GetSuccessionString), wor.Coop.SimulateHostDrop.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/TimerHandle.h"
#include "HostSuccession.generated.h"

class UNetDriver;
class UWorld;

namespace WoRHostSuccession
{
	/** Q-234b (3): how often a client looks for the new host's session, and how many times before it gives up. */
	constexpr float REJOIN_RETRY_SECONDS = 3.0f;
	constexpr int32 REJOIN_MAX_TRIES = 10;

	/** Q-234b (2): a short wait between leaving the old session and hosting the new one (a destroy can finish late). */
	constexpr float TAKE_OVER_DELAY_SECONDS = 1.0f;

	/** B-234c: how long the host waits, after saving and telling everyone, before it leaves (its copy's chunks and the
	 *  hand-over call are reliable RPCs that must arrive first). */
	constexpr float PLANNED_LEAVE_DELAY_SECONDS = 2.0f;

	/** B-234c: a planned hand-over needs a listen server with at least one other player. */
	WORLD_OF_REFRACTION_API bool CanHandOverFrom(ENetMode NetMode, int32 PlayersKnown);

	/** Q-234b (1): the player who hosts after OldHost: the first in Order who isn't OldHost. Empty when nobody is left. */
	WORLD_OF_REFRACTION_API FString PickNextHost(const TArray<FString> &Order, const FString &OldHost);

	/** Whether a lost connection should start a hand-over: a client losing its host, not a refused join or a
	 *  pending-connection failure. */
	WORLD_OF_REFRACTION_API bool IsHostLoss(ENetworkFailure::Type FailureType);
}

/** What this machine does after the host dropped. */
UENUM()
enum class EHostHandOverStep : uint8
{
	None,
	/** Waiting for the default map to load before hosting from the copy. */
	TakeOverPending,
	/** HostSession started; the copy is adopted when it succeeds (then the step is None again). */
	Hosting,
	/** Searching for the new host's session, or joining it (None again once travelling). */
	Rejoining,
	/** No copy, nobody left, or the new host never appeared. */
	GaveUp
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRHostHandOverSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	/** AWoRGameState::OnRep_HostOrder (and the host itself): the latest join order, host first. */
	void NoteHostOrder(const TArray<FString> &Order);

	const TArray<FString> &GetKnownOrder() const { return KnownOrder; }
	EHostHandOverStep GetStep() const { return Step; }
	const FString &GetNextHost() const { return NextHost; }

	/** This machine's own player id, as the host's order spells it (the local player's unique net id). */
	FString GetLocalPlayerId() const;

	/** Starts the hand-over as if the host had just dropped (also the network failure's path). False when there is no
	 *  known order, this machine was the host, or nobody is left. */
	bool BeginHandOver();

	/** B-234c (host, at a rest spot): hand the run over on purpose. Saves (every player gets the copy), tells every
	 *  client to hand over (AWoRPlayerState::Client_BeginPlannedHandOver, after its copy's chunks), then leaves after
	 *  PLANNED_LEAVE_DELAY_SECONDS and rejoins the next host as a client. False when CanHandOverFrom says no or the
	 *  save fails. */
	bool BeginPlannedHandOver(UWorld *World);

	/** B-234c (client): the host is handing over: start the hand-over now and leave, as on a drop. */
	void HandlePlannedHandOver(UWorld *World);

	/** Debug: the known order, this player's id, the next host and the step. */
	FString GetSuccessionString() const;

private:
	void HandleNetworkFailure(UWorld *World, UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString &Error);
	void HandleWorldActorsInitialized(const struct FActorsInitializedParams &Params);
	void ContinueHandOver(UWorld *World);
	/** After the old session is gone (TAKE_OVER_DELAY_SECONDS): host; the copy is adopted only once hosting works. */
	void StartHosting();
	void HandleSessionCreated(bool bSucceeded);
	void TryRejoin();
	void HandleSessionsFound();
	void HandleSessionJoined(bool bSucceeded);
	void StartRejoinTimer();

	TArray<FString> KnownOrder;
	FString OldHost;
	FString NextHost;
	EHostHandOverStep Step = EHostHandOverStep::None;
	int32 RejoinTries = 0;

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle WorldInitHandle;
	/** B-234c review: a planned hand-over waits to leave; a level change during the wait drops it. */
	FDelegateHandle WorldCleanupHandle;
	bool bPlannedLeavePending = false;
	FDelegateHandle SessionsFoundHandle;
	FDelegateHandle SessionCreatedHandle;
	FDelegateHandle SessionJoinedHandle;
	FTimerHandle RetryTimer;
};
