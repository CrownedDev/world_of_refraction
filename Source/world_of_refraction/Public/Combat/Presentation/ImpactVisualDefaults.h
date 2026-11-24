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

/** One infusion level's look (Crown 2026-11-24): how much brighter (the glow's and trail's tint) and bigger (the
 *  glow's Scale_All) it plays. The glow also gets the level itself as the int user parameter InfusionLevel. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FInfusionLevelVisual
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion Glows", meta = (ClampMin = "0.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion Glows", meta = (ClampMin = "0.0"))
	float Size = 1.0f;

	/** Color with its RGB scaled by Intensity; alpha kept. */
	FLinearColor Brighten(const FLinearColor &Color) const
	{
		return FLinearColor(Color.R * Intensity, Color.G * Intensity, Color.B * Intensity, Color.A);
	}
};

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

	// ==================== WEAPON TRAILS (Crown 2026-11-24, WeaponTrail.h) ====================

	/** Every trailing weapon's swing trail (not bows, guns, shields). A ribbon between the user vectors TrailStart and
	 *  TrailEnd (local space), tinted by ElementColor. TODO: art. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Trails")
	TSoftObjectPtr<UNiagaraSystem> WeaponTrailVFX;

	/** The tint with no infusion, a Physical or a Raw one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Trails")
	FLinearColor PlainTrailColor = FLinearColor::White;

	/** A montage with no trail window starts its trail this long (seconds) before its first Impact. TODO: tune. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Trails", meta = (ClampMin = "0.0"))
	float TrailWindUpSeconds = 0.25f;

	// ==================== INFUSION GLOWS (Crown 2026-11-24) ====================

	/** Each element's default infusion glow, used when the infusion's display data has no VFXSystem of its own.
	 *  Tinted by ElementColor like any glow. TODO: art. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion Glows")
	TMap<ESpellElement, TSoftObjectPtr<UNiagaraSystem>> ElementInfusionGlowVFX;

	/** A Physical infusion's default glow: a source with no element (it deals None). TODO: art. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion Glows")
	TSoftObjectPtr<UNiagaraSystem> PhysicalInfusionGlowVFX;

	/** A Raw infusion's default glow (enhanced physical, paid in HP). TODO: art. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion Glows")
	TSoftObjectPtr<UNiagaraSystem> RawInfusionGlowVFX;

	/** An Ice infusion's default glow: a Water infusion whose hit deals Ice (Crown 2026-11-24; Ice is Water's damage
	 *  type, ED-04). TODO: art. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion Glows")
	TSoftObjectPtr<UNiagaraSystem> IceInfusionGlowVFX;

	/** Each infusion level's look, level 1 first; a level past the end uses the last row. Every glow (all elements,
	 *  Physical, Raw) and the weapon trail's tint use the same table. TODO: tune. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion Glows")
	TArray<FInfusionLevelVisual> InfusionLevelVisuals;

	UImpactVisualDefaults();

	/** The default glow slot: Raw's when bRaw, Ice's for a Water infusion when bIce, Physical's with no element (None
	 *  or Generic), else the element's row; empty when that slot has none. */
	TSoftObjectPtr<UNiagaraSystem> GetInfusionGlowVFX(ESpellElement Element, bool bRaw = false, bool bIce = false) const;

	/** Level's row in InfusionLevelVisuals; plain (x1) at level 0 or with no rows. */
	FInfusionLevelVisual GetInfusionLevelVisual(int32 Level) const;

	/** Level's look from the authored asset, else the built-in table. */
	static FInfusionLevelVisual ForInfusionLevel(int32 Level);
};
