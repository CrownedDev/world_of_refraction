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
			return Condition.PriorMemory && Context.Journal && Context.Journal->HasMemory(Condition.PriorMemory);
		}
		if (Condition.Subject == EWoRConditionSubject::FacingEnemy)
		{
			return Condition.Enemy && Context.Opposing.Contains(Condition.Enemy.Get());
		}
		if (Condition.Subject == EWoRConditionSubject::Hp)
		{
			const FWoRFighterState *Fighter = Condition.Who == EWoRConditionWho::Target ? Context.Target : Context.Self;
			return Fighter && WoRConditions::Compare(Fighter->CurrentHP, Condition.Operator, Condition.Value);
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
	case EWoRConditionOperator::AtLeast:
	default:                            return Actual >= Value;
	}
}

bool WoRConditions::IsMet(const FWoRCondition &Condition, const FWoRConditionContext &Context)
{
	if (Condition.Trigger != EWoRConditionTrigger::Any && Condition.Trigger != Context.Trigger)
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

FWoRConditionContext WoRConditions::MakeContext(const FMemoryEvent &Event, const UMemoryJournalSubsystem *Journal)
{
	FWoRConditionContext Context;
	Context.Trigger = TriggerFor(Event.Source);
	Context.Event = &Event;
	Context.Journal = Journal;
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
	Out.Operator = EWoRConditionOperator::AtLeast;
	Out.Value = Condition.UpgradeLevel;
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
	if (Condition.Subject == EWoRConditionSubject::Hp)
	{
		Out += TEXT(" ") + ConditionEnumName(Condition.Who);
	}
	if (Condition.Subject == EWoRConditionSubject::GearEquipped || Condition.Subject == EWoRConditionSubject::EvolutionEquipped ||
		Condition.Subject == EWoRConditionSubject::Hp)
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
