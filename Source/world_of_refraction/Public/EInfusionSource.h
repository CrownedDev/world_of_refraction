// EInfusionSource.h
// Tracks the source of element for infusion - determines L2 costs

#pragma once

#include "CoreMinimal.h"
#include "EInfusionSource.generated.h"

/**
 * Source of element for infusion
 * 
 * Determines the L2 cost when using Element Infusion or Spell Size Infusion:
 * - Innate:    HP cost (Caster only)
 * - Ring:      Ring break chance (Resonator)
 * - Crystal:   Crystal break chance (Any class with weapon crystal)
 * - Evolution: HP damage + self-status buildup (Evolution weapon spells)
 * 
 * Priority when determining source:
 * 1. Weapon crystal (if slotted and not Ilodite)
 * 2. Caster innate element
 * 3. Resonator active ring
 */
UENUM(BlueprintType)
enum class EInfusionSource : uint8
{
	/** No element source available - cannot use Element Infusion */
	None UMETA(DisplayName = "No Source"),

	/** Caster's innate element
	 *  L2 Cost: 5% Max HP */
	Innate UMETA(DisplayName = "Innate Element"),

	/** Resonator's equipped ring
	 *  L2 Cost: +20% Ring Break Chance (spells), +15% (abilities) */
	Ring UMETA(DisplayName = "Ring Element"),

	/** Weapon's slotted crystal (non-Ilodite)
	 *  L2 Cost: +15% Crystal Break Chance */
	Crystal UMETA(DisplayName = "Crystal Element"),

	/** Evolution weapon spells
	 *  L2 Cost: 3% Max HP + 50% Self-Status Buildup */
	Evolution UMETA(DisplayName = "Evolution Element")
};

/**
 * Helper functions for infusion source checks
 */
namespace InfusionSourceHelpers
{
	/** Does this source have an element? */
	inline bool HasElement(EInfusionSource Source)
	{
		return Source != EInfusionSource::None;
	}

	/** Does L2 cost HP damage? */
	inline bool CostsHP(EInfusionSource Source)
	{
		return Source == EInfusionSource::Innate || Source == EInfusionSource::Evolution;
	}

	/** Does L2 increase break chance? */
	inline bool CostsBreakChance(EInfusionSource Source)
	{
		return Source == EInfusionSource::Ring || Source == EInfusionSource::Crystal;
	}

	/** Does L2 apply self-status? (Evolution only) */
	inline bool CostsSelfStatus(EInfusionSource Source)
	{
		return Source == EInfusionSource::Evolution;
	}

	/** Can this source break? */
	inline bool CanBreak(EInfusionSource Source)
	{
		return Source == EInfusionSource::Ring || Source == EInfusionSource::Crystal;
	}

	/** Get display name */
	inline FString GetSourceName(EInfusionSource Source)
	{
		switch (Source)
		{
		case EInfusionSource::None:      return TEXT("None");
		case EInfusionSource::Innate:    return TEXT("Innate");
		case EInfusionSource::Ring:      return TEXT("Ring");
		case EInfusionSource::Crystal:   return TEXT("Crystal");
		case EInfusionSource::Evolution: return TEXT("Evolution");
		default: return TEXT("Unknown");
		}
	}

	/** Get L2 cost description */
	inline FString GetL2CostDescription(EInfusionSource Source)
	{
		switch (Source)
		{
		case EInfusionSource::None:
			return TEXT("N/A");
		case EInfusionSource::Innate:
			return TEXT("5% Max HP");
		case EInfusionSource::Ring:
			return TEXT("+15-20% Ring Break Chance");
		case EInfusionSource::Crystal:
			return TEXT("+15% Crystal Break Chance");
		case EInfusionSource::Evolution:
			return TEXT("3% Max HP + Self-Status Buildup");
		default:
			return TEXT("");
		}
	}
}
