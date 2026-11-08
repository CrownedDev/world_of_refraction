// WoRConditions.cpp

#include "Conditions/WoRConditions.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Character/CharacterData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/EquipmentDataBase.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Memory/CurseSubsystem.h"
#include "Memory/HiddenCounterSubsystem.h"
#include "Engine/GameInstance.h"
#include "Memory/MemoryUnlock.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/WorldStatRequirements.h"

namespace
{
	/** An enum value's name without its type prefix ("GearEquipped"). */
	template <class TEnum>
	FString ConditionEnumName(TEnum Value)
	{
		const UEnum *Enum = StaticEnum<TEnum>();
		return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Value)) : FString::FromInt(static_cast<int32>(Value));
	}

	bool IsMemberInParty(const UCharacterData *Character, const FMemoryEvent &Event)
	{
		return Event.Party.ContainsByPredicate([Character](const FMemoryEventMember &Member) { return Member.Character == Character; });
	}

	/** B-376: Character is in the event's whole team (Present), or its party when Present was never filled. */
	bool IsPresent(const UCharacterData *Character, const FMemoryEvent &Event)
	{
		return Event.Present.IsEmpty() ? IsMemberInParty(Character, Event) : Event.Present.Contains(Character);
	}

	/** Some party member (Condition.Character when set) satisfies Holds. */
	template <class TPredicate>
	bool AnyMember(const FWoRCondition &Condition, const FMemoryEvent &Event, TPredicate Holds)
	{
		return Event.Party.ContainsByPredicate([&](const FMemoryEventMember &Member)
		{
			return (!Condition.Character || Member.Character == Condition.Character) && Holds(Member);
		});
	}

	/** A level-up's wearer can be Condition.Character: whoever levelled it, or (unknown) anyone in the party. */
	bool IsUpgradedBy(const FWoRCondition &Condition, const FMemoryEvent &Event)
	{
		return !Condition.Character || (Event.UpgradedBy ? Event.UpgradedBy == Condition.Character : IsMemberInParty(Condition.Character, Event));
	}

	/** Who levelled it, as far as requirements go (unknown and no character named: null). */
	const UCharacterData *GetUpgrader(const FWoRCondition &Condition, const FMemoryEvent &Event)
	{
		return Event.UpgradedBy ? Event.UpgradedBy : Condition.Character.Get();
	}

	/** B-381 activations: the bound item's socket, stone, tier and upgrade level against Condition (WornItemState). */
	bool IsWornItemStateMet(const FWoRCondition &Condition, const FWornItemState &Item)
	{
		const bool bAnyElement = Condition.SocketElement == ESpellElement::None;
		return (bAnyElement || Item.SocketElement == Condition.SocketElement) &&
			   (!Condition.Evolution || Item.SocketEvolution == Condition.Evolution) &&
			   (Condition.StoneType == ECrystalType::None || Item.StoneType == Condition.StoneType) &&
			   static_cast<uint8>(Item.Tier) >= static_cast<uint8>(Condition.MinTier) &&
			   WoRConditions::Compare(Item.UpgradeSteps, Condition.Operator, Condition.Value);
	}

	bool IsGearMet(const FWoRCondition &Condition, const FMemoryEvent &Event)
	{
		if (!Condition.Item)
		{
			return false;
		}
		const auto IsLevelled = [&Condition](EItemTier Tier)
		{
			return WoRConditions::Compare(MemoryUnlock::GetUpgradeSteps(*Condition.Item, Tier), Condition.Operator, Condition.Value);
		};
		const bool bWorn = AnyMember(Condition, Event, [&](const FMemoryEventMember &Member)
		{
			return Member.Worn.ContainsByPredicate([&](const TPair<const UEquipmentDataBase *, EItemTier> &Worn)
			{
				return Worn.Key == Condition.Item && IsLevelled(Worn.Value) &&
					   WoRConditions::FitsRequirements(Condition, &Condition.Item->Requirements, Member.Character);
			});
		});
		// A gear upgrade: the levelled item counts as worn by whoever levelled it (unknown: anyone).
		return bWorn || (Event.UpgradedItem == Condition.Item && IsLevelled(Event.UpgradedTier) && IsUpgradedBy(Condition, Event) &&
						 WoRConditions::FitsRequirements(Condition, &Condition.Item->Requirements, GetUpgrader(Condition, Event)));
	}

	bool IsEvolutionMet(const FWoRCondition &Condition, const FMemoryEvent &Event)
	{
		if (!Condition.Evolution)
		{
			return false;
		}
		const auto IsLevelled = [&Condition](EItemTier Tier)
		{
			return WoRConditions::Compare(static_cast<int32>(Tier) - static_cast<int32>(Condition.Evolution->Tier), Condition.Operator,
										  Condition.Value);
		};
		if (AnyMember(Condition, Event, [&](const FMemoryEventMember &Member)
		{
			return Member.Evolution == Condition.Evolution && IsLevelled(Member.EvolutionTier) &&
				   WoRConditions::FitsRequirements(Condition, nullptr, Member.Character);
		}))
		{
			return true;
		}
		// An evolution level-up, as for gear.
		return Event.UpgradedEvolution == Condition.Evolution && IsLevelled(Event.UpgradedTier) && IsUpgradedBy(Condition, Event) &&
			   WoRConditions::FitsRequirements(Condition, nullptr, GetUpgrader(Condition, Event));
	}

	/** B-355: the boss subjects; false without a boss state. */
	bool IsBossSubjectMet(const FWoRCondition &Condition, const FWoRBossState *Boss)
	{
		constexpr int32 PERCENT_SCALE = 100;
		if (!Boss)
		{
			return false;
		}
		switch (Condition.Subject)
		{
		case EWoRConditionSubject::BossHPAtMostPercent:
			return Boss->MaxHP > 0 && static_cast<int64>(Boss->CurrentHP) * PERCENT_SCALE <= static_cast<int64>(Condition.Value) * Boss->MaxHP;
		case EWoRConditionSubject::BossTurnsTaken:
			return WoRConditions::Compare(Boss->TurnsTaken, Condition.Operator, Condition.Value);
		case EWoRConditionSubject::BossAbsorptionMaxed:
			return Boss->bAbsorptionMaxed;
		case EWoRConditionSubject::BossCrystalBroken:
			return Boss->bCrystalBroken;
		case EWoRConditionSubject::BossStatusBreaks:
			return WoRConditions::Compare(Boss->StatusBreaks, Condition.Operator, Condition.Value);
		case EWoRConditionSubject::BossPhasesReached:
			return WoRConditions::Compare(Boss->PhasesReached, Condition.Operator, Condition.Value);
		case EWoRConditionSubject::BossInfusionHits:
		default:
			return false;
		}
	}

	bool IsBossSubject(EWoRConditionSubject Subject)
	{
		return Subject >= EWoRConditionSubject::BossHPAtMostPercent && Subject <= EWoRConditionSubject::BossPhasesReached;
	}

	/** B-337b / B-337d: the subjects that read a fighter's state (FWoRConditionContext's Self / Target / teams). */
	bool IsFighterSubject(EWoRConditionSubject Subject)
	{
		return Subject == EWoRConditionSubject::Hp || Subject == EWoRConditionSubject::HpPercent ||
			   Subject == EWoRConditionSubject::EpPercent || Subject == EWoRConditionSubject::Defender;
	}

	bool IsFighterSubjectMet(const FWoRCondition &Condition, const FWoRFighterState &Fighter)
	{
		switch (Condition.Subject)
		{
		case EWoRConditionSubject::Hp:
			return WoRConditions::Compare(Fighter.CurrentHP, Condition.Operator, Condition.Value);
		case EWoRConditionSubject::HpPercent:
			return WoRConditions::ComparePercent(Fighter.CurrentHP, Fighter.MaxHP, Condition.Operator, Condition.Value);
		case EWoRConditionSubject::EpPercent:
			return WoRConditions::ComparePercent(Fighter.CurrentEP, Fighter.MaxEP, Condition.Operator, Condition.Value);
		case EWoRConditionSubject::Defender:
			return Fighter.bDefender;
		default:
			return false;
		}
	}

	/** Some fighter Who names holds (none in the context: false). */
	template <class TPredicate>
	bool AnyFighter(EWoRConditionWho Who, const FWoRConditionContext &Context, TPredicate Holds)
	{
		switch (Who)
		{
		case EWoRConditionWho::Target:     return Context.Target && Holds(*Context.Target);
		case EWoRConditionWho::SelfTeam:   return Context.SelfTeam.ContainsByPredicate(Holds);
		case EWoRConditionWho::TargetTeam: return Context.TargetTeam.ContainsByPredicate(Holds);
		case EWoRConditionWho::Self:
		default:                           return Context.Self && Holds(*Context.Self);
		}
	}

	bool IsSubjectMet(const FWoRCondition &Condition, const FWoRConditionContext &Context)
	{
		if (Condition.Subject == EWoRConditionSubject::None)
		{
			return true;
		}
		if (IsBossSubject(Condition.Subject))
		{
			return IsBossSubjectMet(Condition, Context.Boss);
		}
		if (Condition.Subject == EWoRConditionSubject::MemoryCollected)
		{
			if (Context.KnownMemoryIds)
			{
				return Condition.PriorMemory && Context.KnownMemoryIds->Contains(Condition.PriorMemory->GetMemoryId()); // B-235e
			}
			return Condition.PriorMemory && Context.Journal && Context.Journal->HasMemory(Condition.PriorMemory);
		}
		if (Condition.Subject == EWoRConditionSubject::FlawlessParry)
		{
			return Context.bFlawlessParry;
		}
		if (Condition.Subject == EWoRConditionSubject::FacingEnemy)
		{
			return Condition.Enemy && Context.Opposing.Contains(Condition.Enemy.Get());
		}
		if (IsFighterSubject(Condition.Subject))
		{
			return AnyFighter(Condition.Who, Context, [&Condition](const FWoRFighterState &Fighter)
			{
				return IsFighterSubjectMet(Condition, Fighter);
			});
		}
		if (Condition.Subject == EWoRConditionSubject::Disabled)
		{
			return false;
		}
		if (Condition.Subject == EWoRConditionSubject::WornItemState)
		{
			return Context.BoundItem && IsWornItemStateMet(Condition, *Context.BoundItem); // B-381: reads the item, not the event
		}
		if (!Context.Event)
		{
			return false; // every other subject reads the event
		}
		const FMemoryEvent &Event = *Context.Event;
		switch (Condition.Subject)
		{
		case EWoRConditionSubject::CharacterInParty:
			return Condition.Character && IsMemberInParty(Condition.Character, Event);
		case EWoRConditionSubject::CharacterPresent:
			return Condition.Character && IsPresent(Condition.Character, Event);
		case EWoRConditionSubject::CurseHeld:
			return !Condition.CurseId.IsNone() && Context.HeldCurses && Context.HeldCurses->Contains(Condition.CurseId);
		case EWoRConditionSubject::CounterReached:
			return !Condition.CounterId.IsNone() &&
				   WoRConditions::Compare(Context.Counters ? Context.Counters->Get(Condition.CounterId, Condition.Character) : 0, Condition.Operator, Condition.Value);
		case EWoRConditionSubject::GearEquipped:
			return IsGearMet(Condition, Event);
		case EWoRConditionSubject::AtLocation:
			return !Condition.LocationId.IsNone() && Event.Locations.Contains(Condition.LocationId);
		case EWoRConditionSubject::EnemyBeaten:
			return Condition.Enemy && Event.Beaten.Contains(Condition.Enemy.Get());
		case EWoRConditionSubject::NoDamageTaken:
			return Event.bNoDamageTaken;
		case EWoRConditionSubject::Hidden:
			return false;
		case EWoRConditionSubject::EvolutionEquipped:
			return IsEvolutionMet(Condition, Event);
		case EWoRConditionSubject::SkillSlotted:
			return Condition.Skill && AnyMember(Condition, Event, [&](const FMemoryEventMember &Member)
			{
				return Member.Slotted.Contains(Condition.Skill.Get()) &&
					   WoRConditions::FitsRequirements(Condition, &Condition.Skill->Requirements, Member.Character);
			});
		case EWoRConditionSubject::SkillUsed:
			return Condition.Skill && AnyMember(Condition, Event, [&](const FMemoryEventMember &Member) { return Member.Used.Skills.Contains(Condition.Skill.Get()); });
		case EWoRConditionSubject::BreakthroughUsed:
			return Condition.Evolution &&
				   AnyMember(Condition, Event, [&](const FMemoryEventMember &Member) { return Member.Used.Breakthroughs.Contains(Condition.Evolution.Get()); });
		case EWoRConditionSubject::FinishingBlow:
			return Condition.Item &&
				   AnyMember(Condition, Event, [&](const FMemoryEventMember &Member) { return Member.Used.FinishingWeapons.Contains(Condition.Item.Get()); });
		default:
			return false;
		}
	}

	EWoRConditionSubject SubjectFor(EMemoryConditionKind Kind)
	{
		switch (Kind)
		{
		case EMemoryConditionKind::Character:         return EWoRConditionSubject::CharacterInParty;
		case EMemoryConditionKind::GearEquipped:      return EWoRConditionSubject::GearEquipped;
		case EMemoryConditionKind::Location:          return EWoRConditionSubject::AtLocation;
		case EMemoryConditionKind::EnemyBeaten:       return EWoRConditionSubject::EnemyBeaten;
		case EMemoryConditionKind::NoDamageTaken:     return EWoRConditionSubject::NoDamageTaken;
		case EMemoryConditionKind::PriorMemory:       return EWoRConditionSubject::MemoryCollected;
		case EMemoryConditionKind::Hidden:            return EWoRConditionSubject::Hidden;
		case EMemoryConditionKind::EvolutionEquipped: return EWoRConditionSubject::EvolutionEquipped;
		case EMemoryConditionKind::SkillSlotted:      return EWoRConditionSubject::SkillSlotted;
		case EMemoryConditionKind::SkillUsed:         return EWoRConditionSubject::SkillUsed;
		case EMemoryConditionKind::BreakthroughUsed:  return EWoRConditionSubject::BreakthroughUsed;
		case EMemoryConditionKind::FinishingBlow:     return EWoRConditionSubject::FinishingBlow;
		case EMemoryConditionKind::PartyHas:          return EWoRConditionSubject::CharacterPresent;
		case EMemoryConditionKind::CurseHeld:         return EWoRConditionSubject::CurseHeld;
		case EMemoryConditionKind::CounterReached:    return EWoRConditionSubject::CounterReached;
		}
		return EWoRConditionSubject::Hidden; // an unknown kind never holds, as before
	}

	EWoRRequirementFit FitFor(EMemoryRequirementFit Fit)
	{
		switch (Fit)
		{
		case EMemoryRequirementFit::Met:   return EWoRRequirementFit::Met;
		case EMemoryRequirementFit::Above: return EWoRRequirementFit::Above;
		case EMemoryRequirementFit::Under: return EWoRRequirementFit::Under;
		case EMemoryRequirementFit::Any:
		default:                           return EWoRRequirementFit::Any;
		}
	}
}

