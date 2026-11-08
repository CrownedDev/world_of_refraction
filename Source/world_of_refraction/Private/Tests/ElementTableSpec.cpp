// ElementTableSpec.cpp
// B-271: the element table agrees with every old per-element function (wor.Elements.Compare), so the
// old functions can forward to it with no change in behaviour.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Elements/ElementTable.h"
#include "Debug/Combat/ElementTableDebug.h"
#include "Currency/CurrencyTypes.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Skills/Effects/ESkillEffectType.h"

BEGIN_DEFINE_SPEC(FElementTableSpec, "WoR.Elements.Compare",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FElementTableSpec)

void FElementTableSpec::Define()
{
	It("agrees with every old per-element function", [this]()
	{
		const TArray<FString> Mismatches = UElementTableDebug::CompareElements();
		TestEqual(FString::Printf(TEXT("mismatches: %s"), *FString::Join(Mismatches, TEXT(", "))), Mismatches.Num(), 0);
	});

	It("pins the rows the old switches held (they now forward here)", [this]()
	{
		const FElementRow &Fire = ElementTable::For(EWoRDamageType::Fire);
		TestEqual(TEXT("Fire gem"), Fire.Gem, ECrystalType::Garnet);
		TestEqual(TEXT("Fire essence"), Fire.Essence, EEssenceType::Fire);
		TestEqual(TEXT("Fire break"), Fire.BreakTrigger, ESkillEffectType::DOT);
		TestEqual(TEXT("Fire immunity"), Fire.Immunity, ESkillEffectType::GrantFireImmunity);
		const FElementRow &None = ElementTable::For(EWoRDamageType::None);
		TestEqual(TEXT("None gem"), None.Gem, ECrystalType::Quartz);
		TestEqual(TEXT("None essence"), None.Essence, EEssenceType::Generic);
		TestEqual(TEXT("Slash break"), ElementTable::For(EWoRDamageType::Slash).BreakTrigger, ESkillEffectType::DOT);
		TestEqual(TEXT("Reality absorbable"), ElementTable::For(EWoRDamageType::Reality).bBDAbsorbable, false);
		TestEqual(TEXT("Void forbidden"), ElementTable::For(EWoRDamageType::Void).bBDForbidden, true);
		TestEqual(TEXT("Ice element"), ElementTable::For(EWoRDamageType::Ice).Element, ESpellElement::None);
	});

	It("reads the None row for Generic, None and no physical type", [this]()
	{
		TestEqual(TEXT("Generic"), ElementTable::ForElement(ESpellElement::Generic).Type, EWoRDamageType::None);
		TestEqual(TEXT("None"), ElementTable::ForElement(ESpellElement::None).Type, EWoRDamageType::None);
		TestEqual(TEXT("no physical"), ElementTable::ForPhysical(EPhysicalDamageType::None).Type, EWoRDamageType::None);
	});

	It("has Water also resisted by Ice and nothing else", [this]()
	{
		for (const FElementRow &Row : ElementTable::Rows())
		{
			TestEqual(*FString::Printf(TEXT("row %d"), static_cast<int32>(Row.Type)), Row.AlsoResistedBy,
					  Row.Type == EWoRDamageType::Water ? EWoRDamageType::Ice : EWoRDamageType::None);
		}
	});
}

#endif
