// ElementColors.h
// Color constants for each element

#pragma once

#include "CoreMinimal.h"
#include "RefractionElement.h"

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
    inline const FLinearColor Darkness = FLinearColor(0.1f, 0.1f, 0.1f);       // Black
    inline const FLinearColor Generic = FLinearColor(0.6f, 0.4f, 0.2f);        // Brown
    inline const FLinearColor BrokenDarkness = FLinearColor(0.1f, 0.1f, 0.1f); // Black (base)

    // Get color for element
    inline FLinearColor GetColorForElement(ERefractionElement Element)
    {
        switch (Element)
        {
        case ERefractionElement::Fire:
            return Fire;
        case ERefractionElement::Lightning:
            return Lightning;
        case ERefractionElement::Earth:
            return Earth;
        case ERefractionElement::Wind:
            return Wind;
        case ERefractionElement::Water:
            return Water;
        case ERefractionElement::Reality:
            return Reality;
        case ERefractionElement::Void:
            return Void;
        case ERefractionElement::Light:
            return Light;
        case ERefractionElement::Darkness:
            return Darkness;
        case ERefractionElement::Generic:
            return Generic;
        case ERefractionElement::BrokenDarkness:
            return BrokenDarkness;
        default:
            return Generic;
        }
    }

    // For BrokenDarkness: blend black with absorbed element
    inline FLinearColor GetBrokenDarknessColor(ERefractionElement AbsorbedElement)
    {
        // No absorption = pure black
        if (AbsorbedElement == ERefractionElement::BrokenDarkness)
        {
            return BrokenDarkness;
        }

        FLinearColor Absorbed = GetColorForElement(AbsorbedElement);

        // 50/50 blend with black
        return FLinearColor(
            BrokenDarkness.R + (Absorbed.R * 0.5f),
            BrokenDarkness.G + (Absorbed.G * 0.5f),
            BrokenDarkness.B + (Absorbed.B * 0.5f),
            1.0f);
    }
}