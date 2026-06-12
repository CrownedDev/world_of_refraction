// WeaponAttackData.cpp
// Implementation of WeaponAttackData functions.

#include "Equipment/Weapons/WeaponAttackData.h"
#include "Equipment/Weapons/WeaponData.h"

float UWeaponAttackData::CalculateAnimSpeed(float AnimationSpeedMultiplier) const
{
    return BaseAnimSpeed * AnimationSpeedMultiplier;
}

FString UWeaponAttackData::GetAttackSummary() const
{
    FString Summary;

    if (HitCount == 1)
    {
        Summary = TEXT("Single Hit");
    }
    else
    {
        // Per-hit distribution is authored via DamageSplit on the base (D1).
        Summary = FString::Printf(TEXT("%d Hits"), HitCount);
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
