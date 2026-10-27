// BattleGameMode.h
// GameMode for battle stages (Cluster T-C1): the far side of the encounter
// level swap. On BeginPlay (deferred one tick so player login settles) it
// consumes the roster UTrialRunSubsystem stashed before the OpenLevel, spawns
// a fresh pawn per UCharacterData (deferred-spawn so the component's BeginPlay
// init cascade — inventory / loadout / world stats / HP-EP seed — reads the
// RIGHT asset), possesses the first LocalParty pawn with PC0, spawns the
// orchestrator, and starts combat. On OnCombatResultReady it routes back to
// the trial via UTrialRunSubsystem::ExitEncounter — after a results screen
// (UCombatResultsWidget) is acknowledged or times out.
//
// Victory and Defeat currently produce the SAME trial-side outcome (the trial
// level reloads whole from the map asset — defeated enemies included). Banked
// for the persistence keystone arc.

#pragma once

#include "Combat/CombatOrchestrator.h"
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BattleGameMode.generated.h"

class UCharacterData;
class UCombatResultsWidget;

UCLASS()
class WORLD_OF_REFRACTION_API ABattleGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    /** Picks AWoRPlayerState: the player's wallet (D-046, B-151). */
    ABattleGameMode();

    /** Pawn class spawned per LocalParty (player-side) CharacterData. */
    UPROPERTY(EditDefaultsOnly, Category = "Battle")
    TSoftClassPtr<APawn> DefaultPlayerPawnClass;

    /** Pawn class spawned per OpposingParty (enemy-side) CharacterData. */
    UPROPERTY(EditDefaultsOnly, Category = "Battle")
    TSoftClassPtr<APawn> DefaultEnemyPawnClass;

    /** Orchestrator class (BP_CombatOrchestrator — the BP subclass carries the
     *  OnCombatStartedUI HUD hook; raw C++ = no HUD). */
    UPROPERTY(EditDefaultsOnly, Category = "Battle")
    TSoftClassPtr<ACombatOrchestrator> OrchestratorClass;

    /** Results screen shown before returning to the trial. Null = native UCombatResultsWidget. */
    UPROPERTY(EditDefaultsOnly, Category = "Battle")
    TSoftClassPtr<UCombatResultsWidget> ResultsWidgetClass;

    /** D-016: a Generic or Resonator needs gear (a primary or secondary item) to
     *  fight; a Refractor fights on its innate element. A gearless party member is
     *  left out of the fight (B-071). */
    static bool CanEnterCombat(const AActor *Combatant);

protected:
    virtual void BeginPlay() override;

private:
    /** Next-tick combat bootstrap: consume stash → spawn → possess → StartCombat. */
    void BootstrapCombat();

    /** Deferred-spawn a pawn and assign Data BEFORE FinishSpawning, so
     *  UCharacterDataComponent::BeginPlay initializes from the right asset. */
    APawn *SpawnCombatant(UClass *PawnClass, UCharacterData *Data, const FTransform &SpawnTransform);

    /** Abort helper: log Reason and send the player back through ExitEncounter. */
    void AbortToTrial(const FString &Reason);

    /** The party leader's controller in this level, rebound on the party so later reads are
     *  fresh (B-132). Only a battle map opened without a party falls back to the first local
     *  player, with a warning (a dev path). */
    APlayerController *ResolvePartyLeader() const;

    UFUNCTION()
    void HandleCombatEnd(const FCombatResult &Result);

    /** Results screen acknowledged (or timed out) -> ReturnToTrial. */
    UFUNCTION()
    void HandleResultsContinue();

    /** Show the results screen; false if it couldn't be created (caller returns immediately). */
    bool ShowResults(const FCombatResult &Result);

    void ReturnToTrial();

    UPROPERTY(Transient)
    TObjectPtr<UCombatResultsWidget> ResultsWidget;

    /** Orchestrator spawned by BootstrapCombat; source of OnCombatResultReady. */
    TWeakObjectPtr<ACombatOrchestrator> Orchestrator;
};
