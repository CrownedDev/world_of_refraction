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

    /** Order-agnostic identity: same BonusStat AND the two halves match in either
     *  order (A+B == B+A), so fusion stacking/countability treats half order as
     *  irrelevant. Composes FCrystalId's own operator==. */
    bool operator==(const FFusionId &Other) const
    {
        return BonusStat == Other.BonusStat &&
               ((HalfA == Other.HalfA && HalfB == Other.HalfB) ||
                (HalfA == Other.HalfB && HalfB == Other.HalfA));
    }

    bool operator!=(const FFusionId &Other) const { return !(*this == Other); }
};

/** Symmetric hash — Min/Max of the two half hashes makes it order-independent
 *  (A+B and B+A hash identically), consistent with the order-agnostic
 *  operator==. Composes FCrystalId's GetTypeHash + the bonus sub-stat. */
FORCEINLINE uint32 GetTypeHash(const FFusionId &Id)
{
    const uint32 HashA = GetTypeHash(Id.HalfA);
    const uint32 HashB = GetTypeHash(Id.HalfB);
    return HashCombine(
        HashCombine(FMath::Min(HashA, HashB), FMath::Max(HashA, HashB)),
        ::GetTypeHash(static_cast<uint8>(Id.BonusStat)));
}
