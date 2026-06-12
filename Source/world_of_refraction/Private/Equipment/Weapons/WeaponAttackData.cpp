// WeaponAttackData.cpp
// Implementation of WeaponAttackData functions.

#include "Equipment/Weapons/WeaponAttackData.h"
#include "Equipment/Weapons/WeaponData.h"

// ==================== MIGRATION ====================

void UWeaponAttackData::PostLoad()
{
    Super::PostLoad();

    // D2: mirror the legacy montage into the unified base field. Triggered only
    // while SkillMontage is unauthored; AttackMontage stays the runtime source
    // of truth until the Stage 12 reader switch. Transient until resaved.
    if (!SkillMontage && AttackMontage)
    {
        SkillMontage = AttackMontage;
    }
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

    // Animation validation — SkillMontage is the unified field (D2 reader
    // switch); PostLoad mirrors the legacy AttackMontage into it, so a null
    // here means neither was authored.
    if (SkillMontage == nullptr)
    {
        Context.AddWarning(FText::FromString(TEXT("No attack animation assigned (SkillMontage)")));
    }

    return Result;
}
// CanEditChange override removed (Stage 12 SC7) — DeliveryType/ProjectileSpeed
// are DeprecatedProperty now, hidden from the panel everywhere.
#endif
