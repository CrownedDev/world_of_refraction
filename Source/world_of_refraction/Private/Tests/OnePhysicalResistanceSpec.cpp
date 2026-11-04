// OnePhysicalResistanceSpec.cpp
// B-358 (D-148, Q-358): one Physical resistance column. Slash, Pierce and Impact all read the same value (old
// separate values fold in as their average), a write sets the one value, and each subtype still picks its own
// break (SB-19 to SB-21).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Resistance/ClassInnateResistanceTable.h"
#include "Combat/Resistance/ResistanceColumns.h"
#include "Equipment/FResistanceBonus.h"
#include "Skills/Effects/BarCapTriggerResolver.h"

namespace
{
	constexpr float ONE_PHYSICAL_OLD_SLASH = 15.0f;
	constexpr float ONE_PHYSICAL_OLD_PIERCE = 0.0f;
	constexpr float ONE_PHYSICAL_OLD_IMPACT = -6.0f;
	constexpr float ONE_PHYSICAL_FOLDED = (ONE_PHYSICAL_OLD_SLASH + ONE_PHYSICAL_OLD_PIERCE + ONE_PHYSICAL_OLD_IMPACT) / 3.0f;
	constexpr float ONE_PHYSICAL_NEW_VALUE = 9.0f;
	constexpr float ONE_PHYSICAL_TOLERANCE = 0.001f;

	/** An old-shape piece: three different physical values, as saves and assets from before B-358 hold them. */
	FResistanceBonus OldShapeBonus()
	{
		FResistanceBonus Bonus;
		Bonus.Slash = ONE_PHYSICAL_OLD_SLASH;
		Bonus.Pierce = ONE_PHYSICAL_OLD_PIERCE;
		Bonus.Impact = ONE_PHYSICAL_OLD_IMPACT;
		return Bonus;
	}
}

BEGIN_DEFINE_SPEC(FOnePhysicalResistanceSpec, "WoR.Resistance.OnePhysical",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FOnePhysicalResistanceSpec)

void FOnePhysicalResistanceSpec::Define()
{
	It("gives every physical subtype the one Physical resistance, old values folded in as their average", [this]()
	{
		const FResistanceBonus Bonus = OldShapeBonus();
		for (const EWoRDamageType Type : {EWoRDamageType::Slash, EWoRDamageType::Pierce, EWoRDamageType::Impact})
		{
			TestEqual(FString::Printf(TEXT("%s reads the folded value"), *UEnum::GetValueAsString(Type)),
					  ResistanceColumns::Get(Bonus, Type), ONE_PHYSICAL_FOLDED, ONE_PHYSICAL_TOLERANCE);
		}
		const ClassInnateResistanceTable::FResistanceRow Row = ResistanceColumns::Convert<ClassInnateResistanceTable::FResistanceRow>(Bonus);
		TestEqual(TEXT("a class row: Slash"), ClassInnateResistanceTable::GetPhysicalColumn(Row, EPhysicalDamageType::Slash), ONE_PHYSICAL_FOLDED, ONE_PHYSICAL_TOLERANCE);
		TestEqual(TEXT("a class row: Impact"), ClassInnateResistanceTable::GetPhysicalColumn(Row, EPhysicalDamageType::Impact), ONE_PHYSICAL_FOLDED, ONE_PHYSICAL_TOLERANCE);
	});

	It("writes the one value into all three stored fields", [this]()
	{
		FResistanceBonus Bonus = OldShapeBonus();
		ResistanceColumns::Set(Bonus, EWoRDamageType::Pierce, ONE_PHYSICAL_NEW_VALUE);
		TestTrue(TEXT("all three fields hold it"),
				 Bonus.Slash == ONE_PHYSICAL_NEW_VALUE && Bonus.Pierce == ONE_PHYSICAL_NEW_VALUE && Bonus.Impact == ONE_PHYSICAL_NEW_VALUE);
		TestEqual(TEXT("and Slash reads it"), ResistanceColumns::Get(Bonus, EWoRDamageType::Slash), ONE_PHYSICAL_NEW_VALUE, ONE_PHYSICAL_TOLERANCE);
	});

	It("still lets each subtype pick its own break (SB-19 to SB-21)", [this]()
	{
		const ESkillEffectType SlashBreak = BarCapTriggerResolver::ResolveTrigger(ESpellElement::None, EPhysicalDamageType::Slash, false);
		const ESkillEffectType PierceBreak = BarCapTriggerResolver::ResolveTrigger(ESpellElement::None, EPhysicalDamageType::Pierce, false);
		const ESkillEffectType ImpactBreak = BarCapTriggerResolver::ResolveTrigger(ESpellElement::None, EPhysicalDamageType::Impact, false);
		TestTrue(TEXT("each subtype has a break"),
				 SlashBreak != ESkillEffectType::None && PierceBreak != ESkillEffectType::None && ImpactBreak != ESkillEffectType::None);
		TestTrue(TEXT("three different breaks"), SlashBreak != PierceBreak && PierceBreak != ImpactBreak && SlashBreak != ImpactBreak);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
