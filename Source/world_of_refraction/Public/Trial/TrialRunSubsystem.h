// TrialRunSubsystem.h
// Owns the hub↔trial level transition (Cluster T-A): ATrialDoor::Interact()
// calls EnterTrial (loads the trial's level) or ExitTrial (returns to the
// config-authored HubLevel). Mirrors UMerchantShopSubsystem's outer shape:
// GameInstanceSubsystem + Config-authored soft ref + weak state.
//
// Cluster T-C1 adds the trial↔battle transition: UEncounterComponent stashes
// the encounter roster here (EnterEncounter → OpenLevel to the trial's
// EncounterLevel), ABattleGameMode consumes it to spawn combatants, and
// ExitEncounter returns to the trial level on combat end.
//
// ⚠️ STATE WIPE: OpenLevel destroys every actor. The player's wallet /
// inventory / loadout are ACTOR components, so EVERY transition — hub↔trial
// AND trial↔battle (encounter entry and exit both) — wipes player state, and
// combatants re-enter each fight at CharacterData-default HP/EP. Deferred to
// the persistence keystone arc. This subsystem survives — it is
// GameInstance-scoped — but nothing actor-resident does.
//
// ⚠️ AUTHORED ASSETS ONLY: the pending-combatant refs keep authored
// UCharacterData assets alive across the swap (hard UPROPERTY refs). A
// runtime-NewObject'd CharacterData outered to the dying world will NOT
// survive — battle encounters require authored DA_Character_* assets.
//
// HubLevel comes from config (DefaultGame.ini
// [/Script/world_of_refraction.TrialRunSubsystem]). Null/unset = ExitTrial
// logs and no-ops rather than stranding the player in a broken transition.

#pragma once

#include "AI/EAIDifficulty.h"
#include "Character/EWorldPillar.h"
#include "Combat/Encounter/EEncounterRank.h"
#include "Combat/Encounter/EFirstStrike.h"
#include "Combat/Encounter/EnemyZone.h"
#include "Trial/TrialGate.h"
#include "Trial/RunRewards.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TrialRunSubsystem.generated.h"

class UCharacterData;
class UMemoryData;
class UMerchantData;
struct FMerchantStockEntry;
class UTrialData;
class UWorld;
struct FActorsInitializedParams;

namespace MiniBossRevival
{
	/** B-187b (Q-187b): how many times one mini-boss can be revived in a run. TODO: tune. */
	constexpr int32 MAX_REVIVALS = 3;

	/** B-187b (Q-187b): tier steps a revived mini-boss's team gets per revival (HP and level drops). */
	constexpr int32 TIER_STEPS_PER_REVIVAL = 1;
}

