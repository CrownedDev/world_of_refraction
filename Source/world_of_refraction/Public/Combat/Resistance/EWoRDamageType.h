// EWoRDamageType.h
// B-216 (ResistanceRedesign.md §3.1, D-037): what a hit's damage is, for resistance. One value per
// damage type, so one resistance per type (UWoRAttributeSet_Resistance).
// ESpellElement stays the identity (Generic = inherit) and EPhysicalDamageType the status axis;
// convert only through ToDamageType / PhysicalToDamageType, never a raw cast: ESpellElement has
// Generic = 0 and None = 10, and EPhysicalDamageType has Slash = 0.
// Tags: WoRTags::DamageTypeToTag. Debug: UResistanceQueryDebug (wor.CompareResistance).

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Combat/Damage/EPhysicalDamageType.h"
#include "EWoRDamageType.generated.h"

/** Append-only; the nine elements keep ESpellElement's numbers 1-9 for readability. */
UENUM(BlueprintType)
enum class EWoRDamageType : uint8
{
	None = 0 UMETA(ToolTip = "No damage type: pure effects, and the non-elemental resistance bucket"),
	Fire = 1,
	Water = 2,
	Earth = 3,
	Wind = 4,
	Light = 5,
	Darkness = 6,
	Lightning = 7,
	Void = 8,
	Reality = 9,
	Slash = 10 UMETA(ToolTip = "Physical"),
	Pierce = 11 UMETA(ToolTip = "Physical"),
	Impact = 12 UMETA(ToolTip = "Physical"),
	Ice = 13 UMETA(ToolTip = "D-029: keyed Water; no crystal, ring or Caster"),
};

namespace WoRDamageType
{
	/** Every damage type, in enum order (attribute mirror, debug, specs). */
	inline const TArray<EWoRDamageType> &All()
	{
		static const TArray<EWoRDamageType> Types = {
			EWoRDamageType::None, EWoRDamageType::Fire, EWoRDamageType::Water, EWoRDamageType::Earth,
			EWoRDamageType::Wind, EWoRDamageType::Light, EWoRDamageType::Darkness, EWoRDamageType::Lightning,
			EWoRDamageType::Void, EWoRDamageType::Reality, EWoRDamageType::Slash, EWoRDamageType::Pierce,
			EWoRDamageType::Impact, EWoRDamageType::Ice};
		return Types;
	}

	inline bool IsPhysical(EWoRDamageType Type)
	{
		return Type == EWoRDamageType::Slash || Type == EWoRDamageType::Pierce || Type == EWoRDamageType::Impact;
	}

	/** The element axis alone. Generic is resolved before a hit; a leftover Generic counts as
	 *  non-elemental, like None (Q-216). */
	inline EWoRDamageType ElementToDamageType(ESpellElement Element)
	{
		switch (Element)
		{
		case ESpellElement::Fire:      return EWoRDamageType::Fire;
		case ESpellElement::Water:     return EWoRDamageType::Water;
		case ESpellElement::Earth:     return EWoRDamageType::Earth;
		case ESpellElement::Wind:      return EWoRDamageType::Wind;
		case ESpellElement::Light:     return EWoRDamageType::Light;
		case ESpellElement::Darkness:  return EWoRDamageType::Darkness;
		case ESpellElement::Lightning: return EWoRDamageType::Lightning;
		case ESpellElement::Void:      return EWoRDamageType::Void;
		case ESpellElement::Reality:   return EWoRDamageType::Reality;
		case ESpellElement::Generic:
		case ESpellElement::None:
		default:
			return EWoRDamageType::None;
		}
	}

	/** The physical axis alone; EPhysicalDamageType::None gives None. */
	inline EWoRDamageType PhysicalToDamageType(EPhysicalDamageType PhysicalType)
	{
		switch (PhysicalType)
		{
		case EPhysicalDamageType::Slash:  return EWoRDamageType::Slash;
		case EPhysicalDamageType::Pierce: return EWoRDamageType::Pierce;
		case EPhysicalDamageType::Impact: return EWoRDamageType::Impact;
		case EPhysicalDamageType::None:
		default:
			return EWoRDamageType::None;
		}
	}

	/** A hit's damage type (§3.1): its element if it has one, else its physical subtype, else None.
	 *  A Fire-infused swing is Fire; an uninfused swing is Slash. */
	inline EWoRDamageType ToDamageType(ESpellElement Element, EPhysicalDamageType PhysicalType)
	{
		const EWoRDamageType ElementType = ElementToDamageType(Element);
		return ElementType != EWoRDamageType::None ? ElementType : PhysicalToDamageType(PhysicalType);
	}
}
