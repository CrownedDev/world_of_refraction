// BattleGameMode.h
// GameMode for battle stages (Cluster T-C1): the far side of the encounter
// level swap. On BeginPlay (deferred one tick so player login settles) it
// consumes the roster UTrialRunSubsystem stashed before the OpenLevel, spawns
// a fresh pawn per UCharacterData (deferred-spawn so the component's BeginPlay
// init cascade — inventory / loadout / world stats / HP-EP seed — reads the
// RIGHT asset), possesses the first Team0 pawn with PC0, spawns the
// orchestrator, and starts combat. On OnCombatResultReady it routes back to
// the trial via UTrialRunSubsystem::ExitEncounter.
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

UCLASS()
class WORLD_OF_REFRACTION_API ABattleGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    /** Pawn class spawned per Team0 (player-side) CharacterData. */
    UPROPERTY(EditDefaultsOnly, Category = "Battle")
    TSoftClassPtr<APawn> DefaultPlayerPawnClass;

    /** Pawn class spawned per Team1 (enemy-side) CharacterData. */
    UPROPERTY(EditDefaultsOnly, Category = "Battle")
    TSoftClassPtr<APawn> DefaultEnemyPawnClass;

    /** Orchestrator class (BP_CombatOrchestrator — the BP subclass carries the
     *  OnCombatStartedUI HUD hook; raw C++ = no HUD). */
    UPROPERTY(EditDefaultsOnly, Category = "Battle")
    TSoftClassPtr<ACombatOrchestrator> OrchestratorClass;

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

    UFUNCTION()
    void HandleCombatEnd(const FCombatResult &Result);

    /** Orchestrator spawned by BootstrapCombat; source of OnCombatResultReady. */
    TWeakObjectPtr<ACombatOrchestrator> Orchestrator;
};
