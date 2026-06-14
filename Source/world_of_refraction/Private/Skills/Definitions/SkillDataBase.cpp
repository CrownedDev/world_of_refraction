// SkillDataBase.cpp
// Shared base class implementation.

#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Effects/SkillTriggerUtils.h"

TArray<float> ResolveDamageSplit(int32 HitCount, const TArray<FDamageSplitEntry> &Split)
{
    const int32 N = FMath::Max(1, HitCount);

    // Empty split = even split — byte-identical to the legacy behavior.
    TArray<float> Table;
    if (Split.Num() == 0)
    {
        Table.Init(100.0f / N, N);
        return Table;
    }

    // Walk authored entries; invalid ones warn and are skipped.
    Table.Init(0.0f, N);
    TArray<bool> bAuthored;
    bAuthored.Init(false, N);
    float AuthoredSum = 0.0f;
    int32 AuthoredCount = 0;

    for (const FDamageSplitEntry &Entry : Split)
    {
        if (Entry.HitNumber < 1 || Entry.HitNumber > N)
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[ResolveDamageSplit] HitNumber %d out of range [1,%d] — entry skipped"),
                   Entry.HitNumber, N);
            continue;
        }
        if (Entry.Percent <= 0.0f)
        {
            UE_LOG(LogTemp, Verbose,
                   TEXT("[ResolveDamageSplit] Hit %d Percent %.1f <= 0 — treated as unauthored (remainder split evenly among unassigned hits), not lost"),
                   Entry.HitNumber, Entry.Percent);
            continue;
        }
        if (bAuthored[Entry.HitNumber - 1])
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[ResolveDamageSplit] Duplicate entry for hit %d — entry skipped (first wins)"),
                   Entry.HitNumber);
            continue;
        }

        Table[Entry.HitNumber - 1] = Entry.Percent;
        bAuthored[Entry.HitNumber - 1] = true;
        AuthoredSum += Entry.Percent;
        ++AuthoredCount;
    }

    // Unassigned hits share what's left of 100.
    const int32 UnassignedCount = N - AuthoredCount;
    if (UnassignedCount > 0)
    {
        const float PerUnassigned = FMath::Max(0.0f, 100.0f - AuthoredSum) / UnassignedCount;
        for (int32 i = 0; i < N; ++i)
        {
            if (!bAuthored[i])
            {
                Table[i] = PerUnassigned;
            }
        }
    }

    // Normalize when the table can't sum to 100 (over-authored, or all hits
    // authored to a non-100 total). Ratios preserved, total damage invariant —
    // redistribute, never amplify.
    float Total = 0.0f;
    for (const float Pct : Table)
    {
        Total += Pct;
    }
    if (!FMath::IsNearlyEqual(Total, 100.0f, 0.01f) && Total > 0.0f)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[ResolveDamageSplit] Authored split sums to %.1f%% — normalizing to 100%% (ratios preserved)"),
               Total);
        const float Scale = 100.0f / Total;
        for (float &Pct : Table)
        {
            Pct *= Scale;
        }
    }

    return Table;
}

TArray<FSkillEffect> USkillDataBase::GetEffectsForCondition(ESkillTrigger Condition) const
{
    TArray<FSkillEffect> Result;
    for (const FSkillEffect &Effect : Effects)
    {
        if (Effect.Condition == Condition && Effect.IsValid())
        {
            Result.Add(Effect);
        }
    }
    return Result;
}

bool USkillDataBase::HasDrainEffect() const
{
    for (const FSkillEffect &Effect : Effects)
    {
        if (Effect.IsDrain())
        {
            return true;
        }
    }
    return false;
}

bool USkillDataBase::HasBuffEffects() const
{
    for (const FSkillEffect &Effect : Effects)
    {
        if (Effect.IsBuff())
        {
            return true;
        }
    }
    return false;
}

bool USkillDataBase::HasDebuffEffects() const
{
    for (const FSkillEffect &Effect : Effects)
    {
        if (Effect.IsDebuff())
        {
            return true;
        }
    }
    return false;
}

#if WITH_EDITOR
EDataValidationResult USkillDataBase::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    if (Name.IsEmpty())
    {
        Context.AddWarning(FText::FromString(TEXT("Skill has empty Name")));
    }

    if (Effects.Num() > LoadoutConstants::MAX_SKILL_EFFECTS)
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("Too many effects (%d). Maximum is %d"),
            Effects.Num(), LoadoutConstants::MAX_SKILL_EFFECTS)));
        Result = EDataValidationResult::Invalid;
    }

    return Result;
}

void USkillDataBase::PostEditChangeChainProperty(FPropertyChangedChainEvent &PropertyChangedEvent)
{
    Super::PostEditChangeChainProperty(PropertyChangedEvent);

    // Refresh threshold-visibility flags on every effect so EditCondition gating
    // for ConditionThreshold / SecondaryThreshold / TargetThreshold reacts live
    // to in-editor edits of the matching ESkillTrigger field.
    for (FSkillEffect &Effect : Effects)
    {
        Effect.bConditionUsesThreshold          = SkillTriggerUtils::IsThresholdTrigger(Effect.Condition);
        Effect.bSecondaryConditionUsesThreshold = SkillTriggerUtils::IsThresholdTrigger(Effect.SecondaryCondition);
        Effect.bTargetConditionUsesThreshold    = SkillTriggerUtils::IsThresholdTrigger(Effect.TargetCondition);
    }
}
#endif
