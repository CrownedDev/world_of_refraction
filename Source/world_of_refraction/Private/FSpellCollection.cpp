// FSpellCollection.cpp
// Spell inventory collection implementation

#include "FSpellCollection.h"
#include "SpellData.h"

TArray<USpellData*> FSpellCollection::GetSpellsByElement(ESpellElement Element) const
{
    TArray<USpellData*> Result;
    for (USpellData* Spell : LearnedSpells)
    {
        if (Spell && Spell->Element == Element)
        {
            Result.Add(Spell);
        }
    }
    return Result;
}

TArray<USpellData*> FSpellCollection::GetSpellsBySchool(ESpellSchool School) const
{
    TArray<USpellData*> Result;
    for (USpellData* Spell : LearnedSpells)
    {
        if (Spell && Spell->School == School)
        {
            Result.Add(Spell);
        }
    }
    return Result;
}

TArray<USpellData*> FSpellCollection::GetSpellsByElementAndSchool(ESpellElement Element, ESpellSchool School) const
{
    TArray<USpellData*> Result;
    for (USpellData* Spell : LearnedSpells)
    {
        if (Spell && Spell->Element == Element && Spell->School == School)
        {
            Result.Add(Spell);
        }
    }
    return Result;
}
