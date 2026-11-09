// EDamageScope.h
// B-383 (Crown's direction 2026-11-08, D-166): which hits a damage or defence boost counts for, like an Amp line.
// Read by DamageBuff / DamageDebuff, ModifyDamageDealt, Raw / SpellDamageBuff / Debuff (attacker side) and
// DefenseBuff / DefenseDebuff, ReduceDamageTaken / IncreaseDamageTaken (defender side). Default All, so authored
// data plays as before. Physical and Spell are the same axes as the gear's Raw / Spell damage lines and stones:
// Physical = an action that is not a spell (attacks and abilities, infused or not), Spell = a spell.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "EDamageScope.generated.h"

UENUM(BlueprintType)
enum class EDamageScope : uint8
{
	All = 0 UMETA(DisplayName = "All hits"),
	Physical = 1 UMETA(DisplayName = "Physical (attacks and abilities)"),
	Spell = 2 UMETA(DisplayName = "Spells"),
	/** Only hits of the effect's own Element. */
	Element = 3 UMETA(DisplayName = "One element (the effect's Element)")
};

namespace WoRDamageScope
{
	/** The effect types that read a damage scope (attacker and defender boosts); every other type ignores it. */
	inline bool IsScopedType(ESkillEffectType Type)
	{
		switch (Type)
		{
		case ESkillEffectType::DamageBuff:
		case ESkillEffectType::DamageDebuff:
		case ESkillEffectType::ModifyDamageDealt:
		case ESkillEffectType::RawDamageBuff:
		case ESkillEffectType::RawDamageDebuff:
		case ESkillEffectType::SpellDamageBuff:
		case ESkillEffectType::SpellDamageDebuff:
		case ESkillEffectType::DefenseBuff:
		case ESkillEffectType::DefenseDebuff:
		case ESkillEffectType::ReduceDamageTaken:
		case ESkillEffectType::IncreaseDamageTaken:
			return true;
		default:
			return false;
		}
	}

	/** Whether an effect of Scope (keyed to EffectElement when Scope is Element) counts for a hit of HitElement
	 *  from a spell (bHitIsSpell) or not. An Element scope with no real element counts for nothing. */
	inline bool Matches(EDamageScope Scope, ESpellElement EffectElement, ESpellElement HitElement, bool bHitIsSpell)
	{
		switch (Scope)
		{
		case EDamageScope::Physical: return !bHitIsSpell;
		case EDamageScope::Spell: return bHitIsSpell;
		case EDamageScope::Element:
			return EffectElement != ESpellElement::None && EffectElement != ESpellElement::Generic && EffectElement == HitElement;
		default: return true;
		}
	}

	/** "All" / "Physical" / "Spell" / "Fire only" (debug). */
	inline FString ToString(EDamageScope Scope, ESpellElement EffectElement)
	{
		switch (Scope)
		{
		case EDamageScope::Physical: return TEXT("Physical");
		case EDamageScope::Spell: return TEXT("Spell");
		case EDamageScope::Element:
			return FString::Printf(TEXT("%s only"), *UEnum::GetDisplayValueAsText(EffectElement).ToString());
		default: return TEXT("All");
		}
	}
}
