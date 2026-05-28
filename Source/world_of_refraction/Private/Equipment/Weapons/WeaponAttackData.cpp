// WeaponAttackData.cpp
// Implementation of WeaponAttackData functions.

#include "Equipment/Weapons/WeaponAttackData.h"
#include "Equipment/Weapons/WeaponData.h"

float UWeaponAttackData::GetHitDamagePercent(int32 HitIndex) const
{
    if (HitCount == 1)
    {
        return (HitIndex == 0) ? 100.0f : 0.0f;
    }

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

float UWeaponAttackData::GetTotalDamagePercent() const
{
    if (HitCount == 1)
    {
        return 100.0f;
    }
    return FirstHitPercent + SecondHitPercent;
}

float UWeaponAttackData::CalculateAnimSpeed(float AnimationSpeedMultiplier) const
{
    return BaseAnimSpeed * AnimationSpeedMultiplier;
}

FString UWeaponAttackData::GetAttackSummary() const
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

    Summary += FString::Printf(TEXT(" | Buildup: %d"), StatusBuildup);
    Summary += FString::Printf(TEXT(" | Energy: %d"), BaseEnergyCost);
    Summary += FString::Printf(TEXT(" | Speed: %.2fx"), BaseAnimSpeed);

    return Summary;
}

#if WITH_EDITOR
EDataValidationResult UWeaponAttackData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Name uniqueness (Super warns if empty; here we keep the old "Unnamed Attack" error)
    if (Name == TEXT("Unnamed Skill") || Name == TEXT("Unnamed Attack"))
    {
        Context.AddError(FText::FromString(TEXT("Attack must have a unique name")));
        Result = EDataValidationResult::Invalid;
    }

    // HitCount was previously hard-clamped to 1-2 via meta. Base only enforces ClampMin=1.
    // Above 2 is unusual but not invalid — flag as a warning, don't reject.
    if (HitCount > 2)
    {
        Context.AddWarning(NSLOCTEXT("WeaponAttackData",
                                     "HitCountWarning",
                                     "HitCount above 2 is unusual for attacks."));
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

bool UWeaponAttackData::CanEditChange(const FProperty *InProperty) const
{
    if (InProperty)
    {
        const FName PropertyName = InProperty->GetFName();
        if (PropertyName == GET_MEMBER_NAME_CHECKED(UCastableSkillDataBase, DeliveryType) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(UCastableSkillDataBase, ProjectileSpeed))
        {
            return false;
        }
    }
    return Super::CanEditChange(InProperty);
}
#endif
