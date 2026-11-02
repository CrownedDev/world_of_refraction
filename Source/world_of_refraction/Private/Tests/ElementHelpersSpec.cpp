// ElementHelpersSpec.cpp
// B-276 (SimplifyAudit S10): one helper per element question. IsConcrete is the nine real elements, IsInherit is
// Generic, and UCharacterData::IsRealityRefractor is a Caster with a Reality innate. A non-Caster's hidden,
// stale InnateElement never counts.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/CharacterData.h"
#include "Skills/Definitions/ElementHelpers.h"

namespace
{
	UCharacterData *NewElementTestCharacter(ECharacterClass Class, ESpellElement Innate)
	{
		UCharacterData *Data = NewObject<UCharacterData>(GetTransientPackage());
		Data->CharacterClass = Class;
		Data->InnateElement = Innate;
		return Data;
	}
}

BEGIN_DEFINE_SPEC(FElementHelpersSpec, "WoR.Elements.Helpers", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FElementHelpersSpec)

void FElementHelpersSpec::Define()
{
	It("calls the nine real elements concrete, and Generic the inherit", [this]()
	{
		for (int32 Index = 0; Index <= static_cast<int32>(ESpellElement::None); ++Index)
		{
			const ESpellElement Element = static_cast<ESpellElement>(Index);
			const bool bExpectedConcrete = Element != ESpellElement::Generic && Element != ESpellElement::None;
			TestEqual(UEnum::GetValueAsString(Element) + TEXT(" concrete"), ElementHelpers::IsConcrete(Element), bExpectedConcrete);
			TestEqual(UEnum::GetValueAsString(Element) + TEXT(" inherit"), ElementHelpers::IsInherit(Element),
					  Element == ESpellElement::Generic);
		}
	});

	It("names only a Reality Caster the Reality Refractor", [this]()
	{
		TestTrue(TEXT("Reality Caster"), NewElementTestCharacter(ECharacterClass::Caster, ESpellElement::Reality)->IsRealityRefractor());
		TestFalse(TEXT("Fire Caster"), NewElementTestCharacter(ECharacterClass::Caster, ESpellElement::Fire)->IsRealityRefractor());
		TestFalse(TEXT("Resonator with a stale Reality innate"),
				  NewElementTestCharacter(ECharacterClass::Resonator, ESpellElement::Reality)->IsRealityRefractor());
		TestEqual(TEXT("a Generic character's element"),
				  NewElementTestCharacter(ECharacterClass::Generic, ESpellElement::Reality)->GetElement(), ESpellElement::Generic);
	});
}

#endif
