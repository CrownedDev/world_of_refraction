// ECharacterClass.h
// Character class system for World of Refraction

#pragma once

#include "CoreMinimal.h"
#include "ECharacterClass.generated.h"

/**
 * Character classes. A class is made by its slots, not a skill gate (D-027): what a character can
 * use comes from what its slots hold. Weapons give abilities (an attached crystal makes them
 * elemental, never spells); spells come from rings, evolutions and a Refractor's innate pool.
 *
 * Generic:
 * - Primary slot (weapon, ring or evolution) + a secondary weapon slot; no innate element
 * - Casts only through a ring or evolution it wears
 *
 * Caster (shown as "Refractor"):
 * - Primary slot (weapon, ring or evolution) + innate spells (cost budget; see SpellPoolBudget)
 * - The only class with an innate element (chosen on its character data)
 *
 * Resonator:
 * - Primary slot (weapon or evolution, never a ring) + a ring loadout, sharing a budget of 5
 *   (LoadoutConstants); any mix of ring elements (D-010)
 * - Casts the active ring's spells; rings can break
 */
UENUM(BlueprintType)
enum class ECharacterClass : uint8
{
	Generic UMETA(DisplayName = "Generic"),
	Caster UMETA(DisplayName = "Refractor"),
	Resonator UMETA(DisplayName = "Resonator")
};

/**
 * Helper functions for character class checks
 */
namespace CharacterClassHelpers
{
	/** Can this class use physical abilities from weapons? */
	inline bool CanUseAbilities(ECharacterClass Class)
	{
		return Class == ECharacterClass::Generic;
	}

	/** Can this class use spells? */
	inline bool CanUseSpells(ECharacterClass Class)
	{
		return Class != ECharacterClass::Generic;
	}

	/** Does this class have an innate element? */
	inline bool HasInnateElement(ECharacterClass Class)
	{
		return Class == ECharacterClass::Caster;
	}

	/** Can this class equip multiple weapons? */
	inline bool CanDualWield(ECharacterClass Class)
	{
		return Class == ECharacterClass::Generic;
	}

	/** Does this class use rings? */
	inline bool UsesRings(ECharacterClass Class)
	{
		return Class == ECharacterClass::Resonator;
	}

	/** Get display name */
	inline FString GetClassName(ECharacterClass Class)
	{
		switch (Class)
		{
		case ECharacterClass::Generic:
			return TEXT("Generic");
		case ECharacterClass::Caster:
			return TEXT("Refractor");
		case ECharacterClass::Resonator:
			return TEXT("Resonator");
		default:
			return TEXT("Unknown");
		}
	}

	/** Get class description */
	inline FString GetClassDescription(ECharacterClass Class)
	{
		switch (Class)
		{
		case ECharacterClass::Generic:
			return TEXT("Weapon fighter with two weapon slots. Casts only through a ring or evolution it wears.");
		case ECharacterClass::Caster:
			return TEXT("Elemental caster with innate spells of its own element, plus any slotted gear.");
		case ECharacterClass::Resonator:
			return TEXT("Ring caster. Switches elements by switching rings, but rings can break.");
		default:
			return TEXT("");
		}
	}
}