UCLASS(Config = Game)
class WORLD_OF_REFRACTION_API UTrialRunSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** B-362c: listens for each loaded world, to place the run's Prism drop pickup on the trial map. */
    virtual void Initialize(FSubsystemCollectionBase &Collection) override;
    virtual void Deinitialize() override;

    /** Load Trial's level and mark it active. Defensively closes an open shop
     *  window first (the modal bracket must not straddle a level transition).
     *  Ignored (with a log) when Trial or Trial->Level is null/unset. */
    UFUNCTION(BlueprintCallable, Category = "Trial")
    void EnterTrial(UTrialData *Trial);

    /** B-183: what the door asked, for the run EnterTrial starts next. VariationIndex is into the
     *  trial's Variations (INDEX_NONE when it has none). Kept until the run ends. */
    UFUNCTION(BlueprintCallable, Category = "Trial|Gate")
    void SetGateChoice(int32 InVariationIndex, ETrialDifficulty InDifficulty);

    UFUNCTION(BlueprintPure, Category = "Trial|Gate")
    ETrialDifficulty GetTrialDifficulty() const { return TrialDifficulty; }

    UFUNCTION(BlueprintPure, Category = "Trial|Gate")
    int32 GetVariationIndex() const { return VariationIndex; }

    /** The chosen variation of the active trial, or null. */
    const FTrialVariation *GetChosenVariation() const;

    /** Enemies' max HP and damage for this run, × this (Q-183). */
    UFUNCTION(BlueprintPure, Category = "Trial|Gate")
    float GetEnemyStrengthMultiplier() const { return TrialGate::GetEnemyStrengthMultiplier(TrialDifficulty); }

    /** The run's payout, × this (D-006, Q-183). TODO: read by the run-end payout (B-096). */
    UFUNCTION(BlueprintPure, Category = "Trial|Gate")
    float GetPayoutMultiplier() const { return TrialGate::GetPayoutMultiplier(TrialDifficulty); }

    /** B-347 (D-137): what a run's start does to the party's gear and Gold, without the level travel: run Gold to its
     *  start (none, B-178), a layer left open ended, and the run layer opened with the party's worn loadouts, free.
     *  EnterTrial calls it; specs call it directly. */
    void StartRunGearAndGold();

    /** Return to the config-authored HubLevel and clear the active trial. Safe
     *  to call when no trial is active (it is just "go to the hub"). Ignored
     *  (with a log) when HubLevel is unset. */
    UFUNCTION(BlueprintCallable, Category = "Trial")
    void ExitTrial();

    /** B-362: ExitTrial for a call made mid-teardown (from the combat result broadcast): runs it one tick later, as
     *  ExitEncounter defers its OpenLevel. With no HubLevel configured it logs an error and falls back to ExitEncounter
     *  (the trial), so the player is never stranded in the battle level. */
    void ExitTrialNextTick();

    // ==================== ENCOUNTER (T-C1) ====================

    /** Stash the encounter roster and swap to Trial's EncounterLevel.
     *  ABattleGameMode consumes the stash on the far side. Ignored (with a
     *  log) when Trial / EncounterLevel / either roster is null or empty. Rank (D-091) decides
     *  whether the fight can be fled. */
    UFUNCTION(BlueprintCallable, Category = "Trial|Encounter")
    void EnterEncounter(UTrialData *Trial, const TArray<UCharacterData *> &BattleParty,
                        const TArray<UCharacterData *> &OpposingParty, EAIDifficulty Difficulty,
                        EEncounterRank Rank);

    /** Return to the trial level the encounter came from (fallback: HubLevel).
     *  The OpenLevel is deferred one tick — this is called from
     *  OnCombatResultReady, mid-orchestrator-teardown. */
    UFUNCTION(BlueprintCallable, Category = "Trial|Encounter")
    void ExitEncounter();

    /** True while a stashed roster awaits an ABattleGameMode to consume it. */
    UFUNCTION(BlueprintPure, Category = "Trial|Encounter")
    bool HasPendingEncounter() const { return PendingBattleParty.Num() > 0 && PendingOpposingParty.Num() > 0; }

    UFUNCTION(BlueprintPure, Category = "Trial|Encounter")
    TArray<UCharacterData *> GetPendingBattleParty() const { return TArray<UCharacterData *>(PendingBattleParty); }

    UFUNCTION(BlueprintPure, Category = "Trial|Encounter")
    TArray<UCharacterData *> GetPendingOpposingParty() const { return TArray<UCharacterData *>(PendingOpposingParty); }

    UFUNCTION(BlueprintPure, Category = "Trial|Encounter")
    EAIDifficulty GetPendingDifficulty() const { return PendingDifficulty; }

    UFUNCTION(BlueprintPure, Category = "Trial|Encounter")
    EEncounterRank GetPendingRank() const { return PendingRank; }

    /** B-184: the map encounters the pending fight came from (every team caught). Call before
     *  EnterEncounter; a win marks them all beaten. */
    void SetPendingEncounterSources(const TArray<FName> &EncounterIds) { PendingEncounterSources = EncounterIds; }

    /** B-224b (Q-224c): each opposing member's own rank for its HP, parallel to the opposing party. Call before
     *  EnterEncounter. A fight entered without it (a map feature's boss) gives every enemy the fight's rank. */
    void SetPendingEnemyRanks(const TArray<EEncounterRank> &Ranks) { PendingEnemyRanks = Ranks; }

    /** The rank whose HP the Index-th opposing member gets: its own when one was set for every member, else the
     *  fight's rank (GetPendingRank). */
    EEncounterRank GetPendingEnemyRank(int32 Index) const
    {
        return PendingEnemyRanks.Num() == PendingOpposingParty.Num() && PendingEnemyRanks.IsValidIndex(Index) ? PendingEnemyRanks[Index] : PendingRank;
    }

    // ---- B-223 (D-102): pillar levels earned this run, per character (survive the battle level's travel) ----

    /** Levels Character earned (dropped or bought) this run in Pillar, on top of its asset level and head-start. */
    int32 GetRunEarnedLevels(FName CharacterKey, EWorldPillar Pillar) const;
    void AddRunEarnedLevels(FName CharacterKey, EWorldPillar Pillar, int32 Count);
    /** Levels Character bought from a vendor this run (the rising price, D-102). */
    int32 GetRunLevelPurchases(FName CharacterKey) const;
    void AddRunLevelPurchase(FName CharacterKey);
    /** Forgets one character's run levels, purchases and waiting dropped levels (UCharacterDataComponent::ResetRunWorldStats). */
    void ForgetRunLevels(FName CharacterKey);

    /** B-223b (Q-223 changed by Crown, 2026-11-03): each character's own dropped levels, not yet given to one of its
     *  pillars (wor.UI.LevelDrop). A character's levels go only to its own pillars. */
    int32 GetUnspentLevels(FName CharacterKey) const;
    int32 GetTotalUnspentLevels() const;
    void AddUnspentLevels(FName CharacterKey, int32 Count);
    /** Takes one of the character's unspent levels; false when it has none. */
    bool TakeUnspentLevel(FName CharacterKey);

    /** Debug: every character's run levels and the unspent count. wor.WorldStat.Print. */
    FString GetRunLevelsString() const;

    /** B-184: the pending fight was won; its map encounters stay beaten for the rest of the run
     *  (a rest spot doesn't bring them back). */
    void MarkPendingEncounterBeaten();

    /** B-187b: marks one map encounter beaten in a fight of Rank (a mini-boss's leaves a rest spot). The pending
     *  fight's sources go through here. */
    void MarkEncounterBeaten(FName EncounterId, EEncounterRank Rank);

    /** True when the map encounter EncounterId was beaten earlier in this run. */
    bool IsEncounterBeaten(FName EncounterId) const { return BeatenEncounters.Contains(EncounterId); }

    int32 GetBeatenEncounterCount() const { return BeatenEncounters.Num(); }

    // ==================== MINI-BOSS REST SPOTS (B-187b, RunStructure §4, Q-187b) ====================

    /** B-187b: a mini-boss fight beaten this run (and not revived since) leaves a rest spot where it stood. */
    bool IsBeatenMiniBoss(FName EncounterId) const { return BeatenMiniBosses.Contains(EncounterId); }

    /** How many times the mini-boss EncounterId was revived this run. */
    int32 GetEncounterRevivals(FName EncounterId) const { return EncounterRevivals.FindRef(EncounterId); }

    /** A beaten mini-boss under MiniBossRevival::MAX_REVIVALS can be revived (the main boss is a map feature, never
     *  an encounter, so it never is). */
    bool CanReviveEncounter(FName EncounterId) const;

    /** Brings the mini-boss EncounterId back, one revival stronger: it is no longer beaten (the next load of the map
     *  places it again) and its revival count goes up. False when CanReviveEncounter is. */
    bool ReviveEncounter(FName EncounterId);

    /** Reloads the active trial's level (the server travels for everyone; a client can't). */
    void ReloadTrialLevel();

    /** B-187b: the most tier steps any of the pending fight's enemies gets (debug and specs). */
    void SetPendingTierSteps(int32 Steps) { PendingTierSteps = FMath::Max(0, Steps); }
    int32 GetPendingTierSteps() const { return PendingTierSteps; }

    /** B-187b review: each opposing member's own tier steps (its team's revivals), parallel to the opposing party, as
     *  SetPendingEnemyRanks. Call before EnterEncounter. */
    void SetPendingEnemyTierSteps(const TArray<int32> &Steps);

    /** The Index-th opposing member's tier steps; 0 when none were set for every member. */
    int32 GetPendingEnemyTierSteps(int32 Index) const
    {
        return PendingEnemyTierSteps.Num() == PendingOpposingParty.Num() && PendingEnemyTierSteps.IsValidIndex(Index) ? PendingEnemyTierSteps[Index] : 0;
    }

    /** B-187b review: the pending fight's caught teams whose own rank is a mini-boss's (a win leaves a rest spot for
     *  them only, not for an elite team caught with them). Call before EnterEncounter. */
    void SetPendingMiniBossSources(const TArray<FName> &EncounterIds) { PendingMiniBossSources = EncounterIds; }

    /** Debug: the beaten mini-bosses and their revivals. */
    FString GetRevivalsString() const;

    /** B-187c: the beaten mini-bosses and their revival counts, for the save. */
    TArray<FName> GetBeatenMiniBosses() const { return BeatenMiniBosses.Array(); }
    const TMap<FName, int32> &GetEncounterRevivalCounts() const { return EncounterRevivals; }

    /** B-187c: puts saved marks and revivals back (after RestoreRun, which clears them). Marks for encounters not
     *  beaten in the restored run are dropped. */
    void RestoreMiniBossRevivals(const TArray<FName> &MiniBosses, const TMap<FName, int32> &Revivals);

    /** B-187c: where the party stood when it last rested (ATrialRestSpot::Interact). */
    void NoteRestStand(const FTransform &Stand) { RestStand = Stand; bHasRestStand = true; }

    /** B-187c: the next trial level load puts the local player there (ATrialGameMode::RestartPlayer), once. */
    void SetPendingReturnStand(const FTransform &Stand) { PendingReturnStand = Stand; bHasPendingReturnStand = true; }

    /** B-187c: the pending return stand, consumed (true once per SetPendingReturnStand). */
    bool ConsumePendingReturnStand(FTransform &OutStand);

    /** B-187c: the last rest stand, if the party rested this level visit. */
    bool GetRestStand(FTransform &OutStand) const { OutStand = RestStand; return bHasRestStand; }

    /** B-361b (D-154): the trial's boss fell this run, so the run counts as finished: a co-op run that ends after it
     *  pays every player Mentor Tokens (ExitTrial). */
    void MarkRunBossBeaten() { bRunBossBeaten = true; }
    bool IsRunBossBeaten() const { return bRunBossBeaten; }

    // ==================== B-073: REWARDS ====================

    /** B-073 (server): a fight won: run Prisms to every player now, and the rank's share into the run's tally. */
    void RecordFightReward(UWorld *World, EEncounterRank Rank);

    /** B-073: a chest opened: its share into the run's tally. */
    void RecordChestReward();

    /** B-073: the tally so far (paid only when the run ends, D-006: nothing banked mid-run). */
    const FRunPayout &GetRunTally() const { return RunTally; }

    /** B-073: what the run's end would pay now (the tally scaled by difficulty, boss bonus and multiplier). */
    FRunPayout GetRunEndPayout() const;

    /** B-073: the run's end pays the tally to every player (ExitTrial calls it, win or lose; specs call it). Empties
     *  the tally so it can't be paid twice. Returns how many players were paid. */
    int32 PayRunEnd(UWorld *World);

    /** B-073 / B-096: the payout multiplier (passives raise it); BASE 1.0. */
    float GetPayoutMultiplier() const { return PayoutMultiplier; }
    void SetPayoutMultiplier(float Multiplier) { PayoutMultiplier = FMath::Max(Multiplier, 0.0f); }

    /** B-187 (RunStructure §6): the party leaves the run at a rest spot: the end pays the tally at
     *  RunRewards::LEAVE_EARLY_SHARE with no boss bonus, then back to the hub next tick. Once per run. */
    void LeaveRunEarly();
    bool IsLeavingEarly() const { return bLeavingEarly; }

    /** B-073: the save puts a resumed run's tally back. */
    void RestoreRunTally(const FRunPayout &Tally) { RunTally = Tally; }

    // ==================== LOSING A FIGHT (B-362, D-155, Q-155) ====================

    /** Wipes this run (every party wipe counts, a main-boss loss too) and losses to the main boss. Saved with the run. */
    int32 GetRunWipes() const { return RunWipes; }
    int32 GetRunMainBossLosses() const { return RunMainBossLosses; }

    /** The party was wiped by an enemy of Rank: counts the wipe (and the main-boss loss). Returns true when the run is
     *  now over (DefeatRules::RunEnds). */
    bool RecordWipe(EEncounterRank Rank);

    /** OBSOLETE (B-362b, D-160: a wipe drops the run's Prisms instead; no game caller; delete after PIE).
     *  Takes up to MaxPoints of CharacterKey's run pillar points (earned or bought this run), one at a time from the
     *  pillar with the most (DefeatRules::PickPillarToLose). Returns how many it took; the asset's levels and the head
     *  start are never touched. */
    int32 LoseRunPoints(FName CharacterKey, int32 MaxPoints);

    /** B-362b (D-160): the run's Prisms dropped where the party last fell (the Souls bloodstain: only the newest drop
     *  exists). Amount is the run balance (ECurrencyType::Gold, shown as Prisms, D-159). B-362c (D-160 as changed):
     *  the drop lies at the pending fight's map spot (GetPendingFightSpot), a pickup there gives it back
     *  (APrismDropPickup, TakeDrop). Returns the previous drop's amount, now lost for good. */
    int32 DropPrisms(int32 Amount);

    /** B-362c: the drop's amount, cleared (the pickup was taken). 0 when there is none. */
    int32 TakeDrop();

    /** OBSOLETE (B-362c, D-160 changed: one path, the pickup; no game caller; delete after PIE). The drop when the
     *  pending fight came from one of its map teams: returns its amount and clears it; else 0. */
    int32 TakeDropIfPendingFight();

    int32 GetDroppedPrisms() const { return DroppedPrisms; }

    /** B-235 (D-160 co-op): another player's own drop at the same spot, keyed by WoRDefeat::PlayerDropKey. The local
     *  (host / solo) player's drop stays DropPrisms / TakeDrop, the one the save keeps; these are not saved yet (a
     *  client's run copy is B-234). Returns that player's earlier drop, now lost for good (the newest drop only). */
    int32 DropPrismsForPlayer(const FString &PlayerKey, int32 Amount);

    /** B-235: that player's drop, cleared. 0 when there is none. */
    int32 TakeDropForPlayer(const FString &PlayerKey);

    int32 GetDroppedPrismsForPlayer(const FString &PlayerKey) const;

    /** B-235: every player's drop at the spot (the local one and the others'): the pickup lies while it is above 0. */
    int32 GetTotalDroppedPrisms() const;
    /** OBSOLETE (B-362c): the drop fight's map teams; logged only. */
    const TArray<FName> &GetDropEncounters() const { return DropEncounters; }

    /** B-362c: where the drop lies on the trial map. False when the fight had no map spot (a debug or scripted fight
     *  started off the trial map): the pickup then lies at the trial's start. */
    bool GetDropSpot(FVector &OutSpot) const;

    /** Save restore: puts the run's drop back (B-362c: with its spot). */
    void RestoreDrop(int32 Amount, bool bHasSpot, const FVector &Spot);

    /** B-362c: where the party stood on the trial map when the pending fight started (EnterEncounter records the
     *  local pawn's spot while the active trial's level is the world). False for a fight entered from elsewhere. */
    bool GetPendingFightSpot(FVector &OutSpot) const;

    /** B-362c: tests and debug fights set the spot by hand. Call before EnterEncounter (it overwrites it on the map). */
    void SetPendingFightSpot(const FVector &Spot);

    /** Is World the active trial's level (where the drop's pickup is placed)? */
    bool IsActiveTrialWorld(const UWorld *World) const;

    /** The last rest spot the party used this run (NAME_None = the trial's start). TODO: B-187 calls NoteRestSpotUsed
     *  from the rest spot and the trial level places the returning party there. */
    FName GetLastRestSpot() const { return LastRestSpot; }
    void NoteRestSpotUsed(FName RestSpotId) { LastRestSpot = RestSpotId; }

    /** Save restore: puts the run's loss counters back (RestoreRun does not take them). */
    void RestoreRunLosses(int32 Wipes, int32 MainBossLosses, FName RestSpotId);

    /** B-188: a map feature was used this run (a chest opened, lore read); What says what it gave. */
    void RecordFound(FName FeatureId, const FText &What);

    bool HasFound(FName FeatureId) const { return FoundFeatures.Contains(FeatureId); }

    int32 GetFoundCount() const { return FoundFeatures.Num(); }

    /** B-190: a memory fragment was picked up this run (boss modifiers apply to this run, B-191).
     *  False when null or already collected this run. */
    bool CollectMemory(UMemoryData *Memory);

    const TArray<TObjectPtr<UMemoryData>> &GetRunMemories() const { return RunMemories; }

    // ==================== SAVED RUN (B-208, Q-208) ====================

    /** A trial run is under way (entered, not exited). */
    bool IsRunActive() const { return ActiveTrial != nullptr && RunSeed != 0; }

    const TSet<FName> &GetBeatenEncounters() const { return BeatenEncounters; }
    const TMap<FName, FText> &GetFoundFeatures() const { return FoundFeatures; }

    /** Puts a saved run back without EnterTrial's fresh start (no full heal, no new seed, the
     *  map's beaten fights and found features kept). Opens nothing: ResumeRun does. */
    void RestoreRun(UTrialData *Trial, int32 InRunSeed, int32 InVariationIndex, ETrialDifficulty InDifficulty,
                    const TArray<FName> &Beaten, const TMap<FName, FText> &Found, const TArray<UMemoryData *> &Memories);

    /** Opens the run's trial level (the player lands at its start). False when no run is active. */
    bool ResumeRun();

    /** Is World the configured hub level, where a saved run is resumed from? */
    bool IsHubWorld(const UWorld *World) const;

    /** B-185: who struck first on the map, for the pending fight. Call before EnterEncounter. */
    void SetPendingFirstStrike(EFirstStrike FirstStrike) { PendingFirstStrike = FirstStrike; }

    UFUNCTION(BlueprintPure, Category = "Trial|Encounter")
    EFirstStrike GetPendingFirstStrike() const { return PendingFirstStrike; }

    /** B-186: the map zone the pending fight started in. Call before EnterEncounter. */
    void SetPendingZone(const FEncounterZoneInfo &Zone) { PendingZone = Zone; }

    UFUNCTION(BlueprintPure, Category = "Trial|Encounter")
    FEncounterZoneInfo GetPendingZone() const { return PendingZone; }

    /** The trial whose level we are in (or transitioning to), or null in the
     *  hub. Survives the level load — this subsystem is GameInstance-scoped. */
    UFUNCTION(BlueprintPure, Category = "Trial")
    UTrialData *GetActiveTrial() const { return ActiveTrial; }

    /** This run's seed, rolled on EnterTrial (0 in the hub, so it also says whether a run is on).
     *  Trial vendors roll their stock from it. */
    int32 GetRunSeed() const { return RunSeed; }

    /** The stock Merchant shows now: rolled for this run in a trial, the full list at home (B-179). */
    TArray<FMerchantStockEntry> GetMerchantStock(const UMerchantData *Merchant) const;

    /** One line per stock line GetMerchantStock gives Merchant (what its shop shows now). */
    FString GetMerchantStockString(const UMerchantData *Merchant) const;

    // ==================== DEBUG ====================

    /** Formatted snapshot: active trial (or none) + configured hub level. */
    UFUNCTION(BlueprintPure, Category = "Debug")
    FString GetTrialRunString() const;

