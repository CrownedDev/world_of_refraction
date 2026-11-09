// ClassInnateResistanceTable.cpp
// Out-of-line actor-level query for the class / innate-element resistance table.
// Kept out of the header so the table stays a light enum-only include; this TU
// is the only one that reaches into UCharacterDataComponent.

#include "Combat/Resistance/ClassInnateResistanceTable.h"

#include "GameFramework/Actor.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Combat/Resistance/ResistanceProfileData.h"

namespace
{
	/** Target's resistance profile row; false without CharacterData. */
	bool ResolveClassInnateRow(const AActor *Target, ClassInnateResistanceTable::FResistanceRow &OutRow)
	{
		const UCharacterDataComponent *Comp = Target ? Target->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!Comp || !Comp->CharacterData)
		{
			return false;
		}

		const UCharacterData *Data = Comp->CharacterData;

		// Row selection: BD (runtime OR character-created, via IsBrokenDarkness) >
		// Generic > Resonator > Caster-by-innate-element. The data asset when one exists (B-216),
		// else this table.
		const UResistanceProfileData *Profiles = UResistanceProfileData::GetActive();
		OutRow = Profiles ? Profiles->ResolveRow(Data->CharacterClass, Data->GetElement(), Comp->IsBrokenDarkness())
						  : ClassInnateResistanceTable::ResolveRow(Data->CharacterClass, Data->GetElement(), Comp->IsBrokenDarkness());
		return true;
	}
}

namespace ClassInnateResistanceTable
{
	float GetClassInnateResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType PhysicalType)
	{
		FResistanceRow Row{};
		if (!ResolveClassInnateRow(Target, Row))
		{
			return 0.0f;
		}

		// Both axes compose: incoming element (BD-aliased) + incoming physical type.
		const float ElementTerm = GetElementColumn(Row, Element);
		const float PhysicalTerm = GetPhysicalColumn(Row, PhysicalType);

		return (ElementTerm + PhysicalTerm) / RESISTANCE_PERCENT_DIVISOR;
	}

	float GetClassIceResistance(AActor *Target)
	{
		FResistanceRow Row{};
		return ResolveClassInnateRow(Target, Row) ? Row.Ice / RESISTANCE_PERCENT_DIVISOR : 0.0f;
	}
}
