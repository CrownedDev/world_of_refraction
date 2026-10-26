// StoneRowTable.h
// One row per stat stone (B-021): the sub-stat it amplifies when attached, its consumable's
// item effect, and the buff/debuff that consumable applies (ally gets Buff, enemy Debuff).
// CrystalEffectTable::StoneTargetStat / GetStoneBasePercent, ItemIdentity::GetItemEffectType
// and UItemExecutor::UseItem read this instead of keeping their own per-stone switches.
// Adding a stat stone = one row here (plus its ECrystalType and EItemEffectType).

#pragma once

#include "CoreMinimal.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Inventory/ItemEffectType.h"
#include "Skills/Effects/ESkillEffectType.h"

struct FStoneRow
{
	ECrystalType Type;
	/** What the stone amplifies when attached. None for the pool stones (Max HP / Max EP). */
	ESubStat Stat;
	EItemEffectType ItemEffect;
	ESkillEffectType Buff;
	ESkillEffectType Debuff;
	/** Effect-name suffix: "<stone display name> <Label>". */
	const TCHAR *Label;
	/** Consumable is the standard directional buff (UItemExecutor::ApplyStoneBuffEffect). False for
	 *  ResistanceStone, which encodes direction by sign on ModifyStatusResist. */
	bool bStandardConsumable;
};

namespace StoneRowTable
{
	using SE = ESkillEffectType;
	using IE = EItemEffectType;

	inline constexpr FStoneRow ROWS[] = {
		{ECrystalType::DamageStone, ESubStat::RawDamage, IE::BuffRawDamage, SE::RawDamageBuff, SE::RawDamageDebuff, TEXT("Raw Damage"), true},
		{ECrystalType::DefenseStone, ESubStat::Defense, IE::BuffDefense, SE::DefenseBuff, SE::DefenseDebuff, TEXT("Defense"), true},
		// Crit-DAMAGE form (the attached CritStone's); Opal keeps BuffCrit (crit chance).
		{ECrystalType::CritStone, ESubStat::CritDamage, IE::BuffCritDamage, SE::ModifyCritDamage, SE::CritDamageDebuff, TEXT("Crit Damage"), true},
		// Pure stat; no turn mechanic (that is Emerald's GrantBonusTurn).
		{ECrystalType::TurnSpeedStone, ESubStat::TurnSpeed, IE::BuffTurnSpeed, SE::TurnSpeedBuff, SE::TurnSpeedDebuff, TEXT("Turn Speed"), true},
		{ECrystalType::StatusStone, ESubStat::StatusMultiplier, IE::BuffStatusMultiplier, SE::StatusMultiplierBuff, SE::StatusMultiplierDebuff, TEXT("Status Multiplier"), true},
		{ECrystalType::EfficiencyStone, ESubStat::Efficiency, IE::BuffEfficiency, SE::EfficiencyBuff, SE::EfficiencyDebuff, TEXT("Efficiency"), true},
		{ECrystalType::MaxHPStone, ESubStat::None, IE::BuffMaxHP, SE::MaxHPBuff, SE::MaxHPDebuff, TEXT("Max HP"), true},
		{ECrystalType::MaxEPStone, ESubStat::None, IE::BuffMaxEP, SE::MaxEnergyBuff, SE::MaxEnergyDebuff, TEXT("Max EP"), true},
		{ECrystalType::SpellDamageStone, ESubStat::SpellDamage, IE::BuffSpellDamage, SE::SpellDamageBuff, SE::SpellDamageDebuff, TEXT("Spell Damage"), true},
		// Blanket, signed: ally +percent, enemy -percent on ModifyStatusResist (ExecuteResistanceBuffEffect).
		{ECrystalType::ResistanceStone, ESubStat::Resistance, IE::BuffResistance, SE::ModifyStatusResist, SE::ModifyStatusResist, TEXT("Status Resistance"), false},
		{ECrystalType::SpellSpeedStone, ESubStat::SpellSpeed, IE::BuffSpellSpeed, SE::SpellSpeedBuff, SE::SpellSpeedDebuff, TEXT("Spell Speed"), true},
		{ECrystalType::ActionSpeedStone, ESubStat::ActionSpeed, IE::BuffActionSpeed, SE::ActionSpeedBuff, SE::ActionSpeedDebuff, TEXT("Action Speed"), true},
		{ECrystalType::LuckStone, ESubStat::Luck, IE::BuffLuck, SE::LuckBuff, SE::LuckDebuff, TEXT("Luck"), true},
		{ECrystalType::ReflexStone, ESubStat::Reflex, IE::BuffReflex, SE::ReflexBuff, SE::ReflexDebuff, TEXT("Reflex"), true},
	};

	/** The stat stone's row, or nullptr for anything else (gems, AbilityStone, DurabilityStone, HealingStone). */
	inline const FStoneRow *Find(ECrystalType Type)
	{
		for (const FStoneRow &Row : ROWS)
		{
			if (Row.Type == Type)
			{
				return &Row;
			}
		}
		return nullptr;
	}
}
