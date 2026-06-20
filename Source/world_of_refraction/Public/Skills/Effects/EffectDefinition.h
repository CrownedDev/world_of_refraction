// EffectDefinition.h
// A named, authored effect BUNDLE referenced by weapons / rings / evolutions instead of
// re-authoring inline FSkillEffects. One UEffectDefinition wraps a list of new-shape
// FSkillEffects (each: Conditions[] + Payloads[] + stacking flags). Reference the same
// bundle from several items to share it; each item resolves it into its own apply path.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Skills/Effects/FSkillEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "EffectDefinition.generated.h"

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UEffectDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** Display name for UI / shop / authoring reference. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect Definition")
    FText DisplayName;

    /** The authored effect bundle (new-shape FSkillEffects: Conditions[] + Payloads[] +
     *  flags each). One or more named effects applied together when this bundle is
     *  referenced; an item referencing it gets every effect in the list. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Definition")
    TArray<FSkillEffect> Effects;

    /** Shop price (used by the later shop phase; harmless to author now). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect Definition", meta = (ClampMin = "0"))
    int32 Price = 0;

    // GetPrimaryAssetId() intentionally NOT overridden — mirrors USkillDataBase and
    // UEvolutionItemData, which rely on the engine default. (Only UInventoryData pins a
    // fixed PrimaryAssetType.)

    /** Details-panel button: logs every bundled effect's Conditions[]/Payloads[] dump so
     *  the asset can be inspected without launching PIE. */
    UFUNCTION(CallInEditor, Category = "Debug")
    void LogEffect() const;

#if WITH_EDITOR
    /** Threshold-visibility flags live on each bundled effect (not the owner that only
     *  references this asset), so the bundle syncs its own — keeps the Threshold
     *  EditCondition gating live when authoring effects here. */
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent &PropertyChangedEvent) override;

    /** The SOLE stable-ID guard under def-identity packing: a bundle's effects index
     *  0..N-1 in this def's DefID*100 window, so the effect count (<=10) and each effect's
     *  payload count (<=9) are capped here — the only collision surface. */
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};
