// BarCapTriggerResolver.h
// Maps action source (Element + PhysicalDamageType) to the EStatusType
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
#include "EStatusType.h"
#include "ESpellElement.h"
#include "EPhysicalDamageType.h"

namespace BarCapTriggerResolver
{
    /** Resolve which EStatusType trigger fires when the bar caps.
     *  Returns EStatusType::None when neither element nor physical type
     *  carry a mapping (Generic + None). */
    inline EStatusType ResolveTrigger(ESpellElement Element, EPhysicalDamageType PhysicalType)
    {
        switch (Element)
        {
        case ESpellElement::Fire:      return EStatusType::DOT;
        case ESpellElement::Water:     return EStatusType::HealBlock;
        case ESpellElement::Earth:     return EStatusType::DefenseDebuff;
        case ESpellElement::Wind:      return EStatusType::SkipTurn;
        case ESpellElement::Lightning: return EStatusType::Stun;
        case ESpellElement::Light:     return EStatusType::CritChanceDebuff;
        case ESpellElement::Darkness:  return EStatusType::Silenced;
        case ESpellElement::Void:      return EStatusType::RandomSkill;
        case ESpellElement::Reality:   return EStatusType::BurstDamage;
        case ESpellElement::Generic:
        case ESpellElement::BrokenDarkness:
        default:
            break; // Fall through to physical
        }

        switch (PhysicalType)
        {
        case EPhysicalDamageType::Slash:   return EStatusType::DOT;
        case EPhysicalDamageType::Pierce:  return EStatusType::DefenseDebuff;
        case EPhysicalDamageType::Impact:  return EStatusType::Stun;
        case EPhysicalDamageType::None:
        default:
            return EStatusType::None;
        }
    }
}
