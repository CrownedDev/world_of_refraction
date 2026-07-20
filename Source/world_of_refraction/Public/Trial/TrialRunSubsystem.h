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
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TrialRunSubsystem.generated.h"

class UCharacterData;
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

    /** Return to the config-authored HubLevel and clear the active trial. Safe
     *  to call when no trial is active (it is just "go to the hub"). Ignored
     *  (with a log) when HubLevel is unset. */
    UFUNCTION(BlueprintCallable, Category = "Trial")
    void ExitTrial();

    // ==================== ENCOUNTER (T-C1) ====================

    /** Stash the encounter roster and swap to Trial's EncounterLevel.
     *  ABattleGameMode consumes the stash on the far side. Ignored (with a
     *  log) when Trial / EncounterLevel / either roster is null or empty. */
    UFUNCTION(BlueprintCallable, Category = "Trial|Encounter")
    void EnterEncounter(UTrialData *Trial, const TArray<UCharacterData *> &BattleParty,
                        const TArray<UCharacterData *> &OpposingParty, EAIDifficulty Difficulty);

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

    /** The trial whose level we are in (or transitioning to), or null in the
     *  hub. Survives the level load — this subsystem is GameInstance-scoped. */
    UFUNCTION(BlueprintPure, Category = "Trial")
    UTrialData *GetActiveTrial() const { return ActiveTrial.Get(); }

    // ==================== DEBUG ====================

    /** Formatted snapshot: active trial (or none) + configured hub level. */
    UFUNCTION(BlueprintPure, Category = "Debug")
    FString GetTrialRunString() const;

private:
    /** The map ExitTrial returns to — authored in DefaultGame.ini. */
    UPROPERTY(Config)
    TSoftObjectPtr<UWorld> HubLevel;

    /** Weak: the data asset outlives the transition anyway (asset-registry
     *  owned); weak keeps the subsystem from pinning it. */
    TWeakObjectPtr<UTrialData> ActiveTrial;

    /** Encounter roster awaiting the battle level (T-C1). HARD refs — these
     *  pin the authored assets across the OpenLevel (see header ⚠️). */
    UPROPERTY()
    TArray<TObjectPtr<UCharacterData>> PendingBattleParty;

    UPROPERTY()
    TArray<TObjectPtr<UCharacterData>> PendingOpposingParty;

    EAIDifficulty PendingDifficulty = EAIDifficulty::Medium;

    /** Where ExitEncounter returns to — VALUE-typed soft path, immune to the
     *  GC exposure a weak UTrialData ref would have mid-battle. */
    TSoftObjectPtr<UWorld> EncounterReturnLevel;
};
