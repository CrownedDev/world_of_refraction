// FSpellCollection.cpp
// Spell inventory collection implementation

#include "Loadout/Entries/FSpellCollection.h"
#include "Skills/Definitions/SpellData.h"

bool FSpellCollection::LearnSpell(USpellData *Spell)
{
    if (!Spell || !CanLearn() || HasSpell(Spell))
    {
        return false;
    }
    // Learn-time mint: identity originates at ownership (per the spell-instance arc). Seed Tier from
    // the asset (leveling mutates it later — deferred); Quality is the C placeholder until the
    // weighted-roll cluster. UNREAD by combat until cluster ii threads it through the equip chain.
    FSpellInstance Instance(Spell);
    Instance.Tier = Spell->Tier;
    Instance.Quality = EItemQuality::C_Quality;
    LearnedSpells.Add(Instance);
    return true;
}

TArray<USpellData*> FSpellCollection::GetSpellsByElement(ESpellElement Element) const
{
    TArray<USpellData*> Result;
    for (const FSpellInstance& Instance : LearnedSpells)
    {
        if (Instance.Spell && Instance.Spell->Element == Element)
        {
            Result.Add(Instance.Spell);
        }
    }
    return Result;
}

TArray<USpellData*> FSpellCollection::GetSpellsBySchool(ESpellSchool School) const
{
    TArray<USpellData*> Result;
    for (const FSpellInstance& Instance : LearnedSpells)
    {
        if (Instance.Spell && Instance.Spell->School == School)
        {
            Result.Add(Instance.Spell);
        }
    }
    return Result;
}

TArray<USpellData*> FSpellCollection::GetSpellsByElementAndSchool(ESpellElement Element, ESpellSchool School) const
{
    TArray<USpellData*> Result;
    for (const FSpellInstance& Instance : LearnedSpells)
    {
        if (Instance.Spell && Instance.Spell->Element == Element && Instance.Spell->School == School)
        {
            Result.Add(Instance.Spell);
        }
    }
    return Result;
}
