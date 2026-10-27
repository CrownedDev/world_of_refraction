// WoRGameplayTags.h
// GAS stage 1 (B-040, Design/GASMigration.md §4): the native tag tree and the enum <-> tag maps.
// The enums stay the stored form (assets serialize them); tags are the query language from here on.
// Debug: UWoRTagDebug (wor.DumpTags, wor.CompareTags).

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "Character/ECharacterClass.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Combat/Resistance/EWoRDamageType.h"

namespace WoRTags
{
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Generic);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Fire);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Water);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Earth);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Wind);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Light);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Darkness);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Lightning);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Void);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Reality);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_None);

	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Class_Generic);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Class_Caster);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Class_Resonator);

	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Stun);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Silence);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_HealBlock);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_DOT);

	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_All);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_Stun);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_Silence);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_DOT);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_Element_Fire);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_Element_Water);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_Element_Earth);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_Element_Wind);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_Element_Light);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_Element_Darkness);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_Element_Lightning);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_Element_Void);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_Element_Reality);

	// B-216: one tag per EWoRDamageType; DamageType.Physical is the parent of the three subtypes,
	// so a modifier or immunity can key all physical damage at once.
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_None);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Physical);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Physical_Slash);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Physical_Pierce);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Physical_Impact);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Element_Fire);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Element_Water);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Element_Earth);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Element_Wind);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Element_Light);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Element_Darkness);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Element_Lightning);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Element_Void);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Element_Reality);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Ice);

	// B-042b: what a GameplayEffect is (asset tags, Q-040b), where it came from, and its SetByCaller key.
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Family_Status);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Family_Immunity);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Family_StatModifier);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Family_DOT);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Family_HOT);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Family_Reflect);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Family_LastStand);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Family_OnHit);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Family_Attack);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Family_Control);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Family_Periodic);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Family_Action);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Origin_Gear);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Magnitude);

	// B-042d: GameplayEvents the combat code sends (GAS/WoRCombatEvents.h, Q-044j).
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Hit);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_DefenseResolved);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_TurnStart);
	WORLD_OF_REFRACTION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_TurnEnd);

	/** Element.<name>; every ESpellElement has one. */
	WORLD_OF_REFRACTION_API FGameplayTag ElementToTag(ESpellElement Element);
	/** False for a tag outside Element.*. */
	WORLD_OF_REFRACTION_API bool TagToElement(const FGameplayTag &Tag, ESpellElement &OutElement);

	/** DamageType.*; every EWoRDamageType has one. */
	WORLD_OF_REFRACTION_API FGameplayTag DamageTypeToTag(EWoRDamageType DamageType);
	/** False for a tag outside the DamageType.* leaves (the DamageType.Physical parent included). */
	WORLD_OF_REFRACTION_API bool TagToDamageType(const FGameplayTag &Tag, EWoRDamageType &OutDamageType);

	/** Class.<name>; every ECharacterClass has one. */
	WORLD_OF_REFRACTION_API FGameplayTag ClassToTag(ECharacterClass CharacterClass);
	/** False for a tag outside Class.*. */
	WORLD_OF_REFRACTION_API bool TagToClass(const FGameplayTag &Tag, ECharacterClass &OutClass);

	/** The tag an active effect of this type gives its bearer (Status.* or Immunity.*); an empty tag
	 *  for every type with no tag yet (stat modifiers etc. get theirs in B-042). */
	WORLD_OF_REFRACTION_API FGameplayTag EffectTypeToTag(ESkillEffectType EffectType);

	/** Every effect type that has a tag, in a fixed order (for the debug dump and the spec). */
	WORLD_OF_REFRACTION_API const TArray<ESkillEffectType> &TaggedEffectTypes();
}
