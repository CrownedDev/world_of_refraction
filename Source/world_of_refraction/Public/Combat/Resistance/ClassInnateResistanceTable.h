// ClassInnateResistanceTable.h
// Code-side (source-of-truth) class / innate-element status-buildup resistance
// table. Each profile is a FULL 12-value row: 9 element columns
// (Fire Water Earth Wind Light Darkness Lightning Void Reality) plus the 3
// physical columns (Slash Pierce Impact). Values are percent (+resist / -weak);
// the resolver returns (elementColumn + physicalColumn) / 100 as a single
// additive term, sign preserved. Where a column is 0 the term is +0 — neutral
// matchups are byte-identical to the pre-table behaviour.
//
// All values hardcoded here. Code change required to rebalance (mirrors the
// CrystalEffectTable convention). Gear-side resistance (rings / weapons /
// evolutions) is a SEPARATE future arc — it slots in additively at the
// AddStatusBuildup call site, not here.

#pragma once

#include "CoreMinimal.h"
#include "Character/ECharacterClass.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Combat/Damage/EPhysicalDamageType.h"

class AActor;

namespace ClassInnateResistanceTable
{
	// ==================== TIER CONSTANTS ====================
	// Three magnitudes span the whole table (no other values appear).

	/** Primary tier: a profile's signature resist / weakness, and the full
	 *  physical resist/weak band (Generic +, Resonator -). */
	constexpr float R_STRONG = 15.0f;

	/** Secondary tier: the Void spread and Reality's broad-resist / Void-weak band. */
	constexpr float R_MED = 10.0f;

	/** Minor tier: chip weaknesses and the universal BD / Generic element penalty. */
	constexpr float R_LIGHT = 5.0f;

	/** Percent-space -> fraction. Each column is authored in percent; the
	 *  resolved term divides by this before composing into resistance. */
	constexpr float RESISTANCE_PERCENT_DIVISOR = 100.0f;

	// ==================== ROW TYPE ====================

	/** One full resistance profile. Field order IS the column order:
	 *  Fire Water Earth Wind Light Darkness Lightning Void Reality | Slash Pierce Impact.
	 *  Plain struct (not reflected) — the Details-panel display struct is separate. */
	struct FResistanceRow
	{
		float Fire;
		float Water;
		float Earth;
		float Wind;
		float Light;
		float Darkness;
		float Lightning;
		float Void;
		float Reality;
		float Slash;
		float Pierce;
		float Impact;
	};

	// ==================== ELEMENT (CASTER) ROWS ====================

