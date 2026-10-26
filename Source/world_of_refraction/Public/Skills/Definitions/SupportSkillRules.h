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
			if (Spell->School == ESpellSchool::Enhancement || Spell->School == ESpellSchool::Restoration)
			{
				return true;
			}
		}
		return Skill->BaseDamage == 0 && Skill->HasBuffEffects();
	}

	/** The authored status buildup the skill brings to a hit: none for a support skill. */
	inline int32 GetAuthoredStatusBuildup(const USkillDataBase *Skill)
	{
		return Skill && !IsSupportSkill(Skill) ? Skill->StatusBuildup : 0;
	}
}
