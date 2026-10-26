// SkillTriggerUtils.h
// Shared classification helpers for ESkillTrigger.

#pragma once

#include "Skills/Effects/ESkillTrigger.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Defense/GuardRules.h"

namespace SkillTriggerUtils
{
    /** True for triggers that author a numeric threshold (HP or Energy %). */
    inline bool IsThresholdTrigger(ESkillTrigger Trigger)
    {
        return Trigger == ESkillTrigger::OnHPBelowThreshold
            || Trigger == ESkillTrigger::OnHPAboveThreshold
            || Trigger == ESkillTrigger::OnEnergyBelowThreshold
            || Trigger == ESkillTrigger::OnEnergyAboveThreshold;
    }

    /** True for the defense-outcome triggers — impact-driven, fired off OnDefenseResolved.
     *  Distinct from IsThresholdTrigger; used to gate the impact-path eval so pure-threshold
     *  conditionals never leak into it. */
    inline bool IsDefenseOutcomeTrigger(ESkillTrigger T)
    {
        return T == ESkillTrigger::OnParry || T == ESkillTrigger::OnPerfectParry
            || T == ESkillTrigger::OnBlock || T == ESkillTrigger::OnPerfectBlock
            || T == ESkillTrigger::OnDodge || T == ESkillTrigger::OnPerfectDodge
            || T == ESkillTrigger::OnGuard || T == ESkillTrigger::OnPerfectGuard
            || T == ESkillTrigger::OnTakeDamage;
    }

    /** Maps a resolved defense outcome to the trigger(s) it fires.
     *  NOTE: superset — a perfect outcome fires BOTH its perfect trigger and the base
     *    (OnPerfectParry implies OnParry), so an effect authored OnParry fires on any parry.
     *  NOTE: miss/accuracy-avoid is deliberately NOT mapped (dodge covers defender
     *    avoidance; accuracy-miss is a separate axis — future work).
     *  NOTE: attacker-side ("react to how MY target defended") is handled by the
     *    condition's Subject (Target / TargetTeam), NOT a separate trigger. */
    inline void DefenseOutcomeToTriggers(EDefenseType Type, bool bPerfect,
                                         TArray<ESkillTrigger> &OutTriggers)
    {
        switch (GuardRules::Normalize(Type))
        {
        case EDefenseType::None:  OutTriggers.Add(ESkillTrigger::OnTakeDamage); break;
        // Guard (D-084 / D-087): legacy On Block maps to On Guard; legacy On Perfect Block, On Parry
        // and On Perfect Parry map to On Perfect Guard, so effects authored on them keep working.
        case EDefenseType::Block: OutTriggers.Append({ESkillTrigger::OnGuard, ESkillTrigger::OnBlock});
                                  if (bPerfect)
                                  {
                                      OutTriggers.Append({ESkillTrigger::OnPerfectGuard, ESkillTrigger::OnPerfectBlock,
                                                          ESkillTrigger::OnParry, ESkillTrigger::OnPerfectParry});
                                  }
                                  break;
        case EDefenseType::Dodge: OutTriggers.Add(ESkillTrigger::OnDodge);
                                  if (bPerfect) OutTriggers.Add(ESkillTrigger::OnPerfectDodge); break;
        default: break;
        }
    }
}
