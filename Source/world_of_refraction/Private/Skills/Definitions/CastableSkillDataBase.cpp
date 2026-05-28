// CastableSkillDataBase.cpp
// Intermediate base implementation.

#include "Skills/Definitions/CastableSkillDataBase.h"
#include "Character/CharacterData.h"

bool UCastableSkillDataBase::MeetsRequirements(const UCharacterData *Character) const
{
    if (!Character)
    {
        return false;
    }
    return Requirements.MeetsRequirements(Character);
}

int32 UCastableSkillDataBase::GetTotalDeficit(const UCharacterData *Character) const
{
    if (!Character)
    {
        return 0;
    }
    return Requirements.GetTotalDeficit(Character);
}

float UCastableSkillDataBase::CalculateRequirementPenalty(const UCharacterData *Character) const
{
    if (!Character)
    {
        return 0.0f;
    }
    return Requirements.CalculatePenalty(Character);
}

FString UCastableSkillDataBase::GetTierString() const
{
    return TierHelpers::GetTierDisplayString(Tier);
}

#if WITH_EDITOR
EDataValidationResult UCastableSkillDataBase::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    if (BaseDamage < 0)
    {
        Context.AddError(FText::FromString(TEXT("BaseDamage cannot be negative")));
        Result = EDataValidationResult::Invalid;
    }

    if (BaseEnergyCost < 0)
    {
        Context.AddError(FText::FromString(TEXT("BaseEnergyCost cannot be negative")));
        Result = EDataValidationResult::Invalid;
    }

    return Result;
}
#endif
