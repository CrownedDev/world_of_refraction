// EInfusionType.h
// Infusion types for the combat system - player's choice of infusion path

#pragma once

#include "CoreMinimal.h"
#include "EInfusionType.generated.h"

/**
 * Types of infusion that can be applied to abilities/spells
 *
 * Player chooses between Physical or Element infusion.
 * Both paths have 2 charge levels (L1, L2) via hold-to-charge mechanic.
 *
 * Physical: Always available to everyone
 * Element:  Available if character has an element source (innate/ring/crystal)
 */
UENUM(BlueprintType)
enum class EInfusionType : uint8
{
	/** No infusion applied - base action only */
	None UMETA(DisplayName = "No Infusion"),

	/** Physical enhancement - weapon status (L1), damage boost (L2)
	 *  Always available to all classes */
	Physical UMETA(DisplayName = "Physical Infusion"),

	/** Elemental enhancement - element status (L1), status + damage (L2)
	 *  Requires element source (innate/ring/crystal) */
	Element UMETA(DisplayName = "Element Infusion")
};

/**
 * Helper functions for infusion type checks
 */
namespace InfusionTypeHelpers
{
	/** Is this an active infusion (not None)? */
	inline bool IsInfused(EInfusionType Type)
	{
		return Type != EInfusionType::None;
	}

	/** Does this infusion type require an element source? */
	inline bool RequiresElementSource(EInfusionType Type)
	{
		return Type == EInfusionType::Element;
	}

	/** Get display name */
	inline FString GetInfusionName(EInfusionType Type)
	{
		switch (Type)
		{
		case EInfusionType::None:
			return TEXT("None");
		case EInfusionType::Physical:
			return TEXT("Physical");
		case EInfusionType::Element:
			return TEXT("Elemental");
		default:
			return TEXT("Unknown");
		}
	}

	/** Get infusion description */
	inline FString GetInfusionDescription(EInfusionType Type, int32 Level)
	{
		switch (Type)
		{
		case EInfusionType::None:
			return TEXT("No enhancement. Safe action with base power.");
		case EInfusionType::Physical:
			if (Level == 1)
				return TEXT("Adds weapon status buildup (stagger/impact).");
			else if (Level == 2)
				return TEXT("Adds weapon status and +30% damage.");
			return TEXT("Raw power boost. No element required.");
		case EInfusionType::Element:
			if (Level == 1)
				return TEXT("Adds element status buildup.");
			else if (Level == 2)
				return TEXT("Adds element status and +30% damage. Has additional cost.");
			return TEXT("Elemental enhancement. Requires element source.");
		default:
			return TEXT("");
		}
	}
}