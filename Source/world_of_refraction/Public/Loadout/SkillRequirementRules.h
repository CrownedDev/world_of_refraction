// SkillRequirementRules.h
// D-224 (asset overhaul §5.2): the one gate for "can this skill be used in this piece". A skill names what it needs
// (FSkillGearRequirement); the piece it sits in (its host: a worn weapon, an adapter, or the evolution seat) meets it
// or not. Read by the loadout rules, the weapon entry, the Equipment screen's choices and the battle list.
// The wield mode (UAbilityData::AllowsWieldMode) and a gem slot's element (FWeaponLoadoutEntry::CanHoldSkill) stay
// their own checks.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/SkillGearRequirement.h"

class USkillDataBase;
class UEvolutionItemData;
struct FWeaponInventoryEntry;
struct FRingInventoryEntry;

/** The piece a skill sits in, as the rule reads it. */
struct WORLD_OF_REFRACTION_API FSkillGearHost
{
	/** Weapon, Adapter or Evolution (the seat); None: no piece. */
	ESkillGearKind Kind = ESkillGearKind::None;
	EWeaponType WeaponType = EWeaponType::Sword;
	EAdapterForm Form = EAdapterForm::Ring;
	/** The socket's element; None when the socket is empty. */
	ESpellElement Element = ESpellElement::None;
	/** The evolution in the socket or the seat. */
	const UEvolutionItemData *Evolution = nullptr;

	/** A weapon entry is read by its main hand (a set's sockets are the main hand's, D-209). */
	static FSkillGearHost FromWeapon(const FWeaponInventoryEntry &Entry);
	static FSkillGearHost FromAdapter(const FRingInventoryEntry &Entry);
	static FSkillGearHost FromEvolutionSeat(const UEvolutionItemData *Evolution);
};

namespace SkillRequirementRules
{
	/** The skill's gear requirement is met by the piece it sits in. A null skill is not. */
	WORLD_OF_REFRACTION_API bool IsMet(const USkillDataBase *Skill, const FSkillGearHost &Host);

	/** Player-facing: what the skill needs ("Needs an Axe."); empty for no requirement. */
	WORLD_OF_REFRACTION_API FText GetNeedText(const USkillDataBase *Skill);

	/** Debug: "Weapon Axe, element Fire" or "None". */
	WORLD_OF_REFRACTION_API FString GetRequirementString(const USkillDataBase *Skill);
}
