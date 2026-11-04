// ResistanceColumnsSpec.cpp
// B-272 (S7): the four resistance column structs read, copy, add and compare by damage type, and every
// column lands in the field of the same name. B-358: ten columns, the one Physical column held in the three
// physical fields.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/CharacterData.h"
#include "Combat/Resistance/ClassInnateResistanceTable.h"
#include "Combat/Resistance/ResistanceColumns.h"
#include "Combat/Resistance/ResistanceProfileData.h"
#include "Equipment/FResistanceBonus.h"

namespace
{
	constexpr int32 RESISTANCE_COLUMN_COUNT = 10;
	constexpr float PHYSICAL_COLUMN_VALUE = 10.0f; // the last column's distinct value

	/** Each column a different value (its position + 1) so a crossed field shows. */
	FResistanceBonus DistinctColumns()
	{
		FResistanceBonus Bonus;
		float Value = 1.0f;
		for (const EWoRDamageType Type : ResistanceColumns::Types())
		{
			ResistanceColumns::Set(Bonus, Type, Value);
			Value += 1.0f;
		}
		return Bonus;
	}

	template <class TRow>
	bool NamedFieldsMatch(const TRow &R, const FResistanceBonus &B)
	{
		return R.Fire == B.Fire && R.Water == B.Water && R.Earth == B.Earth && R.Wind == B.Wind && R.Light == B.Light &&
			   R.Darkness == B.Darkness && R.Lightning == B.Lightning && R.Void == B.Void && R.Reality == B.Reality &&
			   R.Slash == B.Slash && R.Pierce == B.Pierce && R.Impact == B.Impact;
	}
}

BEGIN_DEFINE_SPEC(FResistanceColumnsSpec, "WoR.Resistance.Columns",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FResistanceColumnsSpec)

void FResistanceColumnsSpec::Define()
{
	It("has 10 columns: the nine elements and one Physical", [this]()
	{
		TestEqual(TEXT("column count"), ResistanceColumns::Types().Num(), RESISTANCE_COLUMN_COUNT);
		const FResistanceBonus Bonus = DistinctColumns();
		TestEqual(TEXT("Fire is first"), Bonus.Fire, 1.0f);
		TestEqual(TEXT("Reality is ninth"), Bonus.Reality, 9.0f);
		TestTrue(TEXT("Physical is last, held in all three physical fields"),
				 Bonus.Slash == PHYSICAL_COLUMN_VALUE && Bonus.Pierce == PHYSICAL_COLUMN_VALUE && Bonus.Impact == PHYSICAL_COLUMN_VALUE);
	});

	It("keeps the column order the gear roll's slots use (Fire..Reality, Physical)", [this]()
	{
		const EWoRDamageType Expected[] = {
			EWoRDamageType::Fire, EWoRDamageType::Water, EWoRDamageType::Earth, EWoRDamageType::Wind,
			EWoRDamageType::Light, EWoRDamageType::Darkness, EWoRDamageType::Lightning, EWoRDamageType::Void,
			EWoRDamageType::Reality, ResistanceColumns::PHYSICAL_COLUMN};
		TestEqual(TEXT("one slot per column"), ResistanceColumns::Types().Num(), static_cast<int32>(UE_ARRAY_COUNT(Expected)));
		for (int32 Slot = 0; Slot < FMath::Min(ResistanceColumns::Types().Num(), static_cast<int32>(UE_ARRAY_COUNT(Expected))); ++Slot)
		{
			TestTrue(FString::Printf(TEXT("slot %d"), Slot), ResistanceColumns::Types()[Slot] == Expected[Slot]);
		}
	});

	It("reads 0 for a type with no column (None, Ice)", [this]()
	{
		const FResistanceBonus Bonus = DistinctColumns();
		TestEqual(TEXT("None"), ResistanceColumns::Get(Bonus, EWoRDamageType::None), 0.0f);
		TestEqual(TEXT("Ice"), ResistanceColumns::Get(Bonus, EWoRDamageType::Ice), 0.0f);
	});

	It("copies every column into the field of the same name in each struct", [this]()
	{
		const FResistanceBonus Bonus = DistinctColumns();
		TestTrue(TEXT("table row"), NamedFieldsMatch(ResistanceColumns::Convert<ClassInnateResistanceTable::FResistanceRow>(Bonus), Bonus));
		TestTrue(TEXT("profile row"), NamedFieldsMatch(ResistanceColumns::Convert<FResistanceProfileRow>(Bonus), Bonus));
		TestTrue(TEXT("display"), NamedFieldsMatch(ResistanceColumns::Convert<FResistanceProfileDisplay>(Bonus), Bonus));
		const FResistanceProfileRow Profile = FResistanceProfileRow::FromTableRow(ResistanceColumns::Convert<ClassInnateResistanceTable::FResistanceRow>(Bonus));
		TestTrue(TEXT("profile row round trip"), NamedFieldsMatch(Profile.ToTableRow(), Bonus));
	});

	It("adds and compares column by column", [this]()
	{
		const FResistanceBonus Bonus = DistinctColumns();
		FResistanceBonus Sum = Bonus;
		Sum.Accumulate(Bonus);
		TestEqual(TEXT("Fire doubled"), Sum.Fire, 2.0f);
		TestEqual(TEXT("Physical doubled"), ResistanceColumns::Get(Sum, EWoRDamageType::Impact), PHYSICAL_COLUMN_VALUE * 2.0f);
		TestTrue(TEXT("equal to itself across structs"), ResistanceColumns::Equal(Bonus, ResistanceColumns::Convert<ClassInnateResistanceTable::FResistanceRow>(Bonus)));
		TestFalse(TEXT("a doubled one differs"), ResistanceColumns::Equal(Bonus, Sum));
	});

	It("reads the element and physical column of a table row by type", [this]()
	{
		const ClassInnateResistanceTable::FResistanceRow Row = ResistanceColumns::Convert<ClassInnateResistanceTable::FResistanceRow>(DistinctColumns());
		TestEqual(TEXT("Lightning"), ClassInnateResistanceTable::GetElementColumn(Row, ESpellElement::Lightning), 7.0f);
		TestEqual(TEXT("Generic adds nothing"), ClassInnateResistanceTable::GetElementColumn(Row, ESpellElement::Generic), 0.0f);
		TestEqual(TEXT("None adds nothing"), ClassInnateResistanceTable::GetElementColumn(Row, ESpellElement::None), 0.0f);
		TestEqual(TEXT("Pierce reads the Physical column"), ClassInnateResistanceTable::GetPhysicalColumn(Row, EPhysicalDamageType::Pierce), PHYSICAL_COLUMN_VALUE);
		TestEqual(TEXT("no physical"), ClassInnateResistanceTable::GetPhysicalColumn(Row, EPhysicalDamageType::None), 0.0f);
	});
}

#endif
