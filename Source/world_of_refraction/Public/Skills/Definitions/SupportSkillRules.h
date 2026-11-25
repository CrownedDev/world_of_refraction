// SupportSkillRules.h
// Rules page 03 (Crown, 2026-10-26): heals, buffs and support skills never build status, on anyone.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Combat/Actions/ActionStructs.h"

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

	/** B-620 (D-275, Crown 2026-11-25): "weapon buffs cannot be infused with elements". A weapon ability with no
	 *  damage aimed at its user or an ally (a buff like Rally) takes no infusion. */
	inline bool IsWeaponBuff(const USkillDataBase *Skill)
	{
		return Skill && Skill->IsA<UAbilityData>() && Skill->BaseDamage <= 0 &&
			   (Skill->TargetType == ETargetType::Self || Skill->TargetType == ETargetType::Ally);
	}

	/** Action with any infusion dropped when it uses a weapon buff (the executor ignores it, it doesn't refuse it). */
	inline FAction WithoutWeaponBuffInfusion(const FAction &Action)
	{
		FAction Out = Action;
		if (IsWeaponBuff(Action.SkillData))
		{
			Out.SelectedSource = EInfusionSourceOption::None;
			Out.bIceInfusion = false;
			Out.AbilityInfusionLevel = 0;
			Out.SpellInfusionLevel = 0;
		}
		return Out;
	}

	/** The authored status buildup the skill brings to a hit: none for a support skill. */
	inline int32 GetAuthoredStatusBuildup(const USkillDataBase *Skill)
	{
		return Skill && !IsSupportSkill(Skill) ? Skill->StatusBuildup : 0;
	}
}
