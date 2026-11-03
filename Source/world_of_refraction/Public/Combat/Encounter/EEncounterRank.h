// EEncounterRank.h
// How big a fight is (D-091): you can flee a normal fight, not a mini-boss or boss. Elite (B-224,
// Q-224: a beefier normal fight, x1.5 HP, fleeable) was appended after Boss, so compare ranks with
// EncounterRankSeverity, not the enum value.

#pragma once

#include "CoreMinimal.h"
#include "EEncounterRank.generated.h"

/** Append-only: authored on encounters. */
UENUM(BlueprintType)
enum class EEncounterRank : uint8
{
    Normal UMETA(DisplayName = "Normal"),
    MiniBoss UMETA(DisplayName = "Mini-boss"),
    Boss UMETA(DisplayName = "Boss"),
    Elite UMETA(DisplayName = "Elite")
};

/** How hard a rank is, for "the hardest rank rules the fight": Normal < Elite < Mini-boss < Boss. */
inline int32 EncounterRankSeverity(EEncounterRank Rank)
{
    switch (Rank)
    {
    case EEncounterRank::Elite:    return 1;
    case EEncounterRank::MiniBoss: return 2;
    case EEncounterRank::Boss:     return 3;
    case EEncounterRank::Normal:
    default:                       return 0;
    }
}

/** The harder of two ranks by EncounterRankSeverity. */
inline EEncounterRank HarderEncounterRank(EEncounterRank A, EEncounterRank B)
{
    return EncounterRankSeverity(B) > EncounterRankSeverity(A) ? B : A;
}