	/** The 9 Caster rows, keyed by the Caster's INNATE element. Field order per
	 *  FResistanceRow. Only ever reached after the BD / Generic / Resonator
	 *  selection arms fall through (see ResolveRow), so a BrokenDarkness innate
	 *  element never lands here. */
	inline FResistanceRow GetElementRow(ESpellElement InnateElement)
	{
		switch (InnateElement)
		{
		//                   Fire       Water      Earth      Wind       Light      Darkness   Lightning  Void       Reality    Sl   Pi   Im
		case ESpellElement::Fire:
			return {  R_STRONG, -R_STRONG,      0.0f, -R_LIGHT,      0.0f,      0.0f,      0.0f,      0.0f, -R_STRONG, 0.0f, 0.0f, 0.0f };
		case ESpellElement::Water:
			return { -R_STRONG,  R_STRONG,      0.0f,      0.0f,      0.0f,      0.0f, -R_STRONG,      0.0f, -R_STRONG, 0.0f, 0.0f, 0.0f };
		case ESpellElement::Earth:
			return {      0.0f, -R_LIGHT,  R_STRONG,      0.0f,      0.0f,      0.0f, -R_STRONG,      0.0f, -R_STRONG, 0.0f, 0.0f, 0.0f };
		case ESpellElement::Wind:
			return {      0.0f,      0.0f, -R_STRONG,  R_STRONG,      0.0f,      0.0f,      0.0f,      0.0f, -R_STRONG, 0.0f, 0.0f, 0.0f };
		case ESpellElement::Light:
			return {      0.0f,      0.0f,      0.0f,      0.0f,  R_STRONG, -R_STRONG,      0.0f,      0.0f, -R_STRONG, 0.0f, 0.0f, 0.0f };
		case ESpellElement::Darkness:
			return {      0.0f,      0.0f,      0.0f,      0.0f, -R_STRONG,  R_STRONG,      0.0f,   -R_MED, -R_STRONG, 0.0f, 0.0f, 0.0f };
		case ESpellElement::Lightning:
			return {      0.0f,  R_STRONG,      0.0f,      0.0f,      0.0f,      0.0f,  R_STRONG,      0.0f, -R_STRONG, 0.0f, 0.0f, 0.0f };
		case ESpellElement::Void:
			return {      0.0f,      0.0f,      0.0f,      0.0f,   -R_MED,   -R_MED,      0.0f,      0.0f,   -R_MED, 0.0f, 0.0f, 0.0f };
		case ESpellElement::Reality:
			return {   R_MED,    R_MED,    R_MED,    R_MED,    R_MED,    R_MED,    R_MED,   -R_MED,      0.0f, 0.0f, 0.0f, 0.0f };
		default:
			// Generic / unmapped innate element: no element profile (all-zero row).
			return { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		}
	}

	// ==================== ROW SELECTION ====================

	/** Resolve a target's profile row by the locked precedence:
	 *   1. bIsBrokenDarkness  -> the BD row (OVERRIDES everything below)
	 *   2. Generic class      -> the Generic row
	 *   3. Resonator class    -> the Resonator row
	 *   4. else (Caster)      -> GetElementRow(InnateElement)
	 *  InnateElement is the TARGET's own innate element (GetElement()), used only
	 *  for arm 4 — it is NOT the incoming-attack element (that is applied later by
	 *  GetElementColumn). */
	inline FResistanceRow ResolveRow(ECharacterClass Class, ESpellElement InnateElement, bool bIsBrokenDarkness)
	{
		//                                                                        Fire       Water      Earth      Wind       Light      Darkness   Lightning  Void       Reality    Slash      Pierce     Impact
		if (bIsBrokenDarkness)
		{
			return { -R_LIGHT, -R_LIGHT, -R_LIGHT, -R_LIGHT, -R_LIGHT, -R_LIGHT, -R_LIGHT, -R_LIGHT, -R_STRONG,      0.0f,      0.0f,      0.0f };
		}
		if (Class == ECharacterClass::Generic)
		{
			return { -R_LIGHT, -R_LIGHT, -R_LIGHT, -R_LIGHT, -R_LIGHT, -R_LIGHT, -R_LIGHT, -R_LIGHT,  -R_LIGHT,  R_STRONG,  R_STRONG,  R_STRONG };
		}
		if (Class == ECharacterClass::Resonator)
		{
			return {      0.0f,      0.0f,      0.0f,      0.0f,      0.0f,      0.0f,      0.0f,      0.0f,      0.0f, -R_STRONG, -R_STRONG, -R_STRONG };
		}
		return GetElementRow(InnateElement);
	}

	// ==================== COLUMN READS ====================

	/** Read the row's column for the INCOMING attack element. BD alias: an
	 *  incoming BrokenDarkness attack resolves via the Darkness column. A Generic
	 *  (non-elemental) incoming element contributes 0 — the row has no Generic
	 *  column. This is distinct from the BD-TARGET-row rule in ResolveRow. */
	inline float GetElementColumn(const FResistanceRow &Row, ESpellElement IncomingElement)
	{
		switch (IncomingElement)
		{
		case ESpellElement::Fire:
			return Row.Fire;
		case ESpellElement::Water:
			return Row.Water;
		case ESpellElement::Earth:
			return Row.Earth;
		case ESpellElement::Wind:
			return Row.Wind;
		case ESpellElement::Light:
			return Row.Light;
		case ESpellElement::Darkness:
			return Row.Darkness;
		case ESpellElement::Lightning:
			return Row.Lightning;
		case ESpellElement::Void:
			return Row.Void;
		case ESpellElement::Reality:
			return Row.Reality;
		case ESpellElement::Generic:
		default:
			return 0.0f;
		}
	}

	/** Read the row's column for the incoming physical type. None -> 0 (spells /
	 *  abilities pass None and carry no physical component). */
	inline float GetPhysicalColumn(const FResistanceRow &Row, EPhysicalDamageType PhysicalType)
	{
		switch (PhysicalType)
		{
		case EPhysicalDamageType::Slash:
			return Row.Slash;
		case EPhysicalDamageType::Pierce:
			return Row.Pierce;
		case EPhysicalDamageType::Impact:
			return Row.Impact;
		case EPhysicalDamageType::None:
		default:
			return 0.0f;
		}
	}

	// ==================== ACTOR-LEVEL QUERY ====================

	/** Resolve Target's profile (class + GetElement() + runtime IsBrokenDarkness)
	 *  and return the combined incoming-element + physical resistance as a
	 *  fraction: (elementColumn + physicalColumn) / 100. Both axes are summed —
	 *  a hit carries an element AND a physical type (a fire sword swing is both
	 *  Fire and Slash); a 0 column simply contributes 0. Returns 0 when Target or
	 *  its CharacterData is missing. Defined out-of-line (reaches into
	 *  UCharacterDataComponent). */
	WORLD_OF_REFRACTION_API float GetClassInnateResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType PhysicalType);
}
