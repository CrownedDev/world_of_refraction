// RingData.cpp

#include "RingData.h"
#include "SpellData.h"
#include "ItemData.h"
#include "CrystalType.h"

bool URingData::IsEvolved() const
{
    return SlottedCrystal && SlottedCrystal->bIsEvolutionCrystal;
}

ESpellElement URingData::GetRingElement() const
{
    if (SlottedCrystal)
    {
        return SlottedCrystal->GetAssociatedElement();
    }
    return ESpellElement::Generic;
}

#if WITH_EDITOR
EDataValidationResult URingData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Name validation
    if (Name.IsEmpty() || Name == TEXT("Unnamed Ring"))
    {
        Context.AddError(FText::FromString(TEXT("Ring must have a unique name")));
        Result = EDataValidationResult::Invalid;
    }

    // Crystal validation
    if (!SlottedCrystal)
    {
        Context.AddWarning(FText::FromString(TEXT("Ring has no crystal - cannot be used in combat")));
    }
    else
    {
        if (!SlottedCrystal->bIsRefined)
        {
            Context.AddWarning(FText::FromString(TEXT("Slotted crystal is not refined")));
        }

        // Evolved ring validation
        if (IsEvolved() && !SlottedCrystal->GrantsEvolution())
        {
            Context.AddError(FText::FromString(TEXT("Evolution crystal has no Evolution assigned")));
            Result = EDataValidationResult::Invalid;
        }
    }

    return Result;
}
#endif