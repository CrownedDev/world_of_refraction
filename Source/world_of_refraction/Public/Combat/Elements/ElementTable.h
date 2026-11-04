// ElementTable.h
// B-271 (SimplifyAudit S5): one row per damage type holding what the old per-element switches each
// knew: its element and physical axes, gem, essence, break trigger, immunity, colour and Broken
// Darkness flags. The old functions forward here; WoR.Elements.Compare / wor.CompareElements pin
// old = new. Names (FText) and gameplay tags stay in their tables (WoRGameNames, WoRTags): they
// need the engine up, this table doesn't.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Resistance/EWoRDamageType.h"

enum class ECrystalType : uint8;
enum class EEssenceType : uint8;
enum class ESkillEffectType : uint8;

struct FElementRow
{
	EWoRDamageType Type;
	/** The element axis (None for physical, None and Ice: ResistanceQuery's ToAxes). */
	ESpellElement Element;
	EPhysicalDamageType Physical;
	/** A second type whose resistance also applies (Water -> Ice, Q-145b); None otherwise. */
	EWoRDamageType AlsoResistedBy;
	/** The gem of this element (Quartz for None); ECrystalType::None when no gem carries it. */
	ECrystalType Gem;
	/** OBSOLETE (B-333b): the typed essences retire (D-125); goes with them in the sweep (D-124). */
	EEssenceType Essence;
	/** What a capped status bar fires (BarCapTriggerResolver). A Broken Darkness source's Darkness
	 *  drains energy instead (the resolver's branch, a property of the caster). */
	ESkillEffectType BreakTrigger;
	/** The elemental immunity an item grants; ESkillEffectType::None when there's no element. */
	ESkillEffectType Immunity;
	FLinearColor Colour;
	/** Broken Darkness can absorb it (rotates its pool). */
	bool bBDAbsorbable;
	/** Broken Darkness pays to cast it (Light, Void). */
	bool bBDForbidden;
};

namespace ElementTable
{
	/** Every row, in EWoRDamageType order. */
	WORLD_OF_REFRACTION_API const TArray<FElementRow> &Rows();

	WORLD_OF_REFRACTION_API const FElementRow &For(EWoRDamageType Type);

	/** The element's row; Generic and None read the None row. */
	WORLD_OF_REFRACTION_API const FElementRow &ForElement(ESpellElement Element);

	/** The physical type's row; EPhysicalDamageType::None reads the None row. */
	WORLD_OF_REFRACTION_API const FElementRow &ForPhysical(EPhysicalDamageType Physical);
}
