// ClassColors.h
// Class-identity colour resolution — the character-level companion to
// ElementColors. Casters resolve through their innate element (reusing the
// element table, never duplicating it); non-Casters get their class colour.
//
// BrokenDarkness note: BD is InnateElement=Darkness + bBrokenDarknessInnate,
// so it resolves to the Darkness black here. A BD-specific colour is banked
// with the wider BD-for-every-caster arc.

#pragma once

#include "Character/CharacterData.h"
#include "CoreMinimal.h"
#include "Infusion/ElementColors.h"

namespace ClassColors
{
    // Class colors (Casters reuse ElementColors — only genuinely new values here)
    inline const FLinearColor ResonatorSilver = FLinearColor(0.75f, 0.78f, 0.82f); // Steel silver

    // Get color for a character's class identity
    inline FLinearColor GetColorForCharacter(const UCharacterData *Data)
    {
        if (!Data)
        {
            return FLinearColor::White;
        }

        switch (Data->CharacterClass)
        {
        case ECharacterClass::Caster:
            return ElementColors::GetColorForElement(Data->InnateElement);
        case ECharacterClass::Resonator:
            return ResonatorSilver;
        case ECharacterClass::Generic:
        default:
            // The existing neutral brown (trial-door tint) — one brown, one source.
            return ElementColors::GetColorForElement(ESpellElement::Generic);
        }
    }
}