bool WoRConditions::Compare(int32 Actual, EWoRConditionOperator Operator, int32 Value)
{
	switch (Operator)
	{
	case EWoRConditionOperator::AtMost: return Actual <= Value;
	case EWoRConditionOperator::Equal:  return Actual == Value;
	case EWoRConditionOperator::Below:  return Actual < Value;
	case EWoRConditionOperator::Above:  return Actual > Value;
	case EWoRConditionOperator::AtLeast:
	default:                            return Actual >= Value;
	}
}

bool WoRConditions::ComparePercent(int32 Current, int32 Max, EWoRConditionOperator Operator, int32 Percent)
{
	constexpr int64 PERCENT_SCALE = 100;
	if (Max <= 0)
	{
		return Compare(0, Operator, Percent);
	}
	// Current / Max * 100 against Percent, cross-multiplied so no fraction rounds.
	const int64 Scaled = static_cast<int64>(Current) * PERCENT_SCALE;
	const int64 Bound = static_cast<int64>(Percent) * Max;
	switch (Operator)
	{
	case EWoRConditionOperator::AtMost: return Scaled <= Bound;
	case EWoRConditionOperator::Equal:  return Scaled == Bound;
	case EWoRConditionOperator::Below:  return Scaled < Bound;
	case EWoRConditionOperator::Above:  return Scaled > Bound;
	case EWoRConditionOperator::AtLeast:
	default:                            return Scaled >= Bound;
	}
}

