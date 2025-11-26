// EInfusionType.h
// Infusion types for the combat system

#pragma once

#include "CoreMinimal.h"
#include "EInfusionType.generated.h"

/**
 * Types of infusion that can be applied to actions
 * 
 * Per-action choice: Player decides infusion type for each action
 * 
 * Availability by class:
 * - Generic:   Physical Infusion, Evolution Infusion*
 * - Caster:    Element Infusion, Evolution Infusion*
 * - Resonator: Ring Element Infusion, Evolution Infusion*
 * 
 * *Evolution Infusion only available with evolved weapon equipped
 * 
 * Break Risk:
 * - Any infusion increases break chance for Resonator rings
 * - Evolution Infusion has special interactions with evolved equipment
 */
UENUM(BlueprintType)
enum class EInfusionType : uint8
{
	/** No infusion applied - base action only */
	None UMETA(DisplayName = "No Infusion"),

	/** Physical enhancement (Generic class only)
	 *  Increases raw damage, adds physical properties */
	Physical UMETA(DisplayName = "Physical Infusion"),

	/** Elemental enhancement (Caster: innate element, Resonator: ring element)
	 *  Adds elemental damage, triggers elemental effects */
	Element UMETA(DisplayName = "Element Infusion"),

	/** Evolution crystal enhancement (all classes with evolved weapon)
	 *  Uses evolution crystal's element and abilities */
	Evolution UMETA(DisplayName = "Evolution Infusion")
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

	/** Does this infusion add elemental properties? */
	inline bool IsElemental(EInfusionType Type)
	{
		return Type == EInfusionType::Element || Type == EInfusionType::Evolution;
	}

	/** Does this infusion increase break risk for Resonators? */
	inline bool IncreasesBreakRisk(EInfusionType Type)
	{
		// All infusion types increase break risk
		return Type != EInfusionType::None;
	}

	/** Get display name */
	inline FString GetInfusionName(EInfusionType Type)
	{
		switch (Type)
		{
		case EInfusionType::None:      return TEXT("None");
		case EInfusionType::Physical:  return TEXT("Physical");
		case EInfusionType::Element:   return TEXT("Elemental");
		case EInfusionType::Evolution: return TEXT("Evolution");
		default: return TEXT("Unknown");
		}
	}

	/** Get infusion description */
	inline FString GetInfusionDescription(EInfusionType Type)
	{
		switch (Type)
		{
		case EInfusionType::None:
			return TEXT("No enhancement. Safe action with base power.");
		case EInfusionType::Physical:
			return TEXT("Raw power boost. Increases physical damage output.");
		case EInfusionType::Element:
			return TEXT("Elemental enhancement. Adds element damage and effects.");
		case EInfusionType::Evolution:
			return TEXT("Evolution crystal power. Uses evolved weapon's element.");
		default:
			return TEXT("");
		}
	}
}
