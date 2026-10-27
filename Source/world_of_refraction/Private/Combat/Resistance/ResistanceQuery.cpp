// ResistanceQuery.cpp

#include "Combat/Resistance/ResistanceQuery.h"
#include "Combat/Resistance/ClassInnateResistanceTable.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "Combat/CombatConstants.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Loadout/LoadoutComponent.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "GameFramework/Actor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	USkillEffectManager *EffectManagerFor(const AActor *Target)
	{
		const UWorld *World = Target ? Target->GetWorld() : nullptr;
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<USkillEffectManager>() : nullptr;
	}

	/** The two status axes a damage type stands for (element or physical, never both). */
	void ToAxes(EWoRDamageType DamageType, ESpellElement &OutElement, EPhysicalDamageType &OutPhysical)
	{
		OutElement = ESpellElement::None;
		OutPhysical = EPhysicalDamageType::None;
		switch (DamageType)
		{
		case EWoRDamageType::Fire:      OutElement = ESpellElement::Fire; break;
		case EWoRDamageType::Water:     OutElement = ESpellElement::Water; break;
		case EWoRDamageType::Earth:     OutElement = ESpellElement::Earth; break;
		case EWoRDamageType::Wind:      OutElement = ESpellElement::Wind; break;
		case EWoRDamageType::Light:     OutElement = ESpellElement::Light; break;
		case EWoRDamageType::Darkness:  OutElement = ESpellElement::Darkness; break;
		case EWoRDamageType::Lightning: OutElement = ESpellElement::Lightning; break;
		case EWoRDamageType::Void:      OutElement = ESpellElement::Void; break;
		case EWoRDamageType::Reality:   OutElement = ESpellElement::Reality; break;
		case EWoRDamageType::Slash:     OutPhysical = EPhysicalDamageType::Slash; break;
		case EWoRDamageType::Pierce:    OutPhysical = EPhysicalDamageType::Pierce; break;
		case EWoRDamageType::Impact:    OutPhysical = EPhysicalDamageType::Impact; break;
		case EWoRDamageType::None:
		case EWoRDamageType::Ice:
		default:
			break;
		}
	}

	/** Whether a resistance buff/debuff counts toward DamageType. A physical-keyed effect counts
	 *  for its subtype only; any other effect counts for its element's type (None and Generic both
	 *  the non-elemental bucket, Q-216). */
	bool EffectMatches(const FActiveSkillEffect &Effect, EWoRDamageType DamageType)
	{
		if (Effect.PhysicalType != EPhysicalDamageType::None)
		{
			return WoRDamageType::PhysicalToDamageType(Effect.PhysicalType) == DamageType;
		}
		return WoRDamageType::ElementToDamageType(Effect.Element) == DamageType;
	}

	/** #4: typed ResistanceBuff minus ResistanceDebuff, as a fraction. */
	float EffectTypeResistance(AActor *Target, EWoRDamageType DamageType)
	{
		const USkillEffectManager *EffectMgr = EffectManagerFor(Target);
		if (!EffectMgr)
		{
			return 0.0f;
		}
		float Net = 0.0f;
		for (const FActiveSkillEffect &Effect : EffectMgr->GetEffectsByType(Target, ESkillEffectType::ResistanceBuff))
		{
			if (EffectMatches(Effect, DamageType))
			{
				Net += Effect.GetStackedValue();
			}
		}
		for (const FActiveSkillEffect &Effect : EffectMgr->GetEffectsByType(Target, ESkillEffectType::ResistanceDebuff))
		{
			if (EffectMatches(Effect, DamageType))
			{
				Net -= Effect.GetStackedValue();
			}
		}
		return Net / CombatConstants::STAT_PERCENT_DIVISOR;
	}

	/** #6: the equipped pieces' rolled FResistanceBonus for this type's column, as a fraction. */
	float GearTypeResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType Physical)
	{
		const ULoadoutComponent *Loadout = Target->FindComponentByClass<ULoadoutComponent>();
		if (!Loadout)
		{
			return 0.0f;
		}
		const FResistanceBonus Gear = Loadout->GetActiveResistanceBonus(Target);
		const ClassInnateResistanceTable::FResistanceRow GearRow{
			Gear.Fire, Gear.Water, Gear.Earth, Gear.Wind, Gear.Light, Gear.Darkness, Gear.Lightning, Gear.Void,
			Gear.Reality, Gear.Slash, Gear.Pierce, Gear.Impact};
		return (ClassInnateResistanceTable::GetElementColumn(GearRow, Element) +
				ClassInnateResistanceTable::GetPhysicalColumn(GearRow, Physical)) /
			   ClassInnateResistanceTable::RESISTANCE_PERCENT_DIVISOR;
	}
}

