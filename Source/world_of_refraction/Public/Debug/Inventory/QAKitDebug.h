// QAKitDebug.h
// QA setup for agent play (Crown's overnight queue, 2026-11-24): the two things a QA save needs to see B-592b's HP
// ghost and a Caster's selector colour. Everything goes in through the real API (the loadout screen's skill page, the
// party invite), like wor.Debug.GiveTestKit. Hub only, host only, not in a run. Not in a shipping build.
// wor.Debug.QAKit [skill] [caster [element]]
//   no argument: skill and caster.
//   skill    the played character's weapon in hand gets an ability that costs EP in a skill slot (learned first if
//            needed: one whose weapon type and wield mode match), so Raw level II shows an HP cost.
//   caster   a Caster joins the party as a player's seat (not AI): a CharacterData asset of class Caster, the element
//            asked for if given (fire, water, ..., darkness), in a recruitable body standing here or the played one's.
//            With no Caster asset of that element it says so: wor.Debug.TestGear refractor / bd makes the played
//            character a Caster (Darkness for bd) for the PIE session instead.
// It changes the loaded game (learned skills, a slot, the party), which the next save keeps: the QA save only.

#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Skills/Definitions/ESpellElement.h"

class UAbilityData;
class UCharacterData;
class UWeaponData;
class UWorld;

namespace WoRQAKit
{
	/** Ability can show an infusion HP cost on Weapon: it costs EP, isn't the weapon's attack, and fits the weapon's
	 *  type and wield mode. */
	WORLD_OF_REFRACTION_API bool IsHPCostSkillFor(const UAbilityData *Ability, const UWeaponData *Weapon);

	/** The first Caster in Candidates not in the party (InParty), of Element when it isn't None; null if none. */
	WORLD_OF_REFRACTION_API UCharacterData *PickCaster(const TArray<UCharacterData *> &Candidates, ESpellElement Element,
													   const TArray<const UCharacterData *> &InParty);

	/** Runs wor.Debug.QAKit; Args as the command's. One line per part, or why nothing was done. */
	WORLD_OF_REFRACTION_API FString RunQAKit(UWorld *World, const TArray<FString> &Args);
}

#endif // !UE_BUILD_SHIPPING
