// TestLoadoutDebug.h
// B-503n (Crown: test switching properly): wor.Debug.TestLoadout gives the played character a full v2 loadout so
// R / LB steps real infusion sources: up to three weapons with crystals of different elements (the first in hand), an
// adapter with a crystal, and an evolution (in a piece's socket, else the first entry). Each piece goes in through the
// v2 screen's own path (WoRLoadoutV2::ApplyDraft inside ULoadoutComponent::TryChangeGear), so the class rules decide
// what fits; a refused piece stays owned and is logged. It replaces the active loadout's gear (the old gear stays owned).
// The evolution goes in the entry's socket only (as the screen's socket pick does), never onto the owned piece.
// Variants (Generic, Refractor, Resonator, BrokenDarkness, Reality) swap the played pawn's character data for that
// class's test character first (the pawn only: not the party record; gone at the next map). The gear is owned for
// good (it reaches the pool and the save), like any gear given in PIE.
// Debug: GetTestLoadoutString (what went in, what was refused); wor.Field.Print shows the stops after it.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/ESpellElement.h"

class APawn;

enum class ETestLoadoutVariant : uint8
{
	/** The played character as it is. */
	Played,
	Generic,
	Refractor,
	Resonator,
	BrokenDarkness,
	Reality
};

namespace WoRTestLoadout
{
	/** A crystal for each piece that can take one: three weapons and two adapters, one of them holding the evolution. */
	constexpr int32 TEST_CRYSTAL_COUNT = 4;

	/** "Played", "Generic", "Refractor", "Resonator", "BrokenDarkness", "Reality" (any case); an empty string is Played.
	 *  False for anything else. */
	WORLD_OF_REFRACTION_API bool ParseVariant(const FString &Text, ETestLoadoutVariant &OutVariant);

	/** The test character's data asset path for a variant; empty for Played. */
	WORLD_OF_REFRACTION_API FString GetVariantCharacterPath(ETestLoadoutVariant Variant);

	/** Count elements for the pieces' crystals, in a fixed order, never Innate (its own stop) nor Skip. */
	WORLD_OF_REFRACTION_API TArray<ESpellElement> PickCrystalElements(ESpellElement Innate, ESpellElement Skip, int32 Count);

	/** The test evolution's element: the first of a fixed order that isn't Innate. */
	WORLD_OF_REFRACTION_API ESpellElement PickEvolutionElement(ESpellElement Innate);

	/** The evolution asset path used for Element; empty when there is none. */
	WORLD_OF_REFRACTION_API FString GetEvolutionPath(ESpellElement Element);

	/** Gives Pawn (the played character) the test loadout in its active saved loadout, replacing its gear (innate spells
	 *  and items stay). Server only. Returns the report (GetTestLoadoutString's lines). */
	WORLD_OF_REFRACTION_API FString GiveTestLoadout(APawn *Pawn, ETestLoadoutVariant Variant);

	/** Debug: the report of the last GiveTestLoadout ("(none yet)" before one). */
	WORLD_OF_REFRACTION_API FString GetTestLoadoutString();
}
