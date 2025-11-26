// ECharacterClass.h
// Character class system for World of Refraction

#pragma once

#include "CoreMinimal.h"
#include "ECharacterClass.generated.h"

/**
 * Character classes determining combat style and equipment options
 *
 * Generic:
 * - 2 physical weapons, no innate spells
 * - Abilities come from weapons
 * - Physical Infusion or Evolution Infusion (if evolved)
 * - Most durable, least flexible
 *
 * Caster:
 * - 1 weapon, innate element (locked at creation)
 * - 6 innate spells tied to element
 * - Element Infusion or Evolution Infusion (if evolved)
 * - Energy-based, balanced flexibility
 *
 * Resonator:
 * - Uses rings (6 slots, up to 3 same element)
 * - No innate element - element comes from equipped ring
 * - 6 spells per ring (variable per ring)
 * - Ring Element Infusion or Evolution Infusion (if evolved)
 * - Most flexible but most fragile (rings can break)
 */
UENUM(BlueprintType)
enum class ECharacterClass : uint8
{
	Generic UMETA(DisplayName = "Generic"),
	Caster UMETA(DisplayName = "Elementalist"),
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
			return TEXT("Elementalist");
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
			return TEXT("Physical fighter with dual weapons and abilities. No spells, but most durable.");
		case ECharacterClass::Caster:
			return TEXT("Elemental mage with innate spells. Locked to one element, balanced power.");
		case ECharacterClass::Resonator:
			return TEXT("Ring-based caster. Switch elements freely, but rings can break.");
		default:
			return TEXT("");
		}
	}
}