private:
    /** B-227: every level change. A listen server travels seamlessly and takes its clients along
     *  (World->ServerTravel; the game modes set bUseSeamlessTravel); standalone keeps OpenLevel. A client
     *  doesn't travel on its own (it follows the server; its inputs reach the server with B-229). */
    void TravelTo(const TSoftObjectPtr<UWorld> &Level);

    /** The map ExitTrial returns to — authored in DefaultGame.ini. */
    UPROPERTY(Config)
    TSoftObjectPtr<UWorld> HubLevel;

    /** Strong (B-070): in a packaged build nothing else may hold the trial asset,
     *  so a weak pointer let GC drop it mid-run. Cleared on ExitTrial. */
    UPROPERTY(Transient)
    TObjectPtr<UTrialData> ActiveTrial;

    int32 RunSeed = 0;

    int32 VariationIndex = INDEX_NONE;
    /** The gate's run difficulty (B-183); separate from the per-encounter AI difficulty. */
    ETrialDifficulty TrialDifficulty = TrialGate::DEFAULT_DIFFICULTY;

    /** Encounter roster awaiting the battle level (T-C1). HARD refs — these
     *  pin the authored assets across the OpenLevel (see header ⚠️). */
    UPROPERTY()
    TArray<TObjectPtr<UCharacterData>> PendingBattleParty;

    UPROPERTY()
    TArray<TObjectPtr<UCharacterData>> PendingOpposingParty;

    EAIDifficulty PendingDifficulty = EAIDifficulty::Medium;

    /** B-185: carried across the OpenLevel like PendingRank; moves with it for co-op (D-065). */
    EFirstStrike PendingFirstStrike = EFirstStrike::None;

    /** B-186: carried across the OpenLevel like PendingRank (D-065). */
    FEncounterZoneInfo PendingZone;

    /** B-184: map encounters of the pending fight. */
    TArray<FName> PendingEncounterSources;

    /** B-224b: each opposing member's rank, carried across the OpenLevel like PendingRank (D-065); reset with the
     *  encounter sources. */
    TArray<EEncounterRank> PendingEnemyRanks;

    /** B-223: one character's pillar levels earned this run and how many it bought. */
    struct FRunLevels
    {
        int32 Mind = 0;
        int32 Body = 0;
        int32 Spirit = 0;
        int32 Purchases = 0;
        int32 &For(EWorldPillar Pillar) { return Pillar == EWorldPillar::Body ? Body : Pillar == EWorldPillar::Spirit ? Spirit : Mind; }
        int32 Get(EWorldPillar Pillar) const { return Pillar == EWorldPillar::Body ? Body : Pillar == EWorldPillar::Spirit ? Spirit : Mind; }
    };
    /** B-223: run state like BeatenEncounters (cleared on trial entry and exit). TODO: not in the save yet (Q-223). */
    TMap<FName, FRunLevels> RunLevels;
    /** B-223b: per character key. */
    TMap<FName, int32> UnspentLevels;

    /** B-184: map encounters beaten this run. Run state like RunSeed; cleared on trial entry and exit. */
    TSet<FName> BeatenEncounters;

    /** B-187b: the beaten encounters whose fight was a mini-boss's (each leaves a rest spot). */
    TSet<FName> BeatenMiniBosses;

    /** B-187b: revivals per mini-boss encounter this run. */
    TMap<FName, int32> EncounterRevivals;

    /** B-187b: tier steps for the pending fight's enemies (SetPendingTierSteps). */
    int32 PendingTierSteps = 0;
    TArray<int32> PendingEnemyTierSteps;
    TArray<FName> PendingMiniBossSources;

    /** B-187c: where the party last rested, and where the next trial load puts it after a revive. */
    FTransform RestStand;
    bool bHasRestStand = false;
    FTransform PendingReturnStand;
    bool bHasPendingReturnStand = false;

    /** B-361b: run state like BeatenEncounters (cleared on trial entry and exit; moves with it for co-op, D-065).
     *  TODO: not in the save yet (a resumed run forgets it, as Q-223's run levels). */
    bool bRunBossBeaten = false;

    /** B-073: hub Gold and essences earned this run, paid at its end. */
    FRunPayout RunTally;
    float PayoutMultiplier = RunRewards::BASE_PAYOUT_MULTIPLIER;
    /** B-187: the party chose to leave at a rest spot (the run's end pays the leave share). */
    bool bLeavingEarly = false;

    /** B-362: run state like BeatenEncounters (cleared on trial entry and exit; saved with the run in FSavedRun). */
    int32 RunWipes = 0;
    int32 RunMainBossLosses = 0;
    FName LastRestSpot;

    /** B-362b: run state like RunWipes (cleared on trial entry and exit; saved with the run). */
    int32 DroppedPrisms = 0;
    /** B-235: the other players' drops (see DropPrismsForPlayer). */
    TMap<FString, int32> OtherPlayerDrops;
    TArray<FName> DropEncounters;
    /** B-362c: the drop's spot, run state like DroppedPrisms (saved with the run). */
    FVector DropSpot = FVector::ZeroVector;
    bool bDropHasSpot = false;

    /** B-362c: carried across the OpenLevel like PendingRank; reset with the encounter sources. */
    FVector PendingFightSpot = FVector::ZeroVector;
    bool bHasPendingFightSpot = false;

    /** B-362c: places the run's drop pickup when the active trial's level is ready. */
    void HandleWorldActorsInitialized(const FActorsInitializedParams &Params);
    FDelegateHandle WorldActorsInitializedHandle;

    /** B-188: map features used this run (never removed from, so it keeps the order found). Run state
     *  like BeatenEncounters; cleared on trial entry and exit; moves with it for co-op (D-065). */
    TMap<FName, FText> FoundFeatures;

    /** B-190: memories collected this run. Run state like FoundFeatures (D-065). */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMemoryData>> RunMemories;

    /** Carried across the OpenLevel like PendingDifficulty; moves with it for co-op (D-065). */
    EEncounterRank PendingRank = EEncounterRank::Normal;

    /** Where ExitEncounter returns to — VALUE-typed soft path, immune to the
     *  GC exposure a weak UTrialData ref would have mid-battle. */
    TSoftObjectPtr<UWorld> EncounterReturnLevel;
};
