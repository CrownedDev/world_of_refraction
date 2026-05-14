// SkillDataBase.cpp
// Shared base class implementation.

#include "SkillDataBase.h"

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
#endif