namespace WoRResistance
{
	float GetTypeResistance(AActor *Target, EWoRDamageType DamageType)
	{
		if (!Target)
		{
			return 0.0f;
		}
		ESpellElement Element;
		EPhysicalDamageType Physical;
		ToAxes(DamageType, Element, Physical);

		float Resist = EffectTypeResistance(Target, DamageType);                                  // #4
		Resist += ClassInnateResistanceTable::GetClassInnateResistance(Target, Element, Physical); // #5
		Resist += GearTypeResistance(Target, Element, Physical);                                  // #6
		return Resist;
	}

	float GetHitTypeResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType PhysicalType)
	{
		float Resist = GetTypeResistance(Target, WoRDamageType::ElementToDamageType(Element));
		if (PhysicalType != EPhysicalDamageType::None)
		{
			Resist += GetTypeResistance(Target, WoRDamageType::PhysicalToDamageType(PhysicalType));
		}
		return Resist;
	}

	float GetGeneralStatusResistance(AActor *Target)
	{
		const UCharacterDataComponent *TargetComp = Target ? Target->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TargetComp || !TargetComp->CharacterData)
		{
			return 0.0f;
		}
		// The Spirit stat capped alone (UNIVERSAL_STAT_CAP), then gear points and the attached stones
		// (weapon + ring, summed) multiply it toward RESISTANCE_MAX. Never negative: the amplify range
		// comes from the type and bonus layers.
		float General = TargetComp->GetCrystalResistanceStatCapped();                 // #1
		if (const ULoadoutComponent *Loadout = Target->FindComponentByClass<ULoadoutComponent>())
		{
			const FEquipmentStatBonus Bonus = Loadout->GetActiveStatBonus(Target);
			General *= (1.0f + Bonus.BonusResistance * CombatConstants::RESISTANCE_PER_POINT);  // #2
			General *= (1.0f + Loadout->GetAttachedStonePercent(ESubStat::Resistance) /
								   CombatConstants::STAT_PERCENT_DIVISOR);                       // #3
		}
		return FMath::Clamp(General, 0.0f, CombatConstants::RESISTANCE_MAX);
	}

	float GetStatusResistBonus(AActor *Target)
	{
		if (!Target)
		{
			return 0.0f;
		}
		float Bonus = 0.0f;
		// #7 signed: a negative ModifyStatusResist (the stone's enemy curse) reaches the amplify range.
		if (const USkillEffectManager *EffectMgr = EffectManagerFor(Target))
		{
			Bonus += EffectMgr->GetTotalStatModifier(Target, ESkillEffectType::ModifyStatusResist) /
					 CombatConstants::STAT_PERCENT_DIVISOR;
		}
		// #8 boss rising resistance (B-193): a step per status break this fight.
		if (const UBossPhaseComponent *Boss = Target->FindComponentByClass<UBossPhaseComponent>())
		{
			Bonus += Boss->GetStatusResistanceBonus();
		}
		return Bonus;
	}

	float GetTotalStatusResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType PhysicalType)
	{
		const UCharacterDataComponent *TargetComp = Target ? Target->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TargetComp || !TargetComp->CharacterData)
		{
			return 0.0f;
		}
		// Floor RESISTANCE_MIN: a net weakness amplifies buildup (up to 2x at -1). Ceiling
		// RESISTANCE_MAX: above 1 would heal the bar.
		const float Total = GetGeneralStatusResistance(Target) + GetHitTypeResistance(Target, Element, PhysicalType) +
							GetStatusResistBonus(Target);
		return FMath::Clamp(Total, CombatConstants::RESISTANCE_MIN, CombatConstants::RESISTANCE_MAX);
	}
}
