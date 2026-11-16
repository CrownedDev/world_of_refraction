// ClassTableSpec.cpp
// B-549 (D-228, one-player stage 2): the class table on UClassDefaultLoadoutData. Filled from characters (today's
// Lords), a Generic or Resonator gives its class row and a Caster its element row; FindNumbers reads a class, or a
// Refractor's element and Broken Darkness, back.

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/CharacterData.h"
#include "Loadout/ClassDefaultLoadoutData.h"
#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"

namespace
{
	constexpr int32 CLASS_TABLE_TEST_GENERIC_BODY = 7;
	constexpr int32 CLASS_TABLE_TEST_GENERIC_SPEED = 40;
	constexpr int32 CLASS_TABLE_TEST_FIRE_LEVEL = 3;
	constexpr int32 CLASS_TABLE_TEST_DARKNESS_LEVEL = 1;

	UCharacterData *MakeSource(ECharacterClass Class, ESpellElement Element, int32 Mind, int32 Body, int32 Spirit, int32 ActionSpeed = 0, bool bBrokenDarkness = false)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = Class;
		Data->InnateElement = Element;
		Data->bBrokenDarknessInnate = bBrokenDarkness;
		Data->WorldMindLevel = Mind;
		Data->WorldBodyLevel = Body;
		Data->WorldSpiritLevel = Spirit;
		Data->ActionSpeed = ActionSpeed;
		return Data;
	}
}

BEGIN_DEFINE_SPEC(FClassTableSpec, "WoR.Loadout.ClassTable", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FClassTableSpec)

void FClassTableSpec::Define()
{
	It("fills a class row from a Generic and element rows from Refractors, and reads them back", [this]()
	{
		UClassDefaultLoadoutData *Table = NewObject<UClassDefaultLoadoutData>(GetTransientPackage());
		const int32 Written = Table->FillFrom({
			MakeSource(ECharacterClass::Generic, ESpellElement::Generic, 1, CLASS_TABLE_TEST_GENERIC_BODY, 1, CLASS_TABLE_TEST_GENERIC_SPEED),
			MakeSource(ECharacterClass::Caster, ESpellElement::Fire, CLASS_TABLE_TEST_FIRE_LEVEL, CLASS_TABLE_TEST_FIRE_LEVEL, CLASS_TABLE_TEST_FIRE_LEVEL),
			MakeSource(ECharacterClass::Caster, ESpellElement::Darkness, CLASS_TABLE_TEST_DARKNESS_LEVEL, CLASS_TABLE_TEST_DARKNESS_LEVEL, CLASS_TABLE_TEST_DARKNESS_LEVEL),
			MakeSource(ECharacterClass::Caster, ESpellElement::Darkness, CLASS_TABLE_TEST_DARKNESS_LEVEL, CLASS_TABLE_TEST_DARKNESS_LEVEL, CLASS_TABLE_TEST_DARKNESS_LEVEL, 0, true),
			nullptr});
		TestEqual(TEXT("four rows written, the empty source skipped"), Written, 4);

		FClassElementNumbers Numbers;
		int32 Speed = 0;
		TestTrue(TEXT("the Generic's row"), Table->FindNumbers(ECharacterClass::Generic, ESpellElement::Fire, false, Numbers, Speed));
		TestTrue(TEXT("its numbers, whatever element is asked"), Numbers.WorldBodyLevel == CLASS_TABLE_TEST_GENERIC_BODY && Speed == CLASS_TABLE_TEST_GENERIC_SPEED);

		TestTrue(TEXT("the Fire Refractor"), Table->FindNumbers(ECharacterClass::Caster, ESpellElement::Fire, false, Numbers, Speed));
		TestEqual(TEXT("its levels"), Numbers.WorldMindLevel, CLASS_TABLE_TEST_FIRE_LEVEL);
		TestTrue(TEXT("Broken Darkness is its own row"), Table->FindNumbers(ECharacterClass::Caster, ESpellElement::Darkness, true, Numbers, Speed) && Numbers.bBrokenDarkness);
		TestTrue(TEXT("and Darkness its own"), Table->FindNumbers(ECharacterClass::Caster, ESpellElement::Darkness, false, Numbers, Speed) && !Numbers.bBrokenDarkness);
		TestFalse(TEXT("no Water row: not found"), Table->FindNumbers(ECharacterClass::Caster, ESpellElement::Water, false, Numbers, Speed));
		TestFalse(TEXT("no Resonator row: not found"), Table->FindNumbers(ECharacterClass::Resonator, ESpellElement::Generic, false, Numbers, Speed));
		TestTrue(TEXT("the debug string lists the rows"), Table->GetClassTableString().Contains(TEXT("Broken Darkness")));
	});

	It("replaces an element row filled twice, rather than adding a second", [this]()
	{
		UClassDefaultLoadoutData *Table = NewObject<UClassDefaultLoadoutData>(GetTransientPackage());
		Table->FillFrom({MakeSource(ECharacterClass::Caster, ESpellElement::Fire, 1, 1, 1)});
		Table->FillFrom({MakeSource(ECharacterClass::Caster, ESpellElement::Fire, CLASS_TABLE_TEST_FIRE_LEVEL, 1, 1)});
		const FClassNumbers *Row = Table->Classes.Find(ECharacterClass::Caster);
		TestTrue(TEXT("one Fire row, the last one"), Row && Row->Elements.Num() == 1 && Row->Elements[0].WorldMindLevel == CLASS_TABLE_TEST_FIRE_LEVEL);
	});
}

#endif