bool WoRConditions::IsMet(const FWoRCondition &Condition, const FWoRConditionContext &Context)
{
	if (Condition.Trigger != EWoRConditionTrigger::Any && Condition.Trigger != Context.Trigger &&
		!Context.AlsoFiring.Contains(Condition.Trigger))
	{
		return false;
	}
	return IsSubjectMet(Condition, Context);
}

bool WoRConditions::IsMet(const FWoRConditionSet &Set, const FWoRConditionContext &Context)
{
	return Algo::AllOf(Set.Conditions, [&Context](const FWoRCondition &Condition) { return IsMet(Condition, Context); });
}

bool WoRConditions::IsAnyMet(const TArray<FWoRConditionSet> &Sets, const FWoRConditionContext &Context)
{
	return Sets.IsEmpty() || Sets.ContainsByPredicate([&Context](const FWoRConditionSet &Set) { return IsMet(Set, Context); });
}

bool WoRConditions::FitsRequirements(const FWoRCondition &Condition, const FWorldStatRequirements *Requirements,
									  const UCharacterData *Wearer)
{
	if (Condition.RequirementFit == EWoRRequirementFit::Any)
	{
		return true;
	}
	if (!Wearer)
	{
		return false;
	}
	if (!Requirements || !Requirements->HasRequirements())
	{
		return Condition.RequirementFit == EWoRRequirementFit::Met;
	}
	// Per pillar: the wearer's world level and what is required. TODO: the asset's levels (the D-082
	// gap); switch to the run's live stats so a pillar raised mid-run counts.
	const TPair<int32, int32> Pillars[] = {{Wearer->WorldMindLevel, Requirements->RequiredWorldMind},
										   {Wearer->WorldBodyLevel, Requirements->RequiredWorldBody},
										   {Wearer->WorldSpiritLevel, Requirements->RequiredWorldSpirit}};
	switch (Condition.RequirementFit)
	{
	case EWoRRequirementFit::Met:
		return Requirements->MeetsRequirements(Wearer);
	case EWoRRequirementFit::Above:
		return Requirements->MeetsRequirements(Wearer) && Algo::AllOf(Pillars, [&Condition](const TPair<int32, int32> &Pillar)
		{
			return Pillar.Value == 0 || Pillar.Key - Pillar.Value >= Condition.RequirementMargin;
		});
	case EWoRRequirementFit::Under:
	{
		const int32 Short = FMath::Max(Condition.RequirementMargin, 1);
		return Algo::AnyOf(Pillars, [Short](const TPair<int32, int32> &Pillar) { return Pillar.Value - Pillar.Key >= Short; });
	}
	case EWoRRequirementFit::Any:
	default:
		return true;
	}
}

