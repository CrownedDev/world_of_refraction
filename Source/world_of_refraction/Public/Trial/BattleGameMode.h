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

#include "Combat/Actions/ActionStructs.h"
#include "Combat/CombatOrchestrator.h"
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Memory/MemoryConditions.h"
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
     *  fresh (B-132). When the leader can't be found (no party: a battle map opened directly)
     *  it falls back to the first local player, with a warning (a dev path). */
    APlayerController *ResolvePartyLeader();

    UFUNCTION()
    void HandleCombatEnd(const FCombatResult &Result);

    /** Results screen acknowledged (or timed out) -> ReturnToTrial. */
    UFUNCTION()
    void HandleResultsContinue();

    /** Show the results screen; false if it couldn't be created (caller returns immediately). */
    bool ShowResults(const FCombatResult &Result);

    void ReturnToTrial();

    /** B-209 (D-100 part 2): a fight won is an event that checks memory unlock conditions; a
     *  beaten boss's MemoryOnDefeat is the memory it names. */
    void CheckFightWonMemories();

    /** No player-side combatant is below the HP it started the fight with. TODO: a heal hides a
     *  hit; count damage taken once a per-fight damage record exists. */
    bool WasPartyUnhurt() const;

    /** Per fight (D-064): each player-side combatant's HP when the fight started. */
    TMap<TWeakObjectPtr<const AActor>, int32> PartyStartHP;

    /** B-209 (Q-209d): a party member's skill and Breakthrough use count toward memory conditions. */
    UFUNCTION()
    void HandleActionStarted(AActor *Executor, const FAction &Action, int32 EnergyCost);

    /** B-209 (Q-209d): a party member's kill counts the weapon it held as a finishing blow. */
    UFUNCTION()
    void HandleTargetKilled(AActor *Killer, AActor *Victim);

    /** Bind / unbind the two handlers above on the action executor. */
    void WatchFightUsage(bool bWatch);

    /** Per fight (D-064): what each player-side combatant used, read when the fight is won. */
    TMap<TWeakObjectPtr<const AActor>, FMemoryFightUsage> FightUsage;

    UPROPERTY(Transient)
    TObjectPtr<UCombatResultsWidget> ResultsWidget;

    /** Orchestrator spawned by BootstrapCombat; source of OnCombatResultReady. */
    TWeakObjectPtr<ACombatOrchestrator> Orchestrator;
};
