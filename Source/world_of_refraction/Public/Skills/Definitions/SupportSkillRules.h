// SupportSkillRules.h
// Rules page 03 (Crown, 2026-10-26): heals, buffs and support skills never build status, on anyone.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"

namespace SupportSkillRules
{
	/** A heal / buff / support skill: an Enhancement or Restoration spell, or any skill with no
	 *  damage and a buff or heal effect (the IsSupportAbility test). */
	inline bool IsSupportSkill(const USkillDataBase *Skill)
	{
		if (!Skill)
		{
			return false;
		}
		if (const USpellData *Spell = Cast<USpellData>(Skill))
		{
			const ESpellSchool School = Spell->GetSchool(); // B-356: a retired school reads as its mapped one
			if (School == ESpellSchool::Enhancement || School == ESpellSchool::Restoration)
			{
				return true;
			}
		}
		return Skill->BaseDamage == 0 && Skill->HasBuffEffects();
	}

	/** B-621: a no-damage skill aimed at the caster's own side (Self / Ally). It lands no hit: no defend
	 *  window, no damage, no hit reaction; its effects still apply to its targets. */
	inline bool IsFriendlyNoDamageSkill(const USkillDataBase *Skill)
	{
		return Skill && Skill->BaseDamage <= 0 &&
			   (Skill->TargetType == ETargetType::Self || Skill->TargetType == ETargetType::Ally);
	}

	/** The authored status buildup the skill brings to a hit: none for a support skill. */
	inline int32 GetAuthoredStatusBuildup(const USkillDataBase *Skill)
	{
		return Skill && !IsSupportSkill(Skill) ? Skill->StatusBuildup : 0;
	}
}