EWoRConditionTrigger WoRConditions::TriggerFor(EMemorySource Source)
{
	switch (Source)
	{
	case EMemorySource::Fragment:  return EWoRConditionTrigger::FragmentFound;
	case EMemorySource::Boss:      return EWoRConditionTrigger::BossBeaten;
	case EMemorySource::Reaction:  return EWoRConditionTrigger::ReactionFired;
	case EMemorySource::MapSpot:   return EWoRConditionTrigger::MapSpotReached;
	case EMemorySource::BoundGear: return EWoRConditionTrigger::BoundGearUsed;
	case EMemorySource::FightWon:  return EWoRConditionTrigger::FightWon;
	}
	return EWoRConditionTrigger::Any;
}

FWoRConditionContext WoRConditions::MakeFlawlessParryContext(const FWoRFighterState *Self, const FWoRFighterState *Target)
{
	FWoRConditionContext Context;
	Context.Trigger = EWoRConditionTrigger::FlawlessParry;
	Context.bFlawlessParry = true;
	Context.Self = Self;
	Context.Target = Target;
	return Context;
}

FWoRConditionContext WoRConditions::MakeContext(const FMemoryEvent &Event, const UMemoryJournalSubsystem *Journal)
{
	FWoRConditionContext Context;
	Context.Trigger = TriggerFor(Event.Source);
	Context.Event = &Event;
	Context.Journal = Journal;
	// B-378: this player's curses, from the same game instance as the journal.
	const UGameInstance *GameInstance = Journal ? Journal->GetGameInstance() : nullptr;
	const UCurseSubsystem *Curses = GameInstance ? GameInstance->GetSubsystem<UCurseSubsystem>() : nullptr;
	Context.HeldCurses = Curses ? &Curses->GetHeldCurses() : nullptr;
	Context.Counters = GameInstance ? GameInstance->GetSubsystem<UHiddenCounterSubsystem>() : nullptr; // B-379
	return Context;
}

