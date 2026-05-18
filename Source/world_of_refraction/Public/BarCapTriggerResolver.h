// BarCapTriggerResolver.h
// Maps action source (Element + PhysicalDamageType) to the ESkillEffectType
// trigger that fires when a target's status bar caps.
//
// Locked design (May 2026 session). Element takes priority - Fire-infused
// Slash builds toward Burn (Fire DOT), not Bleed (physical DOT). Generic
// element falls through to physical resolution.
//
// Used internally by UStatusBuildupManager; orchestrators do not call this
// directly - they pass (Element, PhysicalType) to AddStatusBuildup and
// the manager resolves.

#pragma once

#include "CoreMinimal.h"
#include "ESkillEffectType.h"
#include "ESpellElement.h"
#include "EPhysicalDamageType.h"

namespace BarCapTriggerResolver
{
    /** Resolve which ESkillEffectType trigger fires when the bar caps.
     *  Returns ESkillEffectType::None when neither element nor physical type
     *  carry a mapping (Generic + None). */
    inline ESkillEffectType ResolveTrigger(ESpellElement Element, EPhysicalDamageType PhysicalType)
    {
        switch (Element)
        {
        case ESpellElement::Fire:
            return ESkillEffectType::DOT;
        case ESpellElement::Water:
            return ESkillEffectType::HealBlock;
        case ESpellElement::Earth:
            return ESkillEffectType::DefenseDebuff;
        case ESpellElement::Wind:
            return ESkillEffectType::SkipTurn;
        case ESpellElement::Lightning:
            return ESkillEffectType::Stun;
        case ESpellElement::Light:
            return ESkillEffectType::CritChanceDebuff;
        case ESpellElement::Darkness:
            return ESkillEffectType::Silenced;
        case ESpellElement::Void:
            return ESkillEffectType::RandomSkill;
        case ESpellElement::Reality:
            return ESkillEffectType::BurstDamage;
        case ESpellElement::BrokenDarkness:
            return ESkillEffectType::DrainEnergy;
        case ESpellElement::Generic:
        default:
            break; // Fall through to physical
        }

        switch (PhysicalType)
        {
        case EPhysicalDamageType::Slash:
            return ESkillEffectType::DOT;
        case EPhysicalDamageType::Pierce:
            return ESkillEffectType::DefenseDebuff;
        case EPhysicalDamageType::Impact:
            return ESkillEffectType::Stun;
        case EPhysicalDamageType::None:
        default:
            return ESkillEffectType::None;
        }
    }
}
