// BreakTable.cpp

#include "Skills/Effects/BreakTable.h"
#include "Combat/CombatConstants.h"

namespace
{
	TArray<FBreakRow> BuildBreakRows()
	{
		using S = ESkillEffectType;
		using T = ESkillEffectTiming;
		using Z = EBreakSize;
		constexpr float GATE = BreakTable::BREAK_GATE_VALUE;
		// B-170h2b: one table; the action-pips rows (B-170d, Q-170e) replaced the old timeline's Water, Wind and Ice.
		// Trigger,              Applied,                SizeKind,                Size,                                                 Timing,             DelayFirst, AlsoApplied,         Damage, LostNow, LostNextTurn
		return {
			{S::DOT,               S::DOT,               Z::ShareOfMaxHP,         CombatConstants::BURN_BREAK_MAX_HP_FRACTION,           T::StartOfOwnTurn,  true,       S::None},
			{S::DefenseDebuff,     S::DefenseDebuff,     Z::Percent,              CombatConstants::EARTH_BREAK_DEFENSE_DOWN_PERCENT,     T::Persistent,      false,      S::None},
			{S::SkipTurn,          S::ActionSpeedDebuff, Z::Percent,              CombatConstants::WIND_BREAK_ACTION_SPEED_SLOW_PERCENT, T::Persistent,      false,      S::None,             0.0f,   0,       CombatConstants::WIND_BREAK_ACTIONS_LOST},
			{S::Stun,              S::Stun,              Z::Gate,                 GATE,                                                  T::StartOfOwnTurn,  false,      S::None},
			{S::HealBlock,         S::HealBlock,         Z::Gate,                 GATE,                                                  T::Persistent,      false,      S::None},
			{S::Silenced,          S::Silenced,          Z::Gate,                 GATE,                                                  T::Persistent,      false,      S::None},
			{S::RandomSkill,       S::RandomSkill,       Z::Gate,                 GATE,                                                  T::StartOfOwnTurn,  false,      S::None},
			{S::CritChanceDebuff,  S::CritChanceDebuff,  Z::Percent,              CombatConstants::LIGHT_BREAK_CRIT_CHANCE_DOWN_PERCENT, T::Persistent,      false,      S::None},
			{S::ActionSpeedDebuff, S::SpellSpeedDebuff,  Z::Percent,              CombatConstants::WATER_BREAK_SPEED_SLOW_PERCENT,       T::Persistent,      false,      S::None},
			{S::DrainEnergy,       S::DrainEnergy,       Z::ShareOfMaxEP,         CombatConstants::BD_BREAK_EP_DRAIN_FRACTION,           T::StartOfOwnTurn,  true,       S::None},
			{S::BurstDamage,       S::BurstDamage,       Z::OneShotShareOfMaxHP,  CombatConstants::REALITY_BREAK_MAX_HP_FRACTION,        T::StartOfOwnTurn,  false,      S::None},
			// B-064a (D-090): Ice's break slows both speeds and deals a one-off share of max HP; B-170d takes an action.
			{S::IceBreak,          S::ActionSpeedDebuff, Z::Percent,              CombatConstants::ICE_BREAK_SPEED_SLOW_PERCENT,         T::Persistent,      false,      S::SpellSpeedDebuff,
			 CombatConstants::ICE_BREAK_DAMAGE_MAX_HP_FRACTION, CombatConstants::ICE_BREAK_ACTIONS_LOST, 0},
		};
	}

	FString BreakEnumName(ESkillEffectType Type)
	{
		FString Name = UEnum::GetValueAsString(Type);
		Name.RemoveFromStart(TEXT("ESkillEffectType::"));
		return Name;
	}
}

namespace BreakTable
{
	const TArray<FBreakRow> &Rows()
	{
		static const TArray<FBreakRow> BreakRows = BuildBreakRows();
		return BreakRows;
	}

	const FBreakRow *For(ESkillEffectType Trigger)
	{
		return Rows().FindByPredicate([Trigger](const FBreakRow &Row) { return Row.Trigger == Trigger; });
	}

	FString GetBreakTableString()
	{
		static const TCHAR *SIZE_KIND_NAMES[] = {TEXT("gate"), TEXT("percent"), TEXT("share of max HP"), TEXT("share of max EP"),
												 TEXT("one-shot share of max HP")};
		static_assert(UE_ARRAY_COUNT(SIZE_KIND_NAMES) == static_cast<size_t>(EBreakSize::OneShotShareOfMaxHP) + 1,
					  "one name per EBreakSize");
		const auto RowLine = [](const FBreakRow &Row)
		{
			return FString::Printf(TEXT("  %-18s -> %-18s %s %.2f, %s%s%s%s%s%s\n"), *BreakEnumName(Row.Trigger), *BreakEnumName(Row.Applied),
								   SIZE_KIND_NAMES[static_cast<uint8>(Row.SizeKind)], Row.Size,
								   *UEnum::GetValueAsString(Row.Timing).Replace(TEXT("ESkillEffectTiming::"), TEXT("")), Row.bDelayFirstExecution ? TEXT(", from next turn") : TEXT(""),
								   Row.AlsoApplied != ESkillEffectType::None ? *(TEXT(", also ") + BreakEnumName(Row.AlsoApplied)) : TEXT(""),
								   Row.DamageShareOfMaxHP > 0.0f ? *FString::Printf(TEXT(", deals %.2f of max HP"), Row.DamageShareOfMaxHP) : TEXT(""),
								   Row.ActionsLostNow > 0 ? *FString::Printf(TEXT(", -%d action(s) at once"), Row.ActionsLostNow) : TEXT(""),
								   Row.ActionsLostNextTurn > 0 ? *FString::Printf(TEXT(", -%d action(s) next turn"), Row.ActionsLostNextTurn) : TEXT(""));
		};
		FString Out = FString::Printf(TEXT("Break table (%d rows)\n"), Rows().Num());
		for (const FBreakRow &Row : Rows())
		{
			Out += RowLine(Row);
		}
		return Out;
	}
}
