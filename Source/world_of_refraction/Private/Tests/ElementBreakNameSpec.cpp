// ElementBreakNameSpec.cpp
// B-248 (DRY audit A8): every element's status break and its name come from one row: the trigger the bar cap
// fires (BarCapTriggerResolver) shows the element's WoRGameNames::GetElementBreakName. Water's break shows
// "Frozen" (D-090); dropped repeats still show the shared table's words.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Skills/Effects/BarCapTriggerResolver.h"
#include "Skills/Effects/SkillEffectDisplayNames.h"
#include "WoRGameNames.h"

BEGIN_DEFINE_SPEC(FElementBreakNameSpec, "WoR.Effects.ElementBreakNames", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FElementBreakNameSpec)

void FElementBreakNameSpec::Define()
{
	It("names each element's break from its one row", [this]()
	{
		for (int32 Index = 0; Index < static_cast<int32>(ESpellElement::None); ++Index)
		{
			const ESpellElement Element = static_cast<ESpellElement>(Index);
			const FText BreakName = WoRGameNames::GetElementBreakName(Element);
			if (BreakName.IsEmpty())
			{
				continue;
			}
			const ESkillEffectType Trigger = BarCapTriggerResolver::ResolveTrigger(Element, EPhysicalDamageType::None, false);
			TestEqual(FString::Printf(TEXT("%s break"), *UEnum::GetValueAsString(Element)),
					  SkillEffectDisplayNames::GetDisplayName(Trigger, Element), BreakName.ToString());
		}
	});

	It("shows each element's break word (Water's is Frozen, D-090)", [this]()
	{
		const TPair<ESpellElement, const TCHAR *> ExpectedWords[] = {
			{ESpellElement::Fire, TEXT("Burn")},		  {ESpellElement::Water, TEXT("Frozen")},
			{ESpellElement::Earth, TEXT("Softened")},	  {ESpellElement::Wind, TEXT("Tripped")},
			{ESpellElement::Lightning, TEXT("Stunned")}, {ESpellElement::Light, TEXT("Dimmed")},
			{ESpellElement::Darkness, TEXT("Silenced")}, {ESpellElement::Void, TEXT("Voided")},
		};
		for (const TPair<ESpellElement, const TCHAR *> &Expected : ExpectedWords)
		{
			const ESkillEffectType Trigger = BarCapTriggerResolver::ResolveTrigger(Expected.Key, EPhysicalDamageType::None, false);
			TestEqual(UEnum::GetValueAsString(Expected.Key), SkillEffectDisplayNames::GetDisplayName(Trigger, Expected.Key),
					  FString(Expected.Value));
		}
		TestTrue(TEXT("Reality has no break word"), WoRGameNames::GetElementBreakName(ESpellElement::Reality).IsEmpty());
	});

	It("keeps the shared table's words for the dropped repeats", [this]()
	{
		for (const ESkillEffectType Type : {ESkillEffectType::HealingReceivedBuff, ESkillEffectType::GrantDOTImmunity, ESkillEffectType::Lifesteal})
		{
			TestEqual(UEnum::GetValueAsString(Type), SkillEffectDisplayNames::GetDisplayName(Type, ESpellElement::None),
					  WoRGameNames::GetSkillEffectTypeName(Type).ToString());
		}
	});
}

#endif
