// FFusionId.h
// Design-time identity for a Fusion attachment (EAttachedItemKind::Fusion): two
// crystal-identity halves plus the sub-stat the fusion bonus targets. Every valid
// fusion is stat-stone + one contributor (stat-stone / AbilityStone / gem crystal);
// the half/pair rules are enforced by author-time validation (CrystalTypeHelpers
// IsValidFusionPair / IsElementalFusion), not by editor grey-out.
//
// AUTHORING ONLY. Runtime resolution (both-halves-apply + the
// (TierValue(A)+TierValue(B))/2 bonus + a FRuntimeAttachedItem Fusion branch) is a
// separate follow-up.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "FFusionId.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FFusionId
{
    GENERATED_BODY()

    /** First fusion half — a crystal identity (Type + Tier). Authoring convention: a
     *  stat-stone. Validated as part of the pair (order-agnostic). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fusion")
    FCrystalId HalfA;

    /** Second fusion half — a crystal identity. Stat-stone, AbilityStone, or a gem
     *  crystal (the at-most-one-crystal rule is enforced in validation). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fusion")
    FCrystalId HalfB;

    /** The wired sub-stat the fusion bonus targets. None is rejected by validation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fusion")
    ESubStat BonusStat = ESubStat::None;

    // TODO(runtime): symmetric operator== + GetTypeHash (sort halves so A+B == B+A) —
    // needed for fusion stacking/countability at runtime, NOT for authoring. Deferred to
    // the runtime-resolution follow-up; do not add the operators here.
};
