// FEvolutionAttachment.cpp
// Implementations for FEvolutionAttachment's durability helpers. Kept out of
// the header to avoid pulling EvolutionItemData.h into every transitive
// consumer of FRuntimeAttachedItem.

#include "FEvolutionAttachment.h"
#include "EvolutionItemData.h"

bool FEvolutionAttachment::IsBroken() const
{
    return Item && Item->bCanBreak && CurrentDurability <= 0;
}

bool FEvolutionAttachment::ApplyWear(int32 Amount)
{
    if (!Item || !Item->bCanBreak || Amount <= 0 || CurrentDurability <= 0)
    {
        return false;
    }

    const int32 Before = CurrentDurability;
    CurrentDurability = FMath::Max(0, CurrentDurability - Amount);

    return Before > 0 && CurrentDurability == 0;
}

int32 FEvolutionAttachment::RepairBetweenCombats(int32 Amount)
{
    if (!Item || Amount <= 0)
    {
        return 0;
    }

    const int32 MaxDur = Item->MaxDurability;
    const int32 Before = CurrentDurability;
    CurrentDurability = FMath::Min(MaxDur, CurrentDurability + Amount);

    return CurrentDurability - Before;
}
