// EffectTintSpec.cpp
// B-619 (D-273): the SoftTofu packs' Color_ is set for a real element only (the plain look stays), as a vector.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Presentation/EffectTint.h"
#include "NiagaraComponent.h"
#include "NiagaraTypes.h"

BEGIN_DEFINE_SPEC(FEffectTintSpec, "WoR.Presentation.EffectTint",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FEffectTintSpec)

void FEffectTintSpec::Define()
{
	It("tints Color_ for an element, not for a plain or inherit effect", [this]()
	{
		TestTrue(TEXT("Fire tints"), EffectTint::ShouldSetSoftTofuColour(ESpellElement::Fire));
		TestTrue(TEXT("Wind tints"), EffectTint::ShouldSetSoftTofuColour(ESpellElement::Wind));
		TestFalse(TEXT("None (plain) keeps the pack's look"), EffectTint::ShouldSetSoftTofuColour(ESpellElement::None));
		TestFalse(TEXT("Generic (inherit) keeps the pack's look"), EffectTint::ShouldSetSoftTofuColour(ESpellElement::Generic));
	});

	It("writes the colour to the User.Color_ vector", [this]()
	{
		UNiagaraComponent *Comp = NewObject<UNiagaraComponent>(GetTransientPackage());
		const FLinearColor Colour(0.25f, 0.5f, 0.75f);
		EffectTint::SetSoftTofuColour(*Comp, Colour);
		const FNiagaraVariable Var(FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.Color_"));
		if (TestTrue(TEXT("the vector parameter is set"), Comp->GetOverrideParameters().IndexOf(Var) != INDEX_NONE))
		{
			const FVector3f Value = Comp->GetOverrideParameters().GetParameterValue<FVector3f>(Var);
			TestEqual(TEXT("its value is the colour"), Value, FVector3f(Colour.R, Colour.G, Colour.B));
		}
	});
}

#endif
