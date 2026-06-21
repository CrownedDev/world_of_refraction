// ElementColors.h
// Color constants for each element

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/ESpellElement.h"

namespace ElementColors
{
    // Element colors
    inline const FLinearColor Fire = FLinearColor(1.0f, 0.0f, 0.0f);           // Red
    inline const FLinearColor Lightning = FLinearColor(1.0f, 0.5f, 0.0f);      // Orange
    inline const FLinearColor Earth = FLinearColor(1.0f, 1.0f, 0.0f);          // Yellow
    inline const FLinearColor Wind = FLinearColor(0.0f, 1.0f, 0.0f);           // Green
    inline const FLinearColor Water = FLinearColor(0.0f, 0.5f, 1.0f);          // Blue
    inline const FLinearColor Reality = FLinearColor(0.3f, 0.0f, 0.5f);        // Indigo
    inline const FLinearColor Void = FLinearColor(0.6f, 0.0f, 1.0f);           // Violet
    inline const FLinearColor Light = FLinearColor(1.0f, 1.0f, 1.0f);          // White
    inline const FLinearColor Darkness = FLinearColor(0.02f, 0.02f, 0.02f);    // Near-black — THE Darkness/BD black
    inline const FLinearColor Generic = FLinearColor(0.6f, 0.4f, 0.2f);        // Brown
    inline const FLinearColor BrokenDarkness = Darkness;                       // BD IS Darkness — one black, one source

    // Get color for element
    inline FLinearColor GetColorForElement(ESpellElement Element)
    {
        switch (Element)
        {
        case ESpellElement::Fire:
            return Fire;
        case ESpellElement::Lightning:
            return Lightning;
        case ESpellElement::Earth:
            return Earth;
        case ESpellElement::Wind:
            return Wind;
        case ESpellElement::Water:
            return Water;
        case ESpellElement::Reality:
            return Reality;
        case ESpellElement::Void:
            return Void;
        case ESpellElement::Light:
            return Light;
        case ESpellElement::Darkness:
            return Darkness;
        case ESpellElement::Generic:
        case ESpellElement::None:
            return Generic; // None / Generic both paint the neutral (non-elemental) colour
        default:
            return Generic;
        }
    }

}