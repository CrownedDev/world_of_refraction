// WoRGameState.h
// B-227 (Design/CoopStateOwnership.md): the replicated state every player of a level shares. Set as
// GameStateClass by the C++ game modes (a Blueprint game mode must pick it too). B-228: the turn state
// (UTurnStateComponent); B-232 the run state. B-234b: the players' join order, host first (HostOrder), so a client
// still knows who hosts next once the host drops (UWoRHostHandOverSubsystem).
// Debug: GetDebugString, `wor.GameState.Print`.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Trial/ReviveVote.h"
#include "WoRGameState.generated.h"

class UTurnStateComponent;
class URunStateComponent;
class AWoRPlayerState;

/** B-187e (Q-187e changed): the revive vote opened, changed or closed (server and clients; rest menus refresh on it). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReviveVoteChanged);

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

	/** B-187e (Q-187e changed by Crown, 2026-11-08), server: Starter asks to revive the last rest spot's mini-boss. Every
	 *  player outside battle votes (Starter's start is its yes); unanimous yes revives (WoRRestSpot::ReviveMiniBoss).
	 *  Solo it revives at once. Refused (Failed) for a starter in battle, with no revivable mini-boss, or a vote open. */
	EReviveVoteResult StartReviveVote(AWoRPlayerState *Starter);

	/** Server: Voter's answer (AWoRPlayerState::ServerCastReviveVote, or the host's own menu). */
	void CastReviveVote(const AWoRPlayerState *Voter, bool bYes);

	/** Server: a player is in battle when the active fight has a combatant it commands (ACombatPlayerController::ControlsActor). */
	bool IsPlayerInBattle(const APlayerState *Player) const;

	const FReviveVote &GetReviveVote() const { return ReviveVoteState; }

	UPROPERTY(BlueprintAssignable, Category = "Revive Vote")
	FOnReviveVoteChanged OnReviveVoteChanged;

	/** Debug: the vote's state. Console: wor.Trial.ReviveVote. */
	FString GetReviveVoteString() const;

private:
	UFUNCTION()
	void OnRep_ReviveVote();

	/** Server, once a second while a vote is open: voters who entered a fight leave the list; then the timeout. */
	void TickReviveVote();

	/** Server: acts on a result (Passed revives; Failed tells everyone) and replicates the state. */
	void ApplyReviveVoteResult(EReviveVoteResult Result);

	UPROPERTY(ReplicatedUsing = OnRep_ReviveVote)
	FReviveVote ReviveVoteState;

	FTimerHandle ReviveVoteTimer;

	UFUNCTION()
	void OnRep_HostOrder();

	UPROPERTY(ReplicatedUsing = OnRep_HostOrder)
	TArray<FString> HostOrder;

	UPROPERTY(VisibleAnywhere, Category = "Turn State")
	TObjectPtr<UTurnStateComponent> TurnState;

	UPROPERTY(VisibleAnywhere, Category = "Run State")
	TObjectPtr<URunStateComponent> RunState;
};
