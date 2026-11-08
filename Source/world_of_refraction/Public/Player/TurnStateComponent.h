// TurnStateComponent.h
// B-228 (Design/CoopStateOwnership.md): the fight's turn state on the GameState, so every player's HUD and
// command menu read the same thing. The server copies it from UTurnManager (a per-instance subsystem that
// clients don't share) on every turn start, speed change and combat end (a new round is followed by a turn start); clients get it by replication and broadcast
// OnTurnStateChanged. With no GameState (headless specs), readers fall back to UTurnManager (FindTurnState).
// B-228b: a client builds its combat HUD the first time the replicated state shows combat on (UpdateClientCombatHUD).
// Debug: GetDebugString, `wor.Turn.PrintState`.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TurnStateComponent.generated.h"

class UTurnManager;

/** One slot of the replicated turn order. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FTurnStateSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Turn State")
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Turn State")
	bool bIsBonusTurn = false;
};

/** One combatant's team, so a client can tint allies and enemies. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FTurnStateTeam
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Turn State")
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Turn State")
	int32 Team = INDEX_NONE;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTurnStateChanged);

/** B-228b: what a machine does with its combat HUD when the replicated turn state changes. */
enum class EClientHUDStep : uint8
{
	None,
	Build,
	Teardown,
};

namespace ClientCombatHUD
{
	/** Only a client acts (the server builds its own HUD in OnCombatStartedUI): build once when combat turns on,
	 *  tear down once when it turns off. */
	inline EClientHUDStep Next(bool bIsClient, bool bCombatActive, bool bBuilt)
	{
		if (!bIsClient)
		{
			return EClientHUDStep::None;
		}
		if (bCombatActive && !bBuilt)
		{
			return EClientHUDStep::Build;
		}
		return !bCombatActive && bBuilt ? EClientHUDStep::Teardown : EClientHUDStep::None;
	}
}

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UTurnStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTurnStateComponent();

	/** The GameState's turn state in WorldContext's world, or null (no GameState, or not an AWoRGameState). */
	static UTurnStateComponent *Find(const UObject *WorldContext);

	/** Server: listen to UTurnManager and copy its state now. Called from BeginPlay on authority. */
	void BindToTurnManager();
	void UnbindFromTurnManager();

	/** Server: copy the current actor, turn number, upcoming slots and teams from UTurnManager. */
	void RefreshFromTurnManager();

	UFUNCTION(BlueprintPure, Category = "Turn State")
	bool IsCombatActive() const { return bCombatActive; }
	UFUNCTION(BlueprintPure, Category = "Turn State")
	AActor *GetCurrentActor() const { return CurrentActor; }
	UFUNCTION(BlueprintPure, Category = "Turn State")
	int32 GetTurnNumber() const { return TurnNumber; }
	UFUNCTION(BlueprintPure, Category = "Turn State")
	bool GetCurrentTurnIsBonus() const { return bCurrentTurnIsBonus; }
	/** The next turns after the current one (up to TURN_STATE_PREVIEW_COUNT). */
	UFUNCTION(BlueprintPure, Category = "Turn State")
	const TArray<FTurnStateSlot> &GetUpcoming() const { return Upcoming; }
	/** Actor's team (0 = the local party side, as UTurnManager::GetActorTeam), INDEX_NONE if not in the fight. */
	UFUNCTION(BlueprintPure, Category = "Turn State")
	int32 GetActorTeam(const AActor *Actor) const;
	/** Every combatant on Team, in the server's order (as UTurnManager::GetTeamMembers). */
	TArray<AActor *> GetTeamMembers(int32 Team) const;

	/** Fired on the server after a refresh and on clients when replicated state arrives. */
	UPROPERTY(BlueprintAssignable, Category = "Turn State")
	FOnTurnStateChanged OnTurnStateChanged;

	FString GetDebugString() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleTurnStarted(AActor *Actor, int32 InTurnNumber);
	UFUNCTION()
	void HandleSpeedChanged(AActor *Actor);
	UFUNCTION()
	void HandleCombatEnded(int32 FinalTurnCount);

	UFUNCTION()
	void OnRep_TurnState();

	/** B-228b: a client builds (or tears down) its combat HUD from the replicated state (ClientCombatHUD::Next). */
	void UpdateClientCombatHUD();

	/** B-228b: this client built its combat HUD for the fight now running. */
	bool bClientHUDBuilt = false;

	/** B-232b: on a client, opens the command menu when the replicated current turn is a member this player commands,
	 *  once per turn, and closes it when the turn moves on (the host opens its own in UCombatCommandMenuSubsystem). */
	void UpdateClientCommandMenu();

	/** B-232b: the turn the client last opened its menu for (TurnNumber), INDEX_NONE for none. */
	int32 ClientMenuTurn = INDEX_NONE;

	/** B-228b: a client's own combat HUD (the orchestrator, which owns the server's, is server-only). */
	UPROPERTY(Transient)
	TObjectPtr<class UCombatHUD> ClientHUD;

	UTurnManager *GetTurnManager() const;

	// One OnRep function for the whole state. Each replicated field that changed calls it, so a client may redraw up to
	// six times per turn; folding the fields into one snapshot struct (one OnRep) is still to do. The readers redraw from all of it.
	UPROPERTY(ReplicatedUsing = OnRep_TurnState)
	bool bCombatActive = false;
	UPROPERTY(ReplicatedUsing = OnRep_TurnState)
	TObjectPtr<AActor> CurrentActor = nullptr;
	UPROPERTY(ReplicatedUsing = OnRep_TurnState)
	int32 TurnNumber = 0;
	UPROPERTY(ReplicatedUsing = OnRep_TurnState)
	bool bCurrentTurnIsBonus = false;
	UPROPERTY(ReplicatedUsing = OnRep_TurnState)
	TArray<FTurnStateSlot> Upcoming;
	UPROPERTY(ReplicatedUsing = OnRep_TurnState)
	TArray<FTurnStateTeam> Teams;

	/** Bound this play session (not cached across PIE sessions). */
	TWeakObjectPtr<UTurnManager> BoundTurnManager;
};
