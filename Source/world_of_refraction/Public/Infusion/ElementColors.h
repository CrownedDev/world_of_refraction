// ElementColors.h
// Color constants for each element. B-294 (D-106): the tuned palette from analysis/ElementPalette_2026-10-29.md
// section 3a. Each element keeps its identity and the lightness is spread so no two of the nine elements fall under
// CIEDE2000 10 in normal, protan, deutan or tritan vision (WoR.Elements.Palette). Darkness stays a near-black grey
// (Crown) and needs a pale outline where it is text.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Combat/Elements/ElementTable.h"

namespace ElementColors
{
    // Element colors (linear; sRGB hex in the comment)
    inline const FLinearColor Fire = FLinearColor(0.434f, 0.023f, 0.009f);      // #B02A18 deep red, the darkest warm colour
    inline const FLinearColor Lightning = FLinearColor(1.0f, 0.604f, 0.0f);    // #FFCC00 amber-yellow, the lightest warm colour
    inline const FLinearColor Earth = FLinearColor(0.631f, 0.188f, 0.027f);     // #D0782E ochre
    inline const FLinearColor Wind = FLinearColor(0.027f, 0.423f, 0.287f);      // #2EAE92 teal-green
    inline const FLinearColor Water = FLinearColor(0.023f, 0.188f, 0.745f);     // #2A78E0 mid blue
    inline const FLinearColor Ice = FLinearColor(0.552f, 0.880f, 1.0f);         // #C4F1FF pale cyan-white (Water's sub-element)
    inline const FLinearColor Reality = FLinearColor(0.102f, 0.027f, 0.658f);   // #5A2ED4 indigo, dark (D-024)
    inline const FLinearColor Void = FLinearColor(0.552f, 0.262f, 1.0f);        // #C48CFF violet, light (D-024)
    inline const FLinearColor Light = FLinearColor(1.0f, 0.965f, 0.687f);       // #FFFBD8 warm cream
    inline const FLinearColor Darkness = FLinearColor(0.02f, 0.02f, 0.02f);     // #272727 near-black, no purple tint (D-106) — THE Darkness/BD black
    inline const FLinearColor Physical = FLinearColor::White;                   // Q-111
    inline const FLinearColor Generic = FLinearColor(0.6f, 0.4f, 0.2f);         // #CBAA7C brown (unchanged); never a damage-number colour
    inline const FLinearColor BrokenDarkness = Darkness;                       // BD IS Darkness — one black, one source

    // Get color for element
    inline FLinearColor GetColorForElement(ESpellElement Element)
    {
        return ElementTable::ForElement(Element).Colour; // None / Generic paint the neutral colour
    }

    /** Darkness is too dark to read as text on dark stone: draw it with a pale outline (D-106). */
    inline bool NeedsTextOutline(ESpellElement Element)
    {
        return Element == ESpellElement::Darkness;
    }

    /** The pale crystal outline for NeedsTextOutline text. */
    inline const FLinearColor TextOutline = FLinearColor(0.791f, 0.863f, 0.905f); // #E6EFF4

}