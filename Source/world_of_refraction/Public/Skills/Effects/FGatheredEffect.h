// FGatheredEffect.h
// Carrier for the def-identity gather (R1): pairs a gathered FSkillEffect with the
// UEffectDefinition it came from — DefID = that def's GetUniqueID() (the *100 ID
// window) and BundleIndex = the effect's position within the def's Effects[]. The apply
// path will (R2) pack the stable EffectID from (DefID, BundleIndex, payload) so the SAME
// definition referenced by any source yields the SAME id (the intended merge).
//
// R1: additive only — the ...Gathered() accessors return this, but nothing packs by
// DefID/BundleIndex yet (the apply paths still read the flat accessors). BlueprintType so
// UHT stays valid if the type is ever pinned in Blueprint.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Effects/FSkillEffect.h"
#include "FGatheredEffect.generated.h"

USTRUCT(BlueprintType)
struct FGatheredEffect
{
    GENERATED_BODY()

    /** The owning UEffectDefinition's GetUniqueID() — the *100 ID window (used at R2). */
    UPROPERTY()
    int32 DefID = 0;

    /** Position of Effect within that def's Effects[] (0-based; the TRUE bundle index,
     *  stable regardless of any starting/conditional partition the gatherer applies). */
    UPROPERTY()
    int32 BundleIndex = 0;

    /** The gathered effect, by value. */
    UPROPERTY()
    FSkillEffect Effect;

    FGatheredEffect() = default;
    FGatheredEffect(int32 InDefID, int32 InBundleIndex, const FSkillEffect &InEffect)
        : DefID(InDefID), BundleIndex(InBundleIndex), Effect(InEffect)
    {
    }
};
