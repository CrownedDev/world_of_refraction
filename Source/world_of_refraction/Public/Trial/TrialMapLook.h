// TrialMapLook.h
// B-189 (RunStructure §4): a trial's element tailors its map's look: sky tint, fog and sunlight from one
// per-element table (placeholder values, TODO: tune with Crown). Applied when the active trial's level loads
// (UTrialRunSubsystem); a Generic (untyped) trial keeps the map as authored.
// Only movable or stationary lights and a height fog / sky atmosphere placed in the level change; a static
// (baked) light keeps its colour.
// Debug: wor.Trial.Look [Element] (preview a look in any map; no argument prints the table), GetLookString.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/ESpellElement.h"

class UWorld;

/** One element's map look. */
struct WORLD_OF_REFRACTION_API FTrialMapLook
{
	/** Generic (and None): no look, the map stays as authored. */
	bool bApplies = false;
	/** The sky atmosphere's Rayleigh scattering (the sky's colour). */
	FLinearColor SkyTint = FLinearColor::White;
	FLinearColor FogColour = FLinearColor::White;
	float FogDensity = 0.0f;
	FLinearColor SunColour = FLinearColor::White;
	/** Multiplies the sun's authored intensity. */
	float SunIntensityScale = 1.0f;
};

/** What ApplyTo changed, for the log and the spec. */
struct WORLD_OF_REFRACTION_API FTrialMapLookResult
{
	int32 Suns = 0;
	int32 Fogs = 0;
	int32 Skies = 0;
};

namespace TrialMapLook
{
	/** The look for Element from the one table. */
	WORLD_OF_REFRACTION_API FTrialMapLook ForElement(ESpellElement Element);

	/** Sets every directional light, height fog and sky atmosphere in World to Element's look. Nothing for Generic. */
	WORLD_OF_REFRACTION_API FTrialMapLookResult ApplyTo(UWorld *World, ESpellElement Element);

	/** Debug: one line per element (or just Element's). */
	WORLD_OF_REFRACTION_API FString GetLookString(TOptional<ESpellElement> Element = {});
}
