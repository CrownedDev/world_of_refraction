// KeyedEffectSpec.cpp
// B-275 (S9): the element-keyed members equal the old per-element ones: an authored keyed effect becomes the
// old member at build time and reads the same; an on-hit DoT of an element with no old member keeps its element.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Elements/ElementTable.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/EffectText.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Effects/FSkillEffectPayload.h"

namespace
{
	constexpr int32 KEYED_TEST_DURATION = 2;
	constexpr int32 KEYED_TEST_EFFECT_ID = 9275;

	FSkillEffectPayload KeyedPayload(ESkillEffectType Type, ESpellElement Element)
	{
		FSkillEffectPayload P;
		P.EffectType = Type;
		P.Element = Element;
		P.Duration = KEYED_TEST_DURATION;
		return P;
	}

	FActiveSkillEffect BuildFrom(const FSkillEffectPayload &P)
	{
		return FActiveSkillEffect::BuildRuntimeFromPayload(TEXT("Keyed"), KEYED_TEST_EFFECT_ID, P, {}, false, 1, false);
	}
}

BEGIN_DEFINE_SPEC(FKeyedEffectSpec, "WoR.Effects.Keyed",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FKeyedEffectSpec)

void FKeyedEffectSpec::Define()
{
	It("resolves an element immunity to the old member for every element", [this]()
	{
		for (const FElementRow &Row : ElementTable::Rows())
		{
			if (Row.Immunity == ESkillEffectType::None)
			{
				continue;
			}
			const FString What = UEnum::GetValueAsString(Row.Element);
			TestEqual(What, SkillEffectClassification::ResolveKeyed(ESkillEffectType::GrantElementImmunity, Row.Element), Row.Immunity);
			TestEqual(What + TEXT(" built"), BuildFrom(KeyedPayload(ESkillEffectType::GrantElementImmunity, Row.Element)).EffectType, Row.Immunity);
		}
		TestEqual(TEXT("no element: stays keyed"), SkillEffectClassification::ResolveKeyed(ESkillEffectType::GrantElementImmunity, ESpellElement::None),
				  ESkillEffectType::GrantElementImmunity);
	});

	It("resolves Fire and Water on-hit DoTs to Burn and Chill, and keeps another element keyed", [this]()
	{
		TestEqual(TEXT("Fire"), BuildFrom(KeyedPayload(ESkillEffectType::ApplyDOTOnHit, ESpellElement::Fire)).EffectType, ESkillEffectType::ApplyBurnToTarget);
		TestEqual(TEXT("Water"), BuildFrom(KeyedPayload(ESkillEffectType::ApplyDOTOnHit, ESpellElement::Water)).EffectType, ESkillEffectType::ApplyChillToTarget);
		const FActiveSkillEffect Earth = BuildFrom(KeyedPayload(ESkillEffectType::ApplyDOTOnHit, ESpellElement::Earth));
		TestEqual(TEXT("Earth stays keyed"), Earth.EffectType, ESkillEffectType::ApplyDOTOnHit);
		TestEqual(TEXT("with its element"), Earth.Element, ESpellElement::Earth);
		TestEqual(TEXT("a plain type is untouched"), SkillEffectClassification::ResolveKeyed(ESkillEffectType::Stun, ESpellElement::Fire), ESkillEffectType::Stun);
	});

	It("reads the same text as the old member", [this]()
	{
		TestEqual(TEXT("Fire immunity"), EffectText::DescribePayload(KeyedPayload(ESkillEffectType::GrantElementImmunity, ESpellElement::Fire)).ToString(),
				  EffectText::DescribePayload(KeyedPayload(ESkillEffectType::GrantFireImmunity, ESpellElement::Fire)).ToString());
		TestEqual(TEXT("Burn on hit"), EffectText::DescribePayload(KeyedPayload(ESkillEffectType::ApplyDOTOnHit, ESpellElement::Fire)).ToString(),
				  EffectText::DescribePayload(KeyedPayload(ESkillEffectType::ApplyBurnToTarget, ESpellElement::Fire)).ToString());
		TestFalse(TEXT("an Earth DoT on hit has a sentence"),
				  EffectText::DescribePayload(KeyedPayload(ESkillEffectType::ApplyDOTOnHit, ESpellElement::Earth)).IsEmpty());
	});

	It("classifies the keyed members like the old ones", [this]()
	{
		TestTrue(TEXT("immunity is a buff"), SkillEffectClassification::IsBuff(ESkillEffectType::GrantElementImmunity, 0.0f));
		TestTrue(TEXT("DoT on hit is a debuff"), SkillEffectClassification::IsDebuff(ESkillEffectType::ApplyDOTOnHit, 0.0f));
	});
}

#endif
