// RingData.cpp

#include "RingData.h"
#include "EvolutionItemData.h"
#include "LoadoutConstants.h"

int32 URingData::GetMaxSpells() const
{
    return LoadoutConstants::MAX_RING_SPELLS;
}

#if WITH_EDITOR
EDataValidationResult URingData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

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

    return Result;
}
#endif
