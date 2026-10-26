// StatModifierTable.h
// Which skill-effect types raise and lower each sub-stat (B-020). One row per ESubStat;
// USkillEffectManager::GetNetStatModifier sums a row as Modify + Buff - Debuff (percent).
// Add a stat's transient read here instead of writing a new Buff - Debuff pair at the call site.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Skills/Effects/ESkillEffectType.h"

/** A sub-stat's transient channels. ESkillEffectType::None means "no channel". */
struct FStatModifierRow
{
	ESkillEffectType Buff = ESkillEffectType::None;
	ESkillEffectType Debuff = ESkillEffectType::None;
	/** A signed "X Up" channel added on top (Turn Speed Up, Status Resist Up). */
	ESkillEffectType Modify = ESkillEffectType::None;
	/** A negative Buff counts as 0 (Crit Damage Up is up-only; guards malformed assets). */
	bool bBuffNeverNegative = false;
	/** Read only element-agnostic (None) effects; element-keyed ones apply where the element is known. */
	bool bGenericElementOnly = false;
};

namespace StatModifierTable
{
	inline FStatModifierRow Find(ESubStat Stat)
	{
		using E = ESkillEffectType;
		switch (Stat)
		{
		case ESubStat::Efficiency:       return {E::EfficiencyBuff, E::EfficiencyDebuff};
		case ESubStat::SpellDamage:      return {E::SpellDamageBuff, E::SpellDamageDebuff};
		// Generic only: the buildup site adds the element-keyed layer (StatusBuildupManager).
		case ESubStat::StatusMultiplier: return {E::StatusMultiplierBuff, E::StatusMultiplierDebuff, E::None, false, true};
		// Crit Damage Up (ModifyCritDamage) is the buff; there is no CritDamageBuff.
		case ESubStat::CritDamage:       return {E::ModifyCritDamage, E::CritDamageDebuff, E::None, true};
		case ESubStat::SpellSpeed:       return {E::SpellSpeedBuff, E::SpellSpeedDebuff};
		case ESubStat::Defense:          return {E::DefenseBuff, E::DefenseDebuff};
		case ESubStat::ActionSpeed:      return {E::ActionSpeedBuff, E::ActionSpeedDebuff};
		case ESubStat::RawDamage:        return {E::RawDamageBuff, E::RawDamageDebuff};
		// Status Resist Up only. ResistanceBuff/Debuff are element-matched and belong to the
		// status-buildup defense aggregate (StatusBuildupManager::GetTotalStatusResistance).
		case ESubStat::Resistance:       return {E::None, E::None, E::ModifyStatusResist};
		case ESubStat::TurnSpeed:        return {E::TurnSpeedBuff, E::TurnSpeedDebuff, E::ModifyTurnSpeed};
		case ESubStat::Luck:             return {E::LuckBuff, E::LuckDebuff};
		case ESubStat::Reflex:           return {E::ReflexBuff, E::ReflexDebuff};
		case ESubStat::None:
		default:                         return {};
		}
	}
}
