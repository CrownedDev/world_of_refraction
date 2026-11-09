// DamageScopeDebug.h
// B-383 (D-166): each damage / defence boost on an actor with its scope. Console: wor.Damage.Scopes

#pragma once

#include "CoreMinimal.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Definitions/ESpellElement.h"

class AActor;
class UWorld;

namespace WoRDamageScopeDebug
{
	/** Whether Type is one of the boosts that read a damage scope (attacker or defender side). */
	WORLD_OF_REFRACTION_API bool IsScopedType(ESkillEffectType Type);

	/** "Name: DamageBuff +20% [All]; DefenseBuff +30% [Fire only]" (or "no damage / defence boosts"). */
	WORLD_OF_REFRACTION_API FString GetScopesString(AActor *Actor);

	/** B-384 (D-167): "Name: All-damage +40% (Fire spell), +20% (physical)": the one summed channel for two sample hits. */
	WORLD_OF_REFRACTION_API FString GetAllDamageString(AActor *Actor, ESpellElement SampleElement = ESpellElement::Fire);

	/** Logs GetScopesString and GetAllDamageString for every actor in World that has effects. */
	WORLD_OF_REFRACTION_API void PrintScopes(UWorld *World);
}
