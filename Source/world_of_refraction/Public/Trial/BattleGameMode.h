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
// A win returns to the trial with that enemy beaten (B-184). A wipe (B-362, D-155) costs run pillar points and returns
// to the trial with the enemy still there, or to the hub when the run is over (DefeatRules).

#pragma once

#include "Combat/Actions/ActionStructs.h"
#include "Combat/CombatOrchestrator.h"
#include "CoreMinimal.h"
#include "Combat/Defense/EDefenseType.h"
#include "GameFramework/GameModeBase.h"
#include "Memory/MemoryConditions.h"
#include "Trial/DefeatRules.h"
#include "BattleGameMode.generated.h"

class UCharacterData;
class UCombatResultsWidget;
class URunEndCardWidget;
class UFightTriggers;
class UStatusBuildupManager;
struct FFieldStrike;
class UTrialRunSubsystem;

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
     *  UCharacterDataComponent::BeginPlay initializes from the right asset. B-360b: MemberId (a party member's) is
     *  stamped the same way, so the pawn reads its own slot; invalid for an enemy. */
    APawn *SpawnCombatant(UClass *PawnClass, UCharacterData *Data, const FTransform &SpawnTransform, const FGuid &MemberId = FGuid());

public:
    /** B-214f: Strike's buildup on each of Enemies, as a hit from Source (the party's first member). Returns how many
     *  took it. Nothing when the strike is unset or carries neither an element nor a physical type. Static for the spec. */
    static int32 ApplyFieldStrikeTo(UStatusBuildupManager &Buildup, AActor *Source, const TArray<AActor *> &Enemies,
                                    const FFieldStrike &Strike);

private:
    /** B-214f: a field hit's buildup (TrialRun's pending field strike) on every enemy before the first turn; then cleared. */
    void ApplyFieldStrike(UTrialRunSubsystem &TrialRun, const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty);

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

    /** Show the results screen; false if it couldn't be created (caller returns immediately). B-465: a wipe shows the
     *  wipe card instead (ShowWipeCard). */
    bool ShowResults(const FCombatResult &Result);

    /** B-465 (D-187, D-188): the wipe card over the battle; false when it couldn't be made. */
    bool ShowWipeCard();

    /** B-465: "Get up": back to the trial, or, when the wipe ended the run, on to the run-end page. */
    UFUNCTION()
    void HandleWipeCardContinue();

    /** B-465 (D-189): the run-end page's "Return": to the hub. */
    UFUNCTION()
    void HandleRunEndContinue();

    void CloseRunEndCard();

    void ReturnToTrial();

    /** B-362 (D-155, Q-155): the party was wiped. Counts it on the run, takes each member's run pillar points
     *  (DefeatRules) and revives the party for the trip back; ReturnToTrial then goes to the hub instead when the
     *  run is over. Replaces B-072's "lose = hub". */
    void ApplyWipe();

    /** B-209 (D-100 part 2): a fight won is an event that checks memory unlock conditions; a
     *  beaten boss's MemoryOnDefeat is the memory it names. */
    void CheckFightWonMemories();

    /** No player-side combatant is below the HP it started the fight with. TODO: a heal hides a
     *  hit; count damage taken once a per-fight damage record exists. */
    bool WasPartyUnhurt() const;

    /** Per fight (D-064): each player-side combatant's HP when the fight started. */
    TMap<TWeakObjectPtr<const AActor>, int32> PartyStartHP;

    /** B-362 (per fight, D-064): this fight's wipe ended the run (3 main-boss losses or 5 wipes), so ReturnToTrial
     *  leaves for the hub, not the trial. */
    bool bWipeEndsRun = false;

    /** B-450 (D-187, per fight): the name of the last opponent that felled a party member (the wipe card names it). */
    FText LastBlowBy;

public:
    /** B-450: this fight's wipe result (empty until a wipe), for the wipe card (B-465). */
    const WoRDefeat::FWipeResult &GetLastWipeResult() const { return LastWipeResult; }

private:
    WoRDefeat::FWipeResult LastWipeResult;

    /** B-209 (Q-209d): a party member's skill and Breakthrough use count toward memory conditions. */
    UFUNCTION()
    void HandleActionStarted(AActor *Executor, const FAction &Action, int32 EnergyCost);

    /** B-209 (Q-209d): a party member's kill counts the weapon it held as a finishing blow. */
    UFUNCTION()
    void HandleTargetKilled(AActor *Killer, AActor *Victim);

    /** B-379: a party member's Perfect Guard (a parry, D-084) adds to their player's hidden Parries counter. */
    UFUNCTION()
    void HandleDefenseResolved(AActor *Defender, AActor *Attacker, EDefenseType DefenseType, bool bPerfect, int32 ImpactIndex,
                               float AttackEnergyCost);

    /** B-379: Counter +1 for Member's character, on the player who plays it (a co-op friend's machine, else here). */
    void AddHiddenCount(const AActor *Member, const TCHAR *Counter);

    /** Bind / unbind the handlers above on the action executor and (B-379) the defense system. */
    void WatchFightUsage(bool bWatch);

    /** The executor outlives the level: unbind if the battle ends without a result. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Debug: one member's fight usage in words. */
    static FString GetFightUsageString(const FMemoryFightUsage &Usage);

    /** Per fight (D-064): what each player-side combatant used, read when the fight is won. */
    TMap<TWeakObjectPtr<const AActor>, FMemoryFightUsage> FightUsage;

    UPROPERTY(Transient)
    TObjectPtr<UCombatResultsWidget> ResultsWidget;

    /** B-465: the wipe card, then the run-end page when the wipe ended the run. */
    UPROPERTY(Transient)
    TObjectPtr<URunEndCardWidget> RunEndCard;

    /** Per fight (D-064, B-212): reactions and boss triggers waiting for their mid-fight moment. */
    UPROPERTY(Transient)
    TObjectPtr<UFightTriggers> FightTriggers;

    /** Orchestrator spawned by BootstrapCombat; source of OnCombatResultReady. */
    TWeakObjectPtr<ACombatOrchestrator> Orchestrator;
};