FWoRCondition WoRConditions::FromMemory(const FMemoryCondition &Condition)
{
	FWoRCondition Out;
	Out.Trigger = EWoRConditionTrigger::Any; // a memory checks its conditions at every event
	Out.Subject = SubjectFor(Condition.Kind);
	Out.Character = Condition.Character;
	Out.Item = Condition.Item;
	Out.Evolution = Condition.Evolution;
	Out.Skill = Condition.Skill;
	Out.Enemy = Condition.Enemy;
	Out.PriorMemory = Condition.PriorMemory;
	Out.LocationId = Condition.LocationId;
	Out.CurseId = Condition.CurseId; // B-378
	Out.Operator = EWoRConditionOperator::AtLeast;
	Out.Value = Condition.UpgradeLevel;
	if (Condition.Kind == EMemoryConditionKind::CounterReached)
	{
		Out.CounterId = Condition.CounterId; // B-379
		Out.Value = Condition.CounterTarget;
	}
	Out.RequirementFit = FitFor(Condition.RequirementFit);
	Out.RequirementMargin = Condition.RequirementMargin;
	return Out;
}

FWoRConditionSet WoRConditions::FromMemory(const TArray<FMemoryCondition> &Conditions)
{
	FWoRConditionSet Set;
	for (const FMemoryCondition &Condition : Conditions)
	{
		Set.Conditions.Add(FromMemory(Condition));
	}
	return Set;
}

