// ImpactVisualDefaults.h
// B-369 (Q-369b, Crown 2026-11-08): the fallback impact visuals when a skill has no Impact entry of its own: one per
// element, then a generic one, and how much larger a crit without its own CritImpact entry plays. One data asset at a
// fixed path (DEFAULTS_PATH); with none authored, a skill without an Impact entry plays nothing. The defence outcomes
// (Guard, Perfect Guard, Dodge) are not here: they are the defender's cosmetic slots (UCosmeticsData).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Skills/Definitions/ESpellElement.h"
#include "ImpactVisualDefaults.generated.h"

class UNiagaraSystem;

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UImpactVisualDefaults : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The asset the game reads (ContentNaming: DA_). TODO: author it in the editor (art). */
	static constexpr const TCHAR *DEFAULTS_PATH = TEXT("/Game/Data/Combat/DA_ImpactVisualDefaults.DA_ImpactVisualDefaults");

	/** The authored defaults, loaded on first use; null when none is authored. */
	static const UImpactVisualDefaults *Get();

	/** A skill's element's hit visual when the skill has no Impact entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Visuals")
	TMap<ESpellElement, TSoftObjectPtr<UNiagaraSystem>> ElementHitVFX;

	/** When neither the skill nor its element has one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Visuals")
	TSoftObjectPtr<UNiagaraSystem> GenericHitVFX;

	/** A crit without a CritImpact entry plays the hit visual this much larger. TODO: tune. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Visuals", meta = (ClampMin = "1.0"))
	float CritImpactVFXScale = 1.5f;
};
