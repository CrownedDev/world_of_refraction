// TrialData.h
// Authored trial definition (Cluster T-A): identity + the level the trial door
// opens. Deliberately minimal — future clusters add LootPool, POIs, Boss, and
// Modifiers fields here (banked; do not author ad-hoc fields elsewhere).
//
// ⚠️ STATE WIPE: entering/leaving a trial uses UGameplayStatics::OpenLevel* —
// every actor is destroyed on transition, and the player's wallet / inventory /
// loadout live on ACTOR components (UCurrencyComponent / UInventoryComponent /
// ULoadoutComponent). Until the Pool persistence arc lands, EVERY hub↔trial
// transition wipes player state. GameInstance subsystems survive; actors do not.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Skills/Definitions/ESpellElement.h"
#include "TrialData.generated.h"

class UWorld;

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UTrialData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FText Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FText Description;

    /** The trial's element — drives the trial door's tint (T-A2) and, later,
     *  the Trial.<Element> availability-tag mapping. Generic = untyped trial. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trial")
    ESpellElement Element = ESpellElement::Generic;

    // ==================== LEVEL ====================

    /** The map this trial plays in. Soft ref — resolved by
     *  UTrialRunSubsystem::EnterTrial via OpenLevelBySoftObjectPtr. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
    TSoftObjectPtr<UWorld> Level;

    // ==================== BANKED (future clusters) ====================
    // - LootPool     : what the trial's rewards draw from
    // - POIs         : points of interest / encounter placement
    // - Boss         : the trial's boss encounter definition
    // - Modifiers    : trial-wide rule modifiers
    // Authored here when their clusters land — fields intentionally absent now.
};
