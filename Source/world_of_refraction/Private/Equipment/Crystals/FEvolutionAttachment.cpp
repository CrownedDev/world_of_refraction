// FEvolutionAttachment.cpp
// Implementations for FEvolutionAttachment's durability helpers. Kept out of
// the header to avoid pulling EvolutionItemData.h into every transitive
// consumer of FRuntimeAttachedItem.

#include "Equipment/Crystals/FEvolutionAttachment.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/EBreakability.h"

bool FEvolutionAttachment::IsBroken() const
{
    // Option Y: any non-Unbreakable crystal worn to 0 is broken — including a
    // BDBreakable crystal worn down by Broken Darkness/Reality, so the
    // between-combat sweep clears the slot. Unbreakable never reaches 0 (the
    // ApplyWear gate blocks all wear), so it is never broken.
    return Item && Item->Breakability != EBreakability::Unbreakable && CurrentDurability <= 0;
}

bool FEvolutionAttachment::ApplyWear(int32 Amount, bool bForceWear)
{
    if (!Item || Amount <= 0 || CurrentDurability <= 0)
    {
        return false;
    }
    switch (Item->Breakability)
    {
    case EBreakability::Unbreakable:  return false;                          // never wears, even BD
    case EBreakability::Breakable:    break;                                 // wears for anyone
    case EBreakability::BDBreakable:  if (!bForceWear) return false; break;  // BD/Reality only
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
