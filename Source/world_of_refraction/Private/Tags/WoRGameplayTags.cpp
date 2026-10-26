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
