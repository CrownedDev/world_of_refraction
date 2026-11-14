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
		// Trigger,              Applied,                SizeKind,                Size,                                                 Timing,             DelayFirst, AlsoApplied,           DelayTurnPlaces
		return {
			{S::DOT,               S::DOT,               Z::ShareOfMaxHP,         CombatConstants::BURN_BREAK_MAX_HP_FRACTION,           T::StartOfOwnTurn,  true,       S::None,               0},
			{S::DefenseDebuff,     S::DefenseDebuff,     Z::Percent,              CombatConstants::EARTH_BREAK_DEFENSE_DOWN_PERCENT,     T::Persistent,      false,      S::None,               0},
			{S::SkipTurn,          S::SkipTurn,          Z::Gate,                 GATE,                                                  T::StartOfOwnTurn,  false,      S::None,               CombatConstants::WIND_BREAK_DELAY_PLACES},
			{S::Stun,              S::Stun,              Z::Gate,                 GATE,                                                  T::StartOfOwnTurn,  false,      S::None,               0},
			{S::HealBlock,         S::HealBlock,         Z::Gate,                 GATE,                                                  T::Persistent,      false,      S::None,               0},
			{S::Silenced,          S::Silenced,          Z::Gate,                 GATE,                                                  T::Persistent,      false,      S::None,               0},
			{S::RandomSkill,       S::RandomSkill,       Z::Gate,                 GATE,                                                  T::StartOfOwnTurn,  false,      S::None,               0},
			{S::CritChanceDebuff,  S::CritChanceDebuff,  Z::Percent,              CombatConstants::LIGHT_BREAK_CRIT_CHANCE_DOWN_PERCENT, T::Persistent,      false,      S::None,               0},
			{S::ActionSpeedDebuff, S::ActionSpeedDebuff, Z::Percent,              CombatConstants::WATER_BREAK_SPEED_SLOW_PERCENT,       T::Persistent,      false,      S::SpellSpeedDebuff,   0},
			{S::DrainEnergy,       S::DrainEnergy,       Z::ShareOfMaxEP,         CombatConstants::BD_BREAK_EP_DRAIN_FRACTION,           T::StartOfOwnTurn,  true,       S::None,               0},
			{S::BurstDamage,       S::BurstDamage,       Z::OneShotShareOfMaxHP,  CombatConstants::REALITY_BREAK_MAX_HP_FRACTION,        T::StartOfOwnTurn,  false,      S::None,               0},
			// B-064a (D-090): Ice's break is Water's slow plus a one-off share of max HP (DamageShareOfMaxHP, the last column).
			{S::IceBreak,          S::ActionSpeedDebuff, Z::Percent,              CombatConstants::ICE_BREAK_SPEED_SLOW_PERCENT,         T::Persistent,      false,      S::SpellSpeedDebuff,   0,
			 CombatConstants::ICE_BREAK_DAMAGE_MAX_HP_FRACTION},
		};
	}

	/** B-170d (Q-170e): the rows an action-pips fight reads instead. The last two columns: actions lost at once, at the
	 *  holder's next turn start. */
	TArray<FBreakRow> BuildActionPipsRows()
	{
		using S = ESkillEffectType;
		using T = ESkillEffectTiming;
		using Z = EBreakSize;
		// Trigger,              Applied,                SizeKind,    Size,                                                    Timing,          DelayFirst, AlsoApplied,         DelayTurnPlaces, Damage, LostNow, LostNextTurn
		return {
			{S::ActionSpeedDebuff, S::SpellSpeedDebuff,  Z::Percent,  CombatConstants::WATER_BREAK_SPEED_SLOW_PERCENT,          T::Persistent,   false,      S::None,             0,               0.0f,   0,       0},
			{S::SkipTurn,          S::ActionSpeedDebuff, Z::Percent,  CombatConstants::WIND_BREAK_ACTION_SPEED_SLOW_PERCENT,    T::Persistent,   false,      S::None,             0,               0.0f,   0,       CombatConstants::WIND_BREAK_ACTIONS_LOST},
			{S::IceBreak,          S::ActionSpeedDebuff, Z::Percent,  CombatConstants::ICE_BREAK_SPEED_SLOW_PERCENT,            T::Persistent,   false,      S::SpellSpeedDebuff, 0,
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

	const TArray<FBreakRow> &ActionPipsRows()
	{
		static const TArray<FBreakRow> PipRows = BuildActionPipsRows();
		return PipRows;
	}

	const FBreakRow *For(ESkillEffectType Trigger, bool bActionPips)
	{
		const auto IsTrigger = [Trigger](const FBreakRow &Row) { return Row.Trigger == Trigger; };
		if (bActionPips)
		{
			if (const FBreakRow *PipRow = ActionPipsRows().FindByPredicate(IsTrigger))
			{
				return PipRow;
			}
		}
		return Rows().FindByPredicate(IsTrigger);
	}

	FString GetBreakTableString()
	{
		static const TCHAR *SIZE_KIND_NAMES[] = {TEXT("gate"), TEXT("percent"), TEXT("share of max HP"), TEXT("share of max EP"),
												 TEXT("one-shot share of max HP")};
		static_assert(UE_ARRAY_COUNT(SIZE_KIND_NAMES) == static_cast<size_t>(EBreakSize::OneShotShareOfMaxHP) + 1,
					  "one name per EBreakSize");
		const auto RowLine = [](const FBreakRow &Row)
		{
			return FString::Printf(TEXT("  %-18s -> %-18s %s %.2f, %s%s%s%s%s%s%s\n"), *BreakEnumName(Row.Trigger), *BreakEnumName(Row.Applied),
								   SIZE_KIND_NAMES[static_cast<uint8>(Row.SizeKind)], Row.Size,
								   *UEnum::GetValueAsString(Row.Timing).Replace(TEXT("ESkillEffectTiming::"), TEXT("")), Row.bDelayFirstExecution ? TEXT(", from next turn") : TEXT(""),
								   Row.AlsoApplied != ESkillEffectType::None ? *(TEXT(", also ") + BreakEnumName(Row.AlsoApplied)) : TEXT(""),
								   Row.DelayTurnPlaces > 0 ? *FString::Printf(TEXT(", delays %d"), Row.DelayTurnPlaces) : TEXT(""),
								   Row.DamageShareOfMaxHP > 0.0f ? *FString::Printf(TEXT(", deals %.2f of max HP"), Row.DamageShareOfMaxHP) : TEXT(""),
								   Row.ActionsLostNow > 0 ? *FString::Printf(TEXT(", -%d action(s) at once"), Row.ActionsLostNow) : TEXT(""),
								   Row.ActionsLostNextTurn > 0 ? *FString::Printf(TEXT(", -%d action(s) next turn"), Row.ActionsLostNextTurn) : TEXT(""));
		};
		FString Out = FString::Printf(TEXT("Break table (%d rows)\n"), Rows().Num());
		for (const FBreakRow &Row : Rows())
		{
			Out += RowLine(Row);
		}
		// B-170d: what an action-pips fight reads instead.
		Out += FString::Printf(TEXT("In a fight, with action pips (%d rows differ)\n"), ActionPipsRows().Num());
		for (const FBreakRow &Row : ActionPipsRows())
		{
			Out += RowLine(Row);
		}
		return Out;
	}
}
