// TrialRunSubsystem.h
// Owns the hub↔trial level transition (Cluster T-A): ATrialDoor::Interact()
// calls EnterTrial (loads the trial's level) or ExitTrial (returns to the
// config-authored HubLevel). Mirrors UMerchantShopSubsystem's outer shape:
// GameInstanceSubsystem + Config-authored soft ref + weak state.
//
// ⚠️ STATE WIPE: OpenLevel destroys every actor. The player's wallet /
// inventory / loadout are ACTOR components, so EVERY hub↔trial transition
// wipes player state until the Pool persistence arc lands. This subsystem
// (and its ActiveTrial) survives — it is GameInstance-scoped — but nothing
// actor-resident does. Do not "fix" this locally; persistence is the Pool arc.
//
// HubLevel comes from config (DefaultGame.ini
// [/Script/world_of_refraction.TrialRunSubsystem]). Null/unset = ExitTrial
// logs and no-ops rather than stranding the player in a broken transition.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TrialRunSubsystem.generated.h"

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
};
