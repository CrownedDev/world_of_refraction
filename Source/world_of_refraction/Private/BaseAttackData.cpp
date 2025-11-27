// BaseAttackData.cpp
// Implementation of BaseAttackData functions

#include "BaseAttackData.h"

float UBaseAttackData::GetHitDamagePercent(int32 HitIndex) const
{
    if (HitCount == 1)
    {
        return (HitIndex == 0) ? 100.0f : 0.0f;
    }

    // Multi-hit
    switch (HitIndex)
    {
    case 0:
        return FirstHitPercent;
    case 1:
        return SecondHitPercent;
    default:
        return 0.0f;
    }
}

float UBaseAttackData::GetTotalDamagePercent() const
{
    if (HitCount == 1)
    {
        return 100.0f;
    }
    return FirstHitPercent + SecondHitPercent;
}

float UBaseAttackData::CalculateAnimSpeed(float AttackSpeedMultiplier) const
{
    return BaseAnimSpeed * AttackSpeedMultiplier;
}

FString UBaseAttackData::GetAttackSummary() const
{
    FString Summary;

    if (HitCount == 1)
    {
        Summary = TEXT("Single Hit (100%)");
    }
    else
    {
        Summary = FString::Printf(TEXT("Double Hit (%.0f%% + %.0f%%)"), FirstHitPercent, SecondHitPercent);
    }

    Summary += FString::Printf(TEXT(" | Infusion Cost: %.0f"), InfusionEnergyCost);
    Summary += FString::Printf(TEXT(" | Speed: %.2fx"), BaseAnimSpeed);

    return Summary;
}

#if WITH_EDITOR
EDataValidationResult UBaseAttackData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Name validation
    if (AttackName.IsEmpty() || AttackName == TEXT("Unnamed Attack"))
    {
        Context.AddError(FText::FromString(TEXT("Attack must have a unique name")));
        Result = EDataValidationResult::Invalid;
    }

    // Multi-hit damage distribution validation
    if (HitCount == 2)
    {
        float Total = FirstHitPercent + SecondHitPercent;
        if (Total < 80.0f || Total > 120.0f)
        {
            Context.AddWarning(FText::FromString(FString::Printf(
                TEXT("Damage distribution total is %.0f%% (expected ~100%%)"), Total)));
        }

        if (FirstHitPercent <= 0.0f || SecondHitPercent <= 0.0f)
        {
            Context.AddError(FText::FromString(TEXT("Both hits must have positive damage percent")));
            Result = EDataValidationResult::Invalid;
        }
    }

    // Animation validation
    if (AttackMontage == nullptr)
    {
        Context.AddWarning(FText::FromString(TEXT("No attack animation assigned")));
    }

    return Result;
}
#endif