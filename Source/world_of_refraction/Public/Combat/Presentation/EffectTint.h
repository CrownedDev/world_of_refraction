// EffectTint.h
// B-619 (D-273): the SoftTofu effect packs read one tint, User.Color_ (a vector), not our CoreColor / EdgeColor. The
// projectile and skill-VFX tint code sets it too, for a real element only: a plain (None) or inherit (Generic) effect
// keeps the pack's authored look, so the uninfused compressed air stays pale.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraComponent.h"
#include "Combat/Presentation/ParryVisuals.h"

namespace EffectTint
{
	/** The SoftTofu packs' tint parameter (User.Color_, a vector). */
	inline const FName SOFTTOFU_COLOR_PARAM(TEXT("Color_"));

	/** Only an element tints (D-273): not None, not Generic. */
	inline bool ShouldSetSoftTofuColour(ESpellElement Element)
	{
		return ParryVisuals::IsColouringElement(Element);
	}

	/** B-622 (D-275, Crown: "Yes, tint only"): the element a skill's VFX is tinted with. A weapon buff or debuff takes no
	 *  infusion, but its look takes the caster's infusion element (LookElement) when one is on; else the action's own. */
	inline ESpellElement ChooseTintElement(ESpellElement ActionElement, ESpellElement LookElement)
	{
		return ShouldSetSoftTofuColour(LookElement) ? LookElement : ActionElement;
	}

	inline void SetSoftTofuColour(UNiagaraComponent &Comp, const FLinearColor &Colour)
	{
		Comp.SetVectorParameter(SOFTTOFU_COLOR_PARAM, FVector(Colour.R, Colour.G, Colour.B));
	}
}
