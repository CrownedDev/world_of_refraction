// EffectDefinition.h
// A named, authored effect referenced by weapons / rings / evolutions instead of
// re-authoring inline FSkillEffects. One UEffectDefinition wraps a single FSkillEffect
// (new-shape Conditions[] + Payloads[] + stacking flags). Reference several
// UEffectDefinitions on an item to bundle multiple named effects.
//
// Cluster 1: standalone asset, no readers yet — the reference arrays on the owners
// (equipment / evolution / skills) land in later clusters.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Skills/Effects/FSkillEffect.h"
#include "EffectDefinition.generated.h"

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UEffectDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** Display name for UI / shop / authoring reference. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect Definition")
    FText DisplayName;

    /** The authored effect (new-shape FSkillEffect: Conditions[] + Payloads[] + flags).
     *  One named effect; reference several UEffectDefinitions on an item to bundle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Definition")
    FSkillEffect Effect;

    /** Shop price (used by the later shop phase; harmless to author now). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect Definition", meta = (ClampMin = "0"))
    int32 Price = 0;

    // GetPrimaryAssetId() intentionally NOT overridden — mirrors USkillDataBase and
    // UEvolutionItemData, which rely on the engine default. (Only UInventoryData pins a
    // fixed PrimaryAssetType.)

    /** Details-panel button: logs the wrapped effect's Conditions[]/Payloads[] dump so
     *  the asset can be inspected without launching PIE. */
    UFUNCTION(CallInEditor, Category = "Debug")
    void LogEffect() const;
};
