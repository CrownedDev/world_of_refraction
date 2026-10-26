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
#include "Combat/Encounter/EEncounterRank.h"
#include "Combat/Encounter/EFirstStrike.h"
#include "Combat/Encounter/EnemyZone.h"
#include "Trial/TrialGate.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TrialRunSubsystem.generated.h"

class UCharacterData;
class UMerchantData;
struct FMerchantStockEntry;
class UTrialData;
class UWorld;

UCLASS(Config = Game)
class WORLD_OF_REFRACTION_API UTrialRunSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
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

    /** Return to the config-authored HubLevel and clear the active trial. Safe
     *  to call when no trial is active (it is just "go to the hub"). Ignored
     *  (with a log) when HubLevel is unset. */
    UFUNCTION(BlueprintCallable, Category = "Trial")
    void ExitTrial();

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

    /** B-184: the pending fight was won; its map encounters stay beaten for the rest of the run
     *  (a rest spot doesn't bring them back). */
    void MarkPendingEncounterBeaten();

    /** True when the map encounter EncounterId was beaten earlier in this run. */
    bool IsEncounterBeaten(FName EncounterId) const { return BeatenEncounters.Contains(EncounterId); }

    int32 GetBeatenEncounterCount() const { return BeatenEncounters.Num(); }

    /** B-188: a map feature was used this run (a chest opened, lore read); What says what it gave. */
    void RecordFound(FName FeatureId, const FText &What);

    bool HasFound(FName FeatureId) const { return FoundFeatures.Contains(FeatureId); }

    int32 GetFoundCount() const { return FoundFeatures.Num(); }

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

    /** B-184: map encounters beaten this run. Run state like RunSeed; cleared on trial entry and exit. */
    TSet<FName> BeatenEncounters;

    /** B-188: map features used this run (never removed from, so it keeps the order found). Run state
     *  like BeatenEncounters; cleared on trial entry and exit; moves with it for co-op (D-065). */
    TMap<FName, FText> FoundFeatures;

    /** Carried across the OpenLevel like PendingDifficulty; moves with it for co-op (D-065). */
    EEncounterRank PendingRank = EEncounterRank::Normal;

    /** Where ExitEncounter returns to — VALUE-typed soft path, immune to the
     *  GC exposure a weak UTrialData ref would have mid-battle. */
    TSoftObjectPtr<UWorld> EncounterReturnLevel;
};
