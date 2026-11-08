// WoRGameState.h
// B-227 (Design/CoopStateOwnership.md): the replicated state every player of a level shares. Set as
// GameStateClass by the C++ game modes (a Blueprint game mode must pick it too). B-228: the turn state
// (UTurnStateComponent); B-232 the run state. B-234b: the players' join order, host first (HostOrder), so a client
// still knows who hosts next once the host drops (UWoRHostHandOverSubsystem).
// Debug: GetDebugString, `wor.GameState.Print`.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "WoRGameState.generated.h"

class UTurnStateComponent;
class URunStateComponent;

UCLASS()
class WORLD_OF_REFRACTION_API AWoRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AWoRGameState();

	/** B-228: the fight's replicated turn state (current actor, turn order, teams). */
	UTurnStateComponent *GetTurnState() const { return TurnState; }

	/** B-232: the run every player shares and the side's roster with its seats (Q-21-audit). */
	URunStateComponent *GetRunState() const { return RunState; }

	/** The players this level holds (PlayerArray), one per line with its net role, then the turn state. */
	FString GetDebugString() const;

	/** B-234b: every player's unique net id in the order they joined, the host first. */
	const TArray<FString> &GetHostOrder() const { return HostOrder; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;
	virtual void AddPlayerState(APlayerState *PlayerState) override;
	virtual void RemovePlayerState(APlayerState *PlayerState) override;

	/** B-234b (server): HostOrder gains PlayerState's id (once) or loses it; the host's hand-over subsystem hears it.
	 *  AddPlayerState comes before a joining player's id is set, so AWoRPlayerState::OnSetUniqueId calls this too. */
	void UpdateHostOrder(const APlayerState *PlayerState, bool bJoined);

	/** B-234c (server → clients): the host is handing the run over at a rest spot; each client starts its hand-over
	 *  (UWoRHostHandOverSubsystem::HandlePlannedHandOver) and leaves. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BeginPlannedHandOver();

private:
	UFUNCTION()
	void OnRep_HostOrder();

	UPROPERTY(ReplicatedUsing = OnRep_HostOrder)
	TArray<FString> HostOrder;

	UPROPERTY(VisibleAnywhere, Category = "Turn State")
	TObjectPtr<UTurnStateComponent> TurnState;

	UPROPERTY(VisibleAnywhere, Category = "Run State")
	TObjectPtr<URunStateComponent> RunState;
};
