// WoRGameplayTags.cpp

#include "Tags/WoRGameplayTags.h"

namespace WoRTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Element_Generic, "Element.Generic", "Inherits the caster's element at cast time");
	UE_DEFINE_GAMEPLAY_TAG(Element_Fire, "Element.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Element_Water, "Element.Water");
	UE_DEFINE_GAMEPLAY_TAG(Element_Earth, "Element.Earth");
	UE_DEFINE_GAMEPLAY_TAG(Element_Wind, "Element.Wind");
	UE_DEFINE_GAMEPLAY_TAG(Element_Light, "Element.Light");
	UE_DEFINE_GAMEPLAY_TAG(Element_Darkness, "Element.Darkness");
	UE_DEFINE_GAMEPLAY_TAG(Element_Lightning, "Element.Lightning");
	UE_DEFINE_GAMEPLAY_TAG(Element_Void, "Element.Void");
	UE_DEFINE_GAMEPLAY_TAG(Element_Reality, "Element.Reality");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Element_None, "Element.None", "Non-elemental");

	UE_DEFINE_GAMEPLAY_TAG(Class_Generic, "Class.Generic");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Class_Caster, "Class.Caster", "Shown to the player as Refractor");
	UE_DEFINE_GAMEPLAY_TAG(Class_Resonator, "Class.Resonator");

	UE_DEFINE_GAMEPLAY_TAG(Status_Stun, "Status.Stun");
	UE_DEFINE_GAMEPLAY_TAG(Status_Silence, "Status.Silence");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_HealBlock, "Status.HealBlock", "Can't be healed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_DOT, "Status.DOT", "Takes damage over time");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Absorb_Barrier, "Status.Absorb.Barrier", "Negates damage of any kind");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Absorb_Shield, "Status.Absorb.Shield", "Negates physical damage");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Absorb_Ward, "Status.Absorb.Ward", "Negates one element's damage");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Immunity_All, "Immunity.All", "Immune to every status");
	UE_DEFINE_GAMEPLAY_TAG(Immunity_Stun, "Immunity.Stun");
	UE_DEFINE_GAMEPLAY_TAG(Immunity_Silence, "Immunity.Silence");
	UE_DEFINE_GAMEPLAY_TAG(Immunity_DOT, "Immunity.DOT");
	UE_DEFINE_GAMEPLAY_TAG(Immunity_Element_Fire, "Immunity.Element.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Immunity_Element_Water, "Immunity.Element.Water");
	UE_DEFINE_GAMEPLAY_TAG(Immunity_Element_Earth, "Immunity.Element.Earth");
	UE_DEFINE_GAMEPLAY_TAG(Immunity_Element_Wind, "Immunity.Element.Wind");
	UE_DEFINE_GAMEPLAY_TAG(Immunity_Element_Light, "Immunity.Element.Light");
	UE_DEFINE_GAMEPLAY_TAG(Immunity_Element_Darkness, "Immunity.Element.Darkness");
	UE_DEFINE_GAMEPLAY_TAG(Immunity_Element_Lightning, "Immunity.Element.Lightning");
	UE_DEFINE_GAMEPLAY_TAG(Immunity_Element_Void, "Immunity.Element.Void");
	UE_DEFINE_GAMEPLAY_TAG(Immunity_Element_Reality, "Immunity.Element.Reality");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DamageType_None, "DamageType.None", "No damage type: pure effects; the non-elemental resistance bucket");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Physical, "DamageType.Physical");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Physical_Slash, "DamageType.Physical.Slash");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Physical_Pierce, "DamageType.Physical.Pierce");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Physical_Impact, "DamageType.Physical.Impact");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Element_Fire, "DamageType.Element.Fire");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Element_Water, "DamageType.Element.Water");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Element_Earth, "DamageType.Element.Earth");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Element_Wind, "DamageType.Element.Wind");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Element_Light, "DamageType.Element.Light");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Element_Darkness, "DamageType.Element.Darkness");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Element_Lightning, "DamageType.Element.Lightning");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Element_Void, "DamageType.Element.Void");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Element_Reality, "DamageType.Element.Reality");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DamageType_Ice, "DamageType.Element.Water.Ice", "D-029: keyed Water; under Water so Water triggers hear it (B-273)");

	UE_DEFINE_GAMEPLAY_TAG(Effect_Family_Status, "Effect.Family.Status");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Family_Immunity, "Effect.Family.Immunity");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Family_StatModifier, "Effect.Family.StatModifier");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Family_DOT, "Effect.Family.DOT");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Family_HOT, "Effect.Family.HOT");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Family_Reflect, "Effect.Family.Reflect");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Family_Absorb, "Effect.Family.Absorb");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Family_LastStand, "Effect.Family.LastStand");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Family_OnHit, "Effect.Family.OnHit");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Family_AttackModifier, "Effect.Family.AttackModifier");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Family_Control, "Effect.Family.Control");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Family_Periodic, "Effect.Family.Periodic");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Family_Action, "Effect.Family.Action");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Origin_Gear, "Effect.Origin.Gear", "D-015: a cleanse leaves it");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Magnitude, "SetByCaller.Magnitude", "The effect value a family GE reads");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stat_Gear, "SetByCaller.Stat.Gear", "A composed stat's gear factor (1 + points as a fraction)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stat_Stone, "SetByCaller.Stat.Stone", "A composed stat's attached-stone factor");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stat_Transient, "SetByCaller.Stat.Transient", "A composed stat's buff / debuff factor");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Family_StatSources, "Effect.Family.StatSources", "B-042p: a stat's gear / stone / buff layers");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Family_DerivedStat, "Effect.Family.DerivedStat", "B-042s: a stat derived from other attributes");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Family_Cost, "Effect.Family.Cost", "B-043a: a skill's EP cost");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Family_Backlash, "Effect.Family.Backlash", "B-043a: an infused skill's HP cost");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Cost_Energy, "SetByCaller.Cost.Energy", "The EP change a cost GE makes: minus the cost");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Cost_Health, "SetByCaller.Cost.Health", "The HP change a backlash GE makes: minus the cost");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Hit, "Event.Combat.Hit", "Sent to the target of a hit; magnitude = damage, 0 = a hit the absorbers stopped entirely (B-042j2)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_DefenseResolved, "Event.Combat.DefenseResolved", "Sent to the defender once its defense resolves");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_TurnStart, "Event.Combat.TurnStart", "Sent to the actor whose turn starts");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_TurnEnd, "Event.Combat.TurnEnd", "Sent to the actor whose turn ends");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Impact, "Event.Combat.Impact", "B-043b (sent from B-043c): a melee impact frame, for WaitGameplayEvent");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Projectile, "Event.Combat.Projectile", "B-043d: a projectile's end (parent of Arrived and Dodged), for WaitGameplayEvent");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Projectile_Arrived, "Event.Combat.Projectile.Arrived", "B-043d: a projectile reached its target; Target = the struck actor, magnitude = cast entry index");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Projectile_Dodged, "Event.Combat.Projectile.Dodged", "B-043d: a projectile's target moved out of its impact zone; Target = that actor");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Shape_Melee, "Ability.Shape.Melee", "B-043b: a skill that strikes at close range");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Shape_Ranged, "Ability.Shape.Ranged", "B-043b: a skill that stays in place and strikes from range");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Shape_SelfCast, "Ability.Shape.SelfCast", "B-043b: a skill on self or allies, no projectile");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Shape_TargetCast, "Ability.Shape.TargetCast", "B-043b: a direct effect on the target, no projectile");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Shape_Projectile, "Ability.Shape.Projectile", "B-043b: a cast that travels to the target");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Shape_AOE, "Ability.Shape.AOE", "B-043b: a cast that spawns at the target's area");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Shape_Instant, "Ability.Shape.Instant", "B-043b: a cast with no travel time");

	namespace
	{
		struct FElementTagRow
		{
			ESpellElement Element;
			const FNativeGameplayTag &Tag;
		};

		const FElementTagRow ELEMENT_TAG_ROWS[] = {
			{ESpellElement::Generic, Element_Generic}, {ESpellElement::Fire, Element_Fire},
			{ESpellElement::Water, Element_Water}, {ESpellElement::Earth, Element_Earth},
			{ESpellElement::Wind, Element_Wind}, {ESpellElement::Light, Element_Light},
			{ESpellElement::Darkness, Element_Darkness}, {ESpellElement::Lightning, Element_Lightning},
			{ESpellElement::Void, Element_Void}, {ESpellElement::Reality, Element_Reality},
			{ESpellElement::None, Element_None}};

		struct FDamageTypeTagRow
		{
			EWoRDamageType DamageType;
			const FNativeGameplayTag &Tag;
		};

		const FDamageTypeTagRow DAMAGE_TYPE_TAG_ROWS[] = {
			{EWoRDamageType::None, DamageType_None}, {EWoRDamageType::Fire, DamageType_Element_Fire},
			{EWoRDamageType::Water, DamageType_Element_Water}, {EWoRDamageType::Earth, DamageType_Element_Earth},
			{EWoRDamageType::Wind, DamageType_Element_Wind}, {EWoRDamageType::Light, DamageType_Element_Light},
			{EWoRDamageType::Darkness, DamageType_Element_Darkness}, {EWoRDamageType::Lightning, DamageType_Element_Lightning},
			{EWoRDamageType::Void, DamageType_Element_Void}, {EWoRDamageType::Reality, DamageType_Element_Reality},
			{EWoRDamageType::Slash, DamageType_Physical_Slash}, {EWoRDamageType::Pierce, DamageType_Physical_Pierce},
			{EWoRDamageType::Impact, DamageType_Physical_Impact}, {EWoRDamageType::Ice, DamageType_Ice}};

		struct FClassTagRow
		{
			ECharacterClass CharacterClass;
			const FNativeGameplayTag &Tag;
		};

		const FClassTagRow CLASS_TAG_ROWS[] = {
			{ECharacterClass::Generic, Class_Generic}, {ECharacterClass::Caster, Class_Caster},
			{ECharacterClass::Resonator, Class_Resonator}};

		struct FEffectTagRow
		{
			ESkillEffectType EffectType;
			const FNativeGameplayTag &Tag;
		};

		const FEffectTagRow EFFECT_TAG_ROWS[] = {
			{ESkillEffectType::Stun, Status_Stun},
			{ESkillEffectType::Silenced, Status_Silence},
			{ESkillEffectType::HealBlock, Status_HealBlock},
			{ESkillEffectType::DOT, Status_DOT},
			{ESkillEffectType::GrantAllStatusImmunity, Immunity_All},
			{ESkillEffectType::GrantStunImmunity, Immunity_Stun},
			{ESkillEffectType::GrantSilenceImmunity, Immunity_Silence},
			{ESkillEffectType::GrantDOTImmunity, Immunity_DOT},
			{ESkillEffectType::GrantFireImmunity, Immunity_Element_Fire},
			{ESkillEffectType::GrantWaterImmunity, Immunity_Element_Water},
			{ESkillEffectType::GrantEarthImmunity, Immunity_Element_Earth},
			{ESkillEffectType::GrantWindImmunity, Immunity_Element_Wind},
			{ESkillEffectType::GrantLightImmunity, Immunity_Element_Light},
			{ESkillEffectType::GrantDarknessImmunity, Immunity_Element_Darkness},
			{ESkillEffectType::GrantLightningImmunity, Immunity_Element_Lightning},
			{ESkillEffectType::GrantVoidImmunity, Immunity_Element_Void},
			{ESkillEffectType::GrantRealityImmunity, Immunity_Element_Reality}};
	}

	FGameplayTag ElementToTag(ESpellElement Element)
	{
		for (const FElementTagRow &Row : ELEMENT_TAG_ROWS)
		{
			if (Row.Element == Element)
			{
				return Row.Tag;
			}
		}
		return FGameplayTag();
	}

	bool TagToElement(const FGameplayTag &Tag, ESpellElement &OutElement)
	{
		for (const FElementTagRow &Row : ELEMENT_TAG_ROWS)
		{
			if (Row.Tag.GetTag() == Tag)
			{
				OutElement = Row.Element;
				return true;
			}
		}
		return false;
	}

	FGameplayTag DamageTypeToTag(EWoRDamageType DamageType)
	{
		for (const FDamageTypeTagRow &Row : DAMAGE_TYPE_TAG_ROWS)
		{
			if (Row.DamageType == DamageType)
			{
				return Row.Tag;
			}
		}
		return FGameplayTag();
	}

	bool TagToDamageType(const FGameplayTag &Tag, EWoRDamageType &OutDamageType)
	{
		for (const FDamageTypeTagRow &Row : DAMAGE_TYPE_TAG_ROWS)
		{
			if (Row.Tag.GetTag() == Tag)
			{
				OutDamageType = Row.DamageType;
				return true;
			}
		}
		return false;
	}

	FGameplayTag ClassToTag(ECharacterClass CharacterClass)
	{
		for (const FClassTagRow &Row : CLASS_TAG_ROWS)
		{
			if (Row.CharacterClass == CharacterClass)
			{
				return Row.Tag;
			}
		}
		return FGameplayTag();
	}

	bool TagToClass(const FGameplayTag &Tag, ECharacterClass &OutClass)
	{
		for (const FClassTagRow &Row : CLASS_TAG_ROWS)
		{
			if (Row.Tag.GetTag() == Tag)
			{
				OutClass = Row.CharacterClass;
				return true;
			}
		}
		return false;
	}

	FGameplayTag EffectTypeToTag(ESkillEffectType EffectType)
	{
		for (const FEffectTagRow &Row : EFFECT_TAG_ROWS)
		{
			if (Row.EffectType == EffectType)
			{
				return Row.Tag;
			}
		}
		return FGameplayTag();
	}

	const TArray<ESkillEffectType> &TaggedEffectTypes()
	{
		static const TArray<ESkillEffectType> Types = []()
		{
			TArray<ESkillEffectType> Result;
			for (const FEffectTagRow &Row : EFFECT_TAG_ROWS)
			{
				Result.Add(Row.EffectType);
			}
			return Result;
		}();
		return Types;
	}
}
