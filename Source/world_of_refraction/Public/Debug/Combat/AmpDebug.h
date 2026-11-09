// AmpDebug.h
// B-382 (Q-164b A): inspect Amp lines without a fight. Console: wor.Damage.Amp

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/ESpellElement.h"

class AActor;
class UWorld;

namespace WoRAmpDebug
{
	/** "Name: +2 Amp, +1 Fire | Fire spell +3, Fire ability +3, unelemental attack +2": worn gear Amp and sample totals. */
	WORLD_OF_REFRACTION_API FString GetAmpString(AActor *Actor, ESpellElement SampleElement = ESpellElement::Fire);

	/** Logs GetAmpString for every actor in World with a loadout. */
	WORLD_OF_REFRACTION_API void PrintAmpState(UWorld *World);
}
