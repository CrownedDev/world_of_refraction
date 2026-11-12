// ElementBreakNameSpec.cpp
// B-248 (DRY audit A8): every element's status break and its name come from one row: the trigger the bar cap
// fires (BarCapTriggerResolver) shows the element's WoRGameNames::GetElementBreakName. Q-064a2 (Crown, 2026-11-12):
// every break is "<Element> Break" and every damage over time "<Element> DoT", no special names; a plain DoT of
// an element doesn't borrow its break's name. Dropped repeats still show the shared table's words.

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
					  SkillEffectDisplayNames::GetBreakDisplayName(Trigger, Element), BreakName.ToString());
		}
	});

	It("names every break \"<Element> Break\" and every DoT \"<Element> DoT\" (Q-064a2, Crown 2026-11-12)", [this]()
	{
		const TPair<ESpellElement, const TCHAR *> ExpectedBreaks[] = {
			{ESpellElement::Fire, TEXT("Fire Break")},		   {ESpellElement::Water, TEXT("Water Break")},
			{ESpellElement::Earth, TEXT("Earth Break")},	   {ESpellElement::Wind, TEXT("Wind Break")},
			{ESpellElement::Lightning, TEXT("Lightning Break")}, {ESpellElement::Light, TEXT("Light Break")},
			{ESpellElement::Darkness, TEXT("Darkness Break")}, {ESpellElement::Void, TEXT("Void Break")},
		};
		for (const TPair<ESpellElement, const TCHAR *> &Expected : ExpectedBreaks)
		{
			const ESkillEffectType Trigger = BarCapTriggerResolver::ResolveTrigger(Expected.Key, EPhysicalDamageType::None, false);
			TestEqual(UEnum::GetValueAsString(Expected.Key), SkillEffectDisplayNames::GetBreakDisplayName(Trigger, Expected.Key),
					  FString(Expected.Value));
			const FString DoTName = SkillEffectDisplayNames::GetDisplayName(ESkillEffectType::DOT, Expected.Key);
			TestEqual(UEnum::GetValueAsString(Expected.Key) + TEXT(" DoT"), DoTName,
					  WoRGameNames::GetElementName(Expected.Key).ToString() + TEXT(" DoT"));
		}
		TestEqual(TEXT("Ice"), WoRGameNames::GetSkillEffectTypeName(ESkillEffectType::IceBreak).ToString(), FString(TEXT("Ice Break")));
		TestEqual(TEXT("Ice on a Water hit"), SkillEffectDisplayNames::GetBreakDisplayName(ESkillEffectType::IceBreak, ESpellElement::Water),
				  FString(TEXT("Ice Break")));
		TestTrue(TEXT("Reality has no break word"), WoRGameNames::GetElementBreakName(ESpellElement::Reality).IsEmpty());
		TestTrue(TEXT("Reality has no DoT word"), WoRGameNames::GetElementDoTName(ESpellElement::Reality).IsEmpty());
	});

	It("drops the special names (Q-064a2): no Frozen, no Frost Bite, and a DoT isn't named as a break", [this]()
	{
		for (int32 Index = 0; Index < static_cast<int32>(ESpellElement::None); ++Index)
		{
			const ESpellElement Element = static_cast<ESpellElement>(Index);
			for (const ESkillEffectType Type : {ESkillEffectType::DOT, ESkillEffectType::SpeedDebuff, ESkillEffectType::SkipTurn})
			{
				const FString Name = SkillEffectDisplayNames::GetDisplayName(Type, Element);
				TestFalse(Name, Name.Contains(TEXT("Frozen")) || Name.Contains(TEXT("Frost")));
			}
		}
		// Fire's break fires a DOT: the break path says "Fire Break", a plain Fire DoT "Fire DoT".
		TestNotEqual(TEXT("a Fire DoT is not its break"), SkillEffectDisplayNames::GetDisplayName(ESkillEffectType::DOT, ESpellElement::Fire),
					 WoRGameNames::GetElementBreakName(ESpellElement::Fire).ToString());
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
