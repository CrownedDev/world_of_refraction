// RingData.cpp

#include "RingData.h"
#include "SpellData.h"
#include "ItemData.h"
#include "CrystalType.h"
#include "LoadoutConstants.h"

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
        Context.AddWarning(FText::FromString(TEXT("Ring must have a unique name")));
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

        // Ring tier is informational only; warn on SlottedCrystal tier mismatch.
        if (SlottedCrystal->Tier != Tier)
        {
            Context.AddWarning(FText::FromString(FString::Printf(
                TEXT("Ring tier (%s) does not match SlottedCrystal tier (%s) — ring tier is informational only"),
                *UEnum::GetValueAsString(Tier),
                *UEnum::GetValueAsString(SlottedCrystal->Tier))));
        }
    }

    // Spell-count cap (warn, not error — designers may iterate over the cap)
    if (DefaultSpells.Num() > LoadoutConstants::MAX_RING_SPELLS)
    {
        Context.AddWarning(FText::FromString(FString::Printf(
            TEXT("DefaultSpells (%d) exceeds MAX_RING_SPELLS (%d)"),
            DefaultSpells.Num(), LoadoutConstants::MAX_RING_SPELLS)));
    }

    return Result;
}
#endif