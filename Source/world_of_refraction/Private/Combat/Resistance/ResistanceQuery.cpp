// ResistanceQuery.cpp

#include "Combat/Resistance/ResistanceQuery.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "Combat/Resistance/ClassInnateResistanceTable.h"
#include "Combat/Resistance/ResistanceColumns.h"
#include "Combat/Elements/ElementTable.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "Combat/CombatConstants.h"
#include "Character/CharacterDataComponent.h"
#include "Character/StatLayers.h"
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
		return WoRFight::Get<USkillEffectManager>(GI);
	}

	/** The two status axes a damage type stands for (element or physical, never both). */
	void ToAxes(EWoRDamageType DamageType, ESpellElement &OutElement, EPhysicalDamageType &OutPhysical)
	{
		const FElementRow &Row = ElementTable::For(DamageType);
		OutElement = Row.Element;
		OutPhysical = Row.Physical;
	}

	/** The resistance of the type DamageType is also resisted by (Water: Ice, B-226), 0 without one. */
	float GetAlsoResistance(AActor *Target, EWoRDamageType DamageType)
	{
		const EWoRDamageType Also = ElementTable::For(DamageType).AlsoResistedBy;
		return Also != EWoRDamageType::None ? WoRResistance::GetTypeResistance(Target, Also) : 0.0f;
	}

	/** Whether a resistance buff/debuff counts toward DamageType. A physical-keyed effect counts
	 *  for its subtype only; any other effect counts for its element's type (None and Generic both
	 *  the non-elemental bucket, Q-216). */
	bool EffectMatches(const FActiveSkillEffect &Effect, EWoRDamageType DamageType)
	{
		if (Effect.PhysicalType != EPhysicalDamageType::None)
		{
			// B-358 (D-148): one Physical resistance: a physical buff or debuff of any subtype covers every physical hit.
			return WoRDamageType::IsPhysical(DamageType);
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
		return (ResistanceColumns::Get(Gear, WoRDamageType::ElementToDamageType(Element)) +
				ResistanceColumns::Get(Gear, WoRDamageType::PhysicalToDamageType(Physical))) /
			   ClassInnateResistanceTable::RESISTANCE_PERCENT_DIVISOR;
	}
}

namespace WoRResistance
{
	FTypeResistanceSources GetTypeResistanceSources(AActor *Target, EWoRDamageType DamageType)
	{
		FTypeResistanceSources Sources;
		if (!Target)
		{
			return Sources;
		}
		ESpellElement Element;
		EPhysicalDamageType Physical;
		ToAxes(DamageType, Element, Physical);

		Sources.Effect = EffectTypeResistance(Target, DamageType);                                 // #4
		Sources.Class = ClassInnateResistanceTable::GetClassInnateResistance(Target, Element, Physical); // #5
		Sources.Gear = GearTypeResistance(Target, Element, Physical);                              // #6
		return Sources;
	}

	float GetTypeResistance(AActor *Target, EWoRDamageType DamageType)
	{
		float OnGAS = 0.0f;
		if (UWoRAttributeSyncComponent::ReadComposedOnGAS(Target, UWoRAttributeSet_Resistance::GetTypeAttribute(DamageType), OnGAS))
		{
			return OnGAS; // B-042yc: GAS's value at UseGAS 1
		}
		return GetTypeResistanceSources(Target, DamageType).Sum();
	}

	float GetElementDamageResistance(AActor *Target, ESpellElement Element)
	{
		return GetHitDamageResistance(Target, FWoRHitKind::FromPair(Element, EPhysicalDamageType::None));
	}

	float GetScopeLineResistance(AActor *Target, bool bHitIsSpell)
	{
		const ULoadoutComponent *Loadout = Target ? Target->FindComponentByClass<ULoadoutComponent>() : nullptr;
		const float Gear = Loadout ? Loadout->GetActiveResistanceBonus(Target).GetScopeLine(bHitIsSpell) /
										 ClassInnateResistanceTable::RESISTANCE_PERCENT_DIVISOR
								   : 0.0f;
		// Q-389 (Crown, 2026-11-09): the class table's Physical column is the class's Physical line for damage too
		// (Generic +15%, Resonator -15%, Refractor 0): an intended balance change. No element passed, so only that column.
		const float ClassPhysical = bHitIsSpell ? 0.0f
												: ClassInnateResistanceTable::GetClassInnateResistance(Target, ESpellElement::None,
																										 EPhysicalDamageType::Slash);
		return Gear + ClassPhysical;
	}

	float GetHitDamageResistance(AActor *Target, const FWoRHitKind &Hit)
	{
		const EWoRDamageType ElementType = Hit.ElementType();
		if (!Target || ElementType == EWoRDamageType::None)
		{
			return 0.0f;
		}
		return GetTypeResistance(Target, ElementType) + GetAlsoResistance(Target, ElementType);
	}

	float GetHitTypeResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType PhysicalType)
	{
		return GetHitTypeResistance(Target, FWoRHitKind::FromPair(Element, PhysicalType));
	}

	float GetHitTypeResistance(AActor *Target, const FWoRHitKind &Hit)
	{
		const EWoRDamageType ElementType = Hit.ElementType();
		float Resist = GetTypeResistance(Target, ElementType) + GetAlsoResistance(Target, ElementType);
		if (Hit.Physical != EPhysicalDamageType::None)
		{
			Resist += GetTypeResistance(Target, WoRDamageType::PhysicalToDamageType(Hit.Physical));
		}
		return Resist;
	}

	bool GetGeneralStatusResistanceLayers(AActor *Target, FWoRStatLayers &OutLayers)
	{
		const UCharacterDataComponent *TargetComp = Target ? Target->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TargetComp || !TargetComp->CharacterData)
		{
			return false;
		}
		// The Spirit stat capped alone (UNIVERSAL_STAT_CAP), then gear points and the attached stones
		// (weapon + ring, summed) multiply it toward RESISTANCE_MAX. Never negative: the amplify range
		// comes from the type and bonus layers.
		OutLayers = FWoRStatLayers();
		OutLayers.Min = 0.0f;
		OutLayers.Max = CombatConstants::RESISTANCE_MAX;
		OutLayers.StatBase = TargetComp->GetCrystalResistanceStatCapped();                 // #1
		if (const ULoadoutComponent *Loadout = Target->FindComponentByClass<ULoadoutComponent>())
		{
			const FEquipmentStatBonus Bonus = Loadout->GetActiveStatBonus(Target);
			OutLayers.GearFactor = 1.0f + Bonus.BonusResistance * CombatConstants::RESISTANCE_PER_POINT;  // #2
			OutLayers.StoneFactor = 1.0f + Loadout->GetAttachedStonePercent(ESubStat::Resistance) /
											   CombatConstants::STAT_PERCENT_DIVISOR;                 // #3
		}
		return true;
	}

	float GetGeneralStatusResistance(AActor *Target)
	{
		float OnGAS = 0.0f;
		if (UWoRAttributeSyncComponent::ReadComposedOnGAS(Target, UWoRAttributeSet_Resistance::GetStatusResistanceAttribute(), OnGAS))
		{
			return OnGAS; // B-042yc: GAS's value at UseGAS 1
		}
		FWoRStatLayers Layers;
		return GetGeneralStatusResistanceLayers(Target, Layers) ? Layers.Compose() : 0.0f;
	}

	FStatusResistBonusSources GetStatusResistBonusSources(AActor *Target)
	{
		FStatusResistBonusSources Sources;
		if (!Target)
		{
			return Sources;
		}
		// #7 signed: a negative ModifyStatusResist (the stone's enemy curse) reaches the amplify range.
		if (const USkillEffectManager *EffectMgr = EffectManagerFor(Target))
		{
			Sources.Effect = EffectMgr->GetTotalStatModifier(Target, ESkillEffectType::ModifyStatusResist) /
							 CombatConstants::STAT_PERCENT_DIVISOR;
		}
		// #8 boss rising resistance (B-193): a step per status break this fight.
		if (const UBossPhaseComponent *Boss = Target->FindComponentByClass<UBossPhaseComponent>())
		{
			Sources.Boss = Boss->GetStatusResistanceBonus();
		}
		return Sources;
	}

	float GetStatusResistBonus(AActor *Target)
	{
		float OnGAS = 0.0f;
		if (UWoRAttributeSyncComponent::ReadComposedOnGAS(Target, UWoRAttributeSet_Resistance::GetStatusResistBonusAttribute(), OnGAS))
		{
			return OnGAS; // B-042yc: GAS's value at UseGAS 1
		}
		return GetStatusResistBonusSources(Target).Sum();
	}

	float GetTotalStatusResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType PhysicalType)
	{
		return GetTotalStatusResistance(Target, FWoRHitKind::FromPair(Element, PhysicalType));
	}

	float GetTotalStatusResistance(AActor *Target, const FWoRHitKind &Hit)
	{
		const UCharacterDataComponent *TargetComp = Target ? Target->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TargetComp || !TargetComp->CharacterData)
		{
			return 0.0f;
		}
		// Floor RESISTANCE_MIN: a net weakness amplifies buildup (up to 2x at -1). Ceiling
		// RESISTANCE_MAX: above 1 would heal the bar.
		const float Total = GetGeneralStatusResistance(Target) + GetHitTypeResistance(Target, Hit) +
							GetStatusResistBonus(Target);
		return FMath::Clamp(Total, CombatConstants::RESISTANCE_MIN, CombatConstants::RESISTANCE_MAX);
	}
}
