// LoadoutSkillsModel.h
// B-440d (UIScreens #2, mockup screen_mockups/menus/loadout/3_skills_refractor.png): the loadout screen's Skills page,
// with no widget in it, so the specs read it headless. One tab per list: a Refractor's innate spells by school (6 a
// school, on the points budget less the pillar discount, SpellPoolConstants.h), then each weapon's skill slots and each
// adapter's spell slots (D-145). The rows are gear-page rows (ELoadoutScreenRowKind::Skill / EmptySkill), so
// ULoadoutScreenWidget draws them with the gear page's list, picker and detail panel.
// A on a slot picks a learned skill (or one of the weapon's built-ins); X takes an innate spell out, or brings back what
// a pick replaced (Q-440d). Changes go through ULoadoutComponent::TryChangeGear (the loadout's rule check).
// Debug: wor.UI.PrintSkills logs GetSkillsPageString (every party member's Skills page).

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/SpellSchool.h"
#include "UI/Menus/LoadoutScreenModel.h"

class UAbilityData;
class USpellData;

enum class ELoadoutSkillList : uint8
{
	/** A Refractor's innate spells of one school. */
	Innate,
	WeaponSkills,
	AdapterSpells
};

struct FLoadoutSkillTab
{
	/** The school, or the weapon's or adapter's name. */
	FText Title;
	/** The filled slots, shown after the title ("Destruction 3"). */
	int32 Filled = 0;
	ELoadoutSkillList List = ELoadoutSkillList::Innate;
	ESpellSchool School = ESpellSchool::Destruction;
	/** The weapon's or adapter's gear-page row (its place); unused for a school. */
	FLoadoutScreenRow Gear;
	TArray<FLoadoutScreenRow> Rows;
};

struct FLoadoutSkillsPage
{
	TArray<FLoadoutSkillTab> Tabs;
	/** A Refractor's innate spell points: spent, and the most (0 for a class with no innate spells). */
	int32 Points = 0;
	int32 MaxPoints = 0;
	/** Taken off each innate spell's points by the pillars (SpellPoolConstants::SpellSlotDiscount). */
	int32 Discount = 0;
	/** Why there are no tabs (the member isn't in this level); empty otherwise. */
	FText Refusal;
};

namespace WoRLoadoutSkills
{
	/** The Skills page of one loadout. LearnedSpells add a Refractor's school tabs (the schools it holds a spell in). */
	WORLD_OF_REFRACTION_API FLoadoutSkillsPage BuildSkillsPage(const FCombatLoadout &Loadout, ECharacterClass Class, int32 Discount,
															   const TArray<USpellData *> &LearnedSpells);

	/** The Skills page of a party member's active loadout, read from its spawned inventory. */
	WORLD_OF_REFRACTION_API FLoadoutSkillsPage BuildSkillsPage(UWorld *World, const FLoadoutScreenMember &Member);

	/** What can go in Row's slot of Page.Tabs[Tab]: the learned spells of the school (innate), the learned abilities and
	 *  the weapon's built-ins (a weapon), the learned spells (an adapter); none already in the list. Each line's skill is
	 *  its Row.Skill; an innate line over the points stays, refused, with the points after it in NewSpent. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutScreenChoice> BuildSkillChoices(const FLoadoutSkillsPage &Page, int32 Tab, const FLoadoutScreenRow &Row,
																		   const TArray<UAbilityData *> &LearnedAbilities,
																		   const TArray<USpellData *> &LearnedSpells, const TArray<UAbilityData *> &BuiltIns);

	/** The member's choices, from what it has learned. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutScreenChoice> BuildSkillChoices(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutSkillsPage &Page,
																		   int32 Tab, const FLoadoutScreenRow &Row);

	/** Puts Skill in Row's slot of Loadout, or with no Skill takes it off (an innate spell comes out; a pick gives back
	 *  what it replaced). False, with the reason, when there is nothing to take off or the list can't take it. */
	WORLD_OF_REFRACTION_API bool PutSkill(FCombatLoadout &Loadout, const FLoadoutSkillTab &Tab, const FLoadoutScreenRow &Row, USkillDataBase *Skill,
										  FText &OutRefusal);

	/** PutSkill on the member's active loadout through ULoadoutComponent::TryChangeGear; the picks are written to the
	 *  owned pieces. Empty on success; else the reason, for the player. */
	WORLD_OF_REFRACTION_API FText ApplySkill(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutSkillTab &Tab, const FLoadoutScreenRow &Row,
											 USkillDataBase *Skill);

	WORLD_OF_REFRACTION_API FString GetSkillsPageString(UWorld *World);
}
