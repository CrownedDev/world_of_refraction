// ElementImmunityMapSpec.cpp
// B-249 (DRY audit A9): one element -> elemental-immunity map (SkillEffectClassification::GetElementImmunityType),
// read by the item executor's resist-or-immunity items and by status buildup's immunity gate. Its GAS tag row
// (WoRTags::EffectTypeToTag) names the same element, so the three readers agree.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Tags/WoRGameplayTags.h"

namespace
{
	const FString ELEMENT_IMMUNITY_TAG_PREFIX = TEXT("Immunity.Element.");
	const FString ELEMENT_TAG_PREFIX = TEXT("Element.");
}

BEGIN_DEFINE_SPEC(FElementImmunityMapSpec, "WoR.Effects.ElementImmunityMap", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FElementImmunityMapSpec)

void FElementImmunityMapSpec::Define()
{
	It("gives each real element its own immunity, tagged with the same element", [this]()
	{
		TSet<ESkillEffectType> Seen;
		for (int32 Index = 0; Index < static_cast<int32>(ESpellElement::None); ++Index)
		{
			const ESpellElement Element = static_cast<ESpellElement>(Index);
			if (Element == ESpellElement::Generic)
			{
				continue;
			}
			const FString Name = UEnum::GetValueAsString(Element);
			const ESkillEffectType Immunity = SkillEffectClassification::GetElementImmunityType(Element);
			if (!TestNotEqual(Name + TEXT(" has an immunity"), Immunity, ESkillEffectType::None))
			{
				continue;
			}
			TestFalse(Name + TEXT(" immunity is its own"), Seen.Contains(Immunity));
			Seen.Add(Immunity);

			const FString ElementLeaf = WoRTags::ElementToTag(Element).ToString().RightChop(ELEMENT_TAG_PREFIX.Len());
			TestEqual(Name + TEXT(" immunity tag"), WoRTags::EffectTypeToTag(Immunity).ToString(), ELEMENT_IMMUNITY_TAG_PREFIX + ElementLeaf);
			TestTrue(Name + TEXT(" immunity is a buff"), SkillEffectClassification::IsBuff(Immunity, 0.0f));
		}
	});

	It("gives no immunity for Generic or None", [this]()
	{
		TestEqual(TEXT("Generic"), SkillEffectClassification::GetElementImmunityType(ESpellElement::Generic), ESkillEffectType::None);
		TestEqual(TEXT("None"), SkillEffectClassification::GetElementImmunityType(ESpellElement::None), ESkillEffectType::None);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
