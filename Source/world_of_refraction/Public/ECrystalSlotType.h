// ECrystalSlotType.h
// Crystal slot types for weapons and rings
// NOTE: Different from ECrystalType.h which is for consumable items (Garnet, Sapphire, etc.)

#pragma once

#include "CoreMinimal.h"
#include "ECrystalSlotType.generated.h"

/**
 * Types of crystals that can be slotted into weapons and rings
 * 
 * These are NOT consumable items - they're permanent equipment modifications
 * 
 * Ilodite:
 * - Enhances existing weapon/ring effects
 * - Can be replaced (not permanent)
 * - Safe enhancement, no downsides
 * 
 * Quartz:
 * - Absorbs element via successful parry/block
 * - Transforms permanently when threshold reached
 * - Cannot be removed once slotted
 * - Gains element matching absorbed damage type
 * 
 * Evolution:
 * - Grants additional spells + stat bonuses
 * - Permanent, cannot be removed
 * - Has specific element tied to crystal
 * - Enables Evolution Infusion
 * - Has class-specific costs (see EvolutionData)
 */
UENUM(BlueprintType)
enum class ECrystalSlotType : uint8
{
	/** No crystal slotted */
	None UMETA(DisplayName = "Empty Slot"),

	/** Enhancement crystal - replaceable, boosts existing effects */
	Ilodite UMETA(DisplayName = "Ilodite"),

	/** Absorption crystal - permanent, gains element via combat */
	Quartz UMETA(DisplayName = "Quartz"),

	/** Evolution crystal - permanent, grants spells and stats */
	Evolution UMETA(DisplayName = "Evolution Crystal")
};

/**
 * Helper functions for crystal slot types
 */
namespace CrystalSlotHelpers
{
	/** Is a crystal slotted? */
	inline bool HasCrystal(ECrystalSlotType Type)
	{
		return Type != ECrystalSlotType::None;
	}

	/** Can this crystal be removed/replaced? */
	inline bool IsRemovable(ECrystalSlotType Type)
	{
		return Type == ECrystalSlotType::None || Type == ECrystalSlotType::Ilodite;
	}

	/** Is this crystal permanent once slotted? */
	inline bool IsPermanent(ECrystalSlotType Type)
	{
		return Type == ECrystalSlotType::Quartz || Type == ECrystalSlotType::Evolution;
	}

	/** Does this crystal enable Evolution Infusion? */
	inline bool EnablesEvolutionInfusion(ECrystalSlotType Type)
	{
		return Type == ECrystalSlotType::Evolution;
	}

	/** Can this crystal absorb elements from combat? */
	inline bool CanAbsorbElement(ECrystalSlotType Type)
	{
		return Type == ECrystalSlotType::Quartz;
	}

	/** Get display name */
	inline FString GetSlotTypeName(ECrystalSlotType Type)
	{
		switch (Type)
		{
		case ECrystalSlotType::None:      return TEXT("Empty");
		case ECrystalSlotType::Ilodite:   return TEXT("Ilodite");
		case ECrystalSlotType::Quartz:    return TEXT("Quartz");
		case ECrystalSlotType::Evolution: return TEXT("Evolution");
		default: return TEXT("Unknown");
		}
	}

	/** Get slot type description */
	inline FString GetSlotTypeDescription(ECrystalSlotType Type)
	{
		switch (Type)
		{
		case ECrystalSlotType::None:
			return TEXT("No crystal equipped. Slot available for modification.");
		case ECrystalSlotType::Ilodite:
			return TEXT("Enhancement crystal. Boosts weapon effects. Can be swapped.");
		case ECrystalSlotType::Quartz:
			return TEXT("Absorption crystal. Gains element from blocked attacks. Permanent.");
		case ECrystalSlotType::Evolution:
			return TEXT("Evolution crystal. Grants spells and stats. Permanent.");
		default:
			return TEXT("");
		}
	}
}
