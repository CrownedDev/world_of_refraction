// EEncounterRank.h
// How big a fight is (D-091): you can flee a normal fight, not a mini-boss or boss.

#pragma once

#include "CoreMinimal.h"
#include "EEncounterRank.generated.h"

/** Append-only: authored on encounters. */
UENUM(BlueprintType)
enum class EEncounterRank : uint8
{
    Normal UMETA(DisplayName = "Normal"),
    MiniBoss UMETA(DisplayName = "Mini-boss"),
    Boss UMETA(DisplayName = "Boss")
};
