// ImpactVisualDefaults.h
// B-369 (Q-369b, Crown 2026-11-08): the fallback impact visuals when a skill has no Impact entry of its own: one per
// element, then a generic one, and how much larger a crit without its own CritImpact entry plays. One data asset at a
// fixed path (ImpactVisualDefaultsAsset::PATH); with none authored, a skill without an Impact entry plays nothing.
// B-394 adds the cast defaults (adapter types, the Generic preference; CastVisuals.h). The defence outcomes
// (Guard, Perfect Guard, Dodge) are not here: they are the defender's cosmetic slots (UCosmeticsData).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Equipment/Rings/EAdapterForm.h"
#include "Combat/Presentation/CastVisuals.h"
#include "ImpactVisualDefaults.generated.h"

class UNiagaraSystem;

namespace ImpactVisualDefaultsAsset
{
	/** The asset the game reads (ContentNaming: DA_). Cooked by the asset manager's ImpactVisualDefaults rule
	 *  (DefaultGame.ini, AlwaysCook). TODO: author it in the editor (art). */
	inline const TCHAR *PATH = TEXT("/Game/Data/Combat/DA_ImpactVisualDefaults.DA_ImpactVisualDefaults");
}

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UImpactVisualDefaults : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The authored defaults, loaded once and kept (a missing asset is looked up once: restart after authoring it);
	 *  null when none is authored. */
	static const UImpactVisualDefaults *Get();

	/** Debug: the element rows, the generic hit and the crit scale. */
	FString GetDefaultsString() const;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintDefaults() const;
#endif

	/** A skill's element's hit visual when the skill has no Impact entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Visuals")
	TMap<ESpellElement, TSoftObjectPtr<UNiagaraSystem>> ElementHitVFX;

	/** When neither the skill nor its element has one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Visuals")
	TSoftObjectPtr<UNiagaraSystem> GenericHitVFX;

	/** A crit without a CritImpact entry plays the hit visual this much larger. TODO: tune. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Visuals", meta = (ClampMin = "1.0"))
	float CritImpactVFXScale = 1.5f;

	// ==================== CAST VISUALS (B-394, D-170, Q-394) ====================
	// This is the one presentation-defaults asset, so the cast defaults live here too (CastVisuals.h).

	/** Each adapter type's cast flash (D-170: Ring is the default type; no per-adapter override). An adapter spell, a
	 *  Refractor's innate spell and an evolution spell play their adapter type's row (Ring with no adapter worn);
	 *  an empty row plays the skill's own Muzzle VFX. TODO: art (AssetAudit: NS_Magic_Circle1_*, element-tinted). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cast Visuals")
	TMap<EAdapterForm, TSoftObjectPtr<UNiagaraSystem>> AdapterCastVFX;

	/** D-170: the Generic class's preference when it wears both a weapon and an adapter (Q-394: Adapter). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cast Visuals")
	ECastVFXPreference GenericCastVFXPreference = ECastVFXPreference::Adapter;
};
