// SkillTriggerUtils.h
// Shared classification helpers for ESkillTrigger.

#pragma once

#include "Skills/Effects/ESkillTrigger.h"
#include "Combat/Defense/EDefenseType.h"

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

    /** True for the 7 defense-outcome triggers — impact-driven, fired off OnDefenseResolved.
     *  Distinct from IsThresholdTrigger; used to gate the impact-path eval so pure-threshold
     *  conditionals never leak into it. */
    inline bool IsDefenseOutcomeTrigger(ESkillTrigger T)
    {
        return T == ESkillTrigger::OnParry || T == ESkillTrigger::OnPerfectParry
            || T == ESkillTrigger::OnBlock || T == ESkillTrigger::OnPerfectBlock
            || T == ESkillTrigger::OnDodge || T == ESkillTrigger::OnPerfectDodge
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
        switch (Type)
        {
        case EDefenseType::None:  OutTriggers.Add(ESkillTrigger::OnTakeDamage); break;
        case EDefenseType::Block: OutTriggers.Add(ESkillTrigger::OnBlock);
                                  if (bPerfect) OutTriggers.Add(ESkillTrigger::OnPerfectBlock); break;
        case EDefenseType::Parry: OutTriggers.Add(ESkillTrigger::OnParry);
                                  if (bPerfect) OutTriggers.Add(ESkillTrigger::OnPerfectParry); break;
        case EDefenseType::Dodge: OutTriggers.Add(ESkillTrigger::OnDodge);
                                  if (bPerfect) OutTriggers.Add(ESkillTrigger::OnPerfectDodge); break;
        }
    }
}
