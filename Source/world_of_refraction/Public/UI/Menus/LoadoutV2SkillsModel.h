// LoadoutV2SkillsModel.h
// B-476d (UIScreens.md #2-3 Skills tab, mockups menus/loadout/v2/6_skills_generic.png, 6a, 6b, 7_skills_refractor.png):
// the v2 loadout screen's Skills tab, with no widget in it, so the specs read it headless. The left lists the same
// entries as the Weapons tab (names only); the right shows the focused entry's skill slots under "Name · 4/9 skills"
// (built-in, picked and empty rows). A Refractor also has an Innate spells entry whose right side is its school rows.
// The rows, the picker and the change are today's Skills page's (WoRLoadoutSkills, B-440d): each v2 entry lists the
// matching weapon or adapter tab of that page, so the rule checks and the save are the same.
// Each skill says what it does (Crown): EP, power, targets, then the effect tree (ET-50), or its short form.
// Debug: GetSkillsString (logged when the tab opens); wor.UI.LoadoutV2 opens the screen (RB to the Skills tab).
// TODO: B-476e: a dual entry's merged list (D-211) once the code merges a pair's slots; the skill's element, scaling
// and pillar requirement lines.

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/LoadoutSkillsModel.h"
#include "UI/Menus/LoadoutV2Model.h"

class USkillDataBase;

/** B-476d: one entry on the Skills tab's left. */
struct FLoadoutV2SkillEntry
{
	/** The Weapons tab's name ("Ashen Blade + Cinder Edge"), or "Innate spells". */
	FText Name;
	/** "Ashen Blade + Cinder Edge · 4/9 skills"; "Innate spells · 18/24 points". */
	FText Header;
	int32 Filled = 0;
	int32 Total = 0;
	bool bInnate = false;
	/** The Weapons tab entry this one stands for (its icons, sockets and ACTIVE show on the row); none for Innate spells. */
	int32 WeaponEntry = INDEX_NONE;
	/** The Skills page's tabs (WoRLoadoutSkills) this entry lists, in order: one for a weapon or adapter, the schools
	 *  for Innate spells. */
	TArray<int32> Tabs;
};

/** B-476d: one row of an entry: a tab and a row of the Skills page. */
struct FLoadoutV2SkillRowRef
{
	int32 Tab = INDEX_NONE;
	int32 Row = INDEX_NONE;
};

struct FLoadoutV2SkillsPage
{
	/** Today's Skills page: the rows, picked into and changed through WoRLoadoutSkills. */
	FLoadoutSkillsPage Source;
	TArray<FLoadoutV2SkillEntry> Entries;
};

namespace WoRLoadoutV2Skills
{
	/** The Skills tab: the Weapons tab's entries that have skills (weapons, adapters), each with its Skills page tab,
	 *  then Innate spells when the page has school tabs. */
	WORLD_OF_REFRACTION_API FLoadoutV2SkillsPage Build(const FLoadoutSkillsPage &Source, const FLoadoutV2WeaponsPage &Weapons);

	/** B-610: the Skills tab entry that stands for the Weapons tab's WeaponEntry, or INDEX_NONE (an evolution, a piece
	 *  with no skill slots). */
	WORLD_OF_REFRACTION_API int32 FindEntryFor(const FLoadoutV2SkillsPage &Page, int32 WeaponEntry);

	/** Entry's rows, in order (a school's rows after the one before). */
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2SkillRowRef> GetRows(const FLoadoutV2SkillsPage &Page, int32 Entry);

	/** The Skills page row a reference points at; null when it doesn't. */
	WORLD_OF_REFRACTION_API const FLoadoutScreenRow *FindRow(const FLoadoutV2SkillsPage &Page, const FLoadoutV2SkillRowRef &Ref);

	/** What Skill does: its name and tier (KindLine, the row's "Ability · Fire from Garnet", is the quiet line above
	 *  the name, as a piece's type on the Weapons tab), EP, power, targets, then the effect tree (bSimple: its short
	 *  form). Empty without a skill. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2DetailLine> BuildSkillDetails(const USkillDataBase *Skill, bool bSimple, const FText &KindLine = FText::GetEmpty());

	/** B-629 (mockup menus/skills/v1): the Details column's facts for Entry: its slots, used, free; Innate spells adds
	 *  its points. Empty for a bad entry.
	 *  TODO: B-629: the mockup's "Takes" (the skill kinds that fit) and "Infusion from" (the crystal and its element). */
	WORLD_OF_REFRACTION_API TArray<FLoadoutScreenDetail> BuildEntryFacts(const FLoadoutV2SkillsPage &Page, int32 Entry);

	WORLD_OF_REFRACTION_API FString GetSkillsString(const FLoadoutV2SkillsPage &Page);
}
