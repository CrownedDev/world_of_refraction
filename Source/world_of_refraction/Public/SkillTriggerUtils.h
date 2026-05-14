// SkillTriggerUtils.h
// Shared classification helpers for ESkillTrigger.

#pragma once

#include "ESkillTrigger.h"

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

    /** Triggers evaluated against the TARGET's state rather than the source's.
     *  Expand as new target-side triggers are added. */
    inline bool IsTargetSideTrigger(ESkillTrigger Trigger)
    {
        return Trigger == ESkillTrigger::OnHPBelowThreshold
            || Trigger == ESkillTrigger::OnHPAboveThreshold
            || Trigger == ESkillTrigger::OnEnergyBelowThreshold
            || Trigger == ESkillTrigger::OnEnergyAboveThreshold;
    }
}
