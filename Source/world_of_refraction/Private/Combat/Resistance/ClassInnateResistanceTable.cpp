// ClassInnateResistanceTable.cpp
// Out-of-line actor-level query for the class / innate-element resistance table.
// Kept out of the header so the table stays a light enum-only include; this TU
// is the only one that reaches into UCharacterDataComponent.

#include "Combat/Resistance/ClassInnateResistanceTable.h"

#include "GameFramework/Actor.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"

namespace ClassInnateResistanceTable
{
	float GetClassInnateResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType PhysicalType)
	{
		if (!Target)
		{
			return 0.0f;
		}

		const UCharacterDataComponent *Comp = Target->FindComponentByClass<UCharacterDataComponent>();
		if (!Comp || !Comp->CharacterData)
		{
			return 0.0f;
		}

		const UCharacterData *Data = Comp->CharacterData;

		// Row selection: BD (runtime OR character-created, via IsBrokenDarkness) >
		// Generic > Resonator > Caster-by-innate-element.
		const FResistanceRow Row = ResolveRow(Data->CharacterClass, Data->GetElement(), Comp->IsBrokenDarkness());

		// Both axes compose: incoming element (BD-aliased) + incoming physical type.
		const float ElementTerm = GetElementColumn(Row, Element);
		const float PhysicalTerm = GetPhysicalColumn(Row, PhysicalType);

		return (ElementTerm + PhysicalTerm) / RESISTANCE_PERCENT_DIVISOR;
	}
}