FString WoRConditions::GetConditionString(const FWoRCondition &Condition)
{
	FString Out = ConditionEnumName(Condition.Subject);
	if (Condition.Trigger != EWoRConditionTrigger::Any)
	{
		Out = FString::Printf(TEXT("on %s: %s"), *ConditionEnumName(Condition.Trigger), *Out);
	}
	const UObject *Operands[] = {Condition.Item.Get(), Condition.Evolution.Get(), Condition.Skill.Get(), Condition.Enemy.Get(), Condition.PriorMemory.Get()};
	for (const UObject *Operand : Operands)
	{
		if (Operand)
		{
			Out += TEXT(" ") + Operand->GetName();
		}
	}
	if (!Condition.LocationId.IsNone())
	{
		Out += TEXT(" ") + Condition.LocationId.ToString();
	}
	if (!Condition.CurseId.IsNone())
	{
		Out += TEXT(" ") + Condition.CurseId.ToString(); // B-378
	}
	if (Condition.Subject == EWoRConditionSubject::WornItemState) // B-381
	{
		Out += FString::Printf(TEXT(" socket %s, stone %s, tier >= %s, level %s %d"), *ConditionEnumName(Condition.SocketElement),
							   *ConditionEnumName(Condition.StoneType), *ConditionEnumName(Condition.MinTier),
							   *ConditionEnumName(Condition.Operator), Condition.Value);
		if (Condition.Evolution)
		{
			Out += TEXT(", evolution ") + Condition.Evolution->GetName();
		}
	}
	if (!Condition.CounterId.IsNone())
	{
		Out += FString::Printf(TEXT(" %s %s %d"), *Condition.CounterId.ToString(), *ConditionEnumName(Condition.Operator), Condition.Value); // B-379
	}
	if (IsFighterSubject(Condition.Subject))
	{
		Out += TEXT(" ") + ConditionEnumName(Condition.Who);
	}
	if (Condition.Subject == EWoRConditionSubject::GearEquipped || Condition.Subject == EWoRConditionSubject::EvolutionEquipped ||
		Condition.Subject == EWoRConditionSubject::Hp || Condition.Subject == EWoRConditionSubject::HpPercent ||
		Condition.Subject == EWoRConditionSubject::EpPercent)
	{
		Out += FString::Printf(TEXT(" %s %d"), *ConditionEnumName(Condition.Operator), Condition.Value);
	}
	if (Condition.RequirementFit != EWoRRequirementFit::Any)
	{
		Out += FString::Printf(TEXT(" (requirements %s %d)"), *ConditionEnumName(Condition.RequirementFit), Condition.RequirementMargin);
	}
	if (Condition.Character)
	{
		Out += FString::Printf(TEXT(", %s"), *Condition.Character->Name);
	}
	return Out;
}

FString WoRConditions::GetConditionSetString(const FWoRConditionSet &Set, const FWoRConditionContext &Context)
{
	return FString::JoinBy(Set.Conditions, TEXT(" + "), [&Context](const FWoRCondition &Condition)
	{
		return FString::Printf(TEXT("%s [%s]"), *GetConditionString(Condition), IsMet(Condition, Context) ? TEXT("met") : TEXT("not yet"));
	});
}
