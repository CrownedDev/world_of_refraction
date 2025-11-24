// UltimateData.cpp
// Implementation of UltimateData functions

#include "UltimateData.h"
#include "CharacterData.h"

// ==================== UTILITY FUNCTIONS ====================

FString UUltimateData::GetElementName() const
{
    const UEnum* EnumPtr = StaticEnum<ERefractionElement>();
    if (EnumPtr)
    {
        FString Name = EnumPtr->GetNameStringByValue(static_cast<int64>(Element));
        Name.RemoveFromStart(TEXT("ERefractionElement::"));
        return Name;
    }
    return TEXT("Unknown");
}

FString UUltimateData::GetUltimateTypeName() const
{
    switch (UltimateType)
    {
    case EUltimateType::Damage:
        return TEXT("Single Target Damage");
    case EUltimateType::DamageAOE:
        return TEXT("Area Damage");
    case EUltimateType::CrowdControl:
        return TEXT("Crowd Control");
    case EUltimateType::Buff:
        return TEXT("Buff");
    case EUltimateType::Debuff:
        return TEXT("Debuff");
    case EUltimateType::Heal:
        return TEXT("Healing");
    case EUltimateType::Utility:
        return TEXT("Utility");
    default:
        return TEXT("Unknown");
    }
}

FString UUltimateData::GetCooldownDescription() const
{
    if (CooldownType == EUltimateCooldownType::OncePerBattle)
    {
        return TEXT("Once per battle");
    }
    else
    {
        return FString::Printf(TEXT("%d turn cooldown"), TurnCooldown);
    }
}

FString UUltimateData::GetScalingTypeName() const
{
    switch (ScalingType)
    {
    case EStatScalingType::None:
        return TEXT("None (Fixed)");
    case EStatScalingType::Body:
        return TEXT("Body (Raw Damage)");
    case EStatScalingType::Spirit:
        return TEXT("Spirit (Effect Damage)");
    case EStatScalingType::Mind:
        return TEXT("Mind (Crit Chance + Crit Damage)");
    default:
        return TEXT("Unknown");
    }
}

// ==================== REQUIREMENT FUNCTIONS ====================

bool UUltimateData::MeetsElementRequirement(const UCharacterData* Character) const
{
    if (!Character)
    {
        return false;
    }

    // Generic ultimates can be used by anyone
    if (Element == ERefractionElement::Generic)
    {
        return true;
    }

    // BrokenDarkness characters can use BrokenDarkness, Darkness, and Generic ultimates
    if (Character->InnateElement == ERefractionElement::BrokenDarkness)
    {
        return (Element == ERefractionElement::BrokenDarkness ||
            Element == ERefractionElement::Darkness);
    }

    // All other characters require exact element match
    return Character->InnateElement == Element;
}

bool UUltimateData::MeetsWorldStatRequirements(const UCharacterData* Character) const
{
    if (!Character)
    {
        return false;
    }

    return Character->WorldMindLevel >= RequiredWorldMind &&
        Character->WorldBodyLevel >= RequiredWorldBody &&
        Character->WorldSpiritLevel >= RequiredWorldSpirit;
}

bool UUltimateData::CanCharacterUse(const UCharacterData* Character) const
{
    if (!Character)
    {
        return false;
    }

    // Check element compatibility first
    if (!MeetsElementRequirement(Character))
    {
        return false;
    }

    // Check world stat requirements
    return MeetsWorldStatRequirements(Character);
}

FString UUltimateData::GetRequirementsString() const
{
    FString Result = TEXT("");

    // Element
    Result += FString::Printf(TEXT("Element: %s\n"), *GetElementName());

    // World stats (only show if > 0)
    bool bHasWorldReqs = (RequiredWorldMind > 0 || RequiredWorldBody > 0 || RequiredWorldSpirit > 0);

    if (bHasWorldReqs)
    {
        Result += TEXT("World Stats: ");
        TArray<FString> Reqs;

        if (RequiredWorldMind > 0)
        {
            Reqs.Add(FString::Printf(TEXT("Mind %d"), RequiredWorldMind));
        }
        if (RequiredWorldBody > 0)
        {
            Reqs.Add(FString::Printf(TEXT("Body %d"), RequiredWorldBody));
        }
        if (RequiredWorldSpirit > 0)
        {
            Reqs.Add(FString::Printf(TEXT("Spirit %d"), RequiredWorldSpirit));
        }

        Result += FString::Join(Reqs, TEXT(", "));
    }
    else
    {
        Result += TEXT("World Stats: None");
    }

    return Result;
}

// ==================== CALCULATION FUNCTIONS ====================

float UUltimateData::CalculateDamage(const UCharacterData* Character) const
{
    if (!Character || BaseDamage <= 0)
    {
        return static_cast<float>(BaseDamage);
    }

    float Multiplier = 1.0f;

    switch (ScalingType)
    {
    case EStatScalingType::Body:
        Multiplier = Character->CalculateRawDamageMultiplier();
        break;
    case EStatScalingType::Spirit:
        Multiplier = Character->CalculateEffectDamageMultiplier();
        break;
    case EStatScalingType::Mind:
        // Mind doesn't scale base damage - it scales crit
        Multiplier = 1.0f;
        break;
    case EStatScalingType::None:
    default:
        Multiplier = 1.0f;
        break;
    }

    return BaseDamage * Multiplier;
}

float UUltimateData::CalculateHealing(const UCharacterData* Character) const
{
    if (!Character || BaseHealing <= 0)
    {
        return static_cast<float>(BaseHealing);
    }

    // Healing always scales with Spirit (Effect Damage)
    if (ScalingType == EStatScalingType::Spirit)
    {
        float Multiplier = Character->CalculateEffectDamageMultiplier();
        return BaseHealing * Multiplier;
    }

    return static_cast<float>(BaseHealing);
}

float UUltimateData::CalculateBonusCritChance(const UCharacterData* Character) const
{
    if (!Character || ScalingType != EStatScalingType::Mind)
    {
        return 0.0f;
    }

    // Mind scaling grants +50% of base crit chance as bonus
    float BaseCrit = Character->CalculateCriticalChance();
    return BaseCrit * 0.5f;
}

float UUltimateData::CalculateCritMultiplier(const UCharacterData* Character) const
{
    // Base crit multiplier
    const float BaseCritMultiplier = 1.5f;

    if (!Character || ScalingType != EStatScalingType::Mind)
    {
        return BaseCritMultiplier;
    }

    // Mind scaling enhances crit damage: 1.5x base + (EffectiveMind * 0.02)
    float EffectiveMind = Character->GetEffectiveMind();
    return BaseCritMultiplier + (EffectiveMind * 0.02f);
}

// ==================== VALIDATION ====================

#if WITH_EDITOR
EDataValidationResult UUltimateData::IsDataValid(TArray<FText>& ValidationErrors)
{
    EDataValidationResult Result = Super::IsDataValid(ValidationErrors);

    // Name validation
    if (UltimateName.IsEmpty() || UltimateName == TEXT("Unnamed Ultimate"))
    {
        ValidationErrors.Add(FText::FromString(TEXT("Ultimate must have a unique name")));
        Result = EDataValidationResult::Invalid;
    }

    // Energy cost validation
    if (EnergyCost <= 0)
    {
        ValidationErrors.Add(FText::FromString(TEXT("Ultimate should have an energy cost > 0")));
        Result = EDataValidationResult::Invalid;
    }

    // Damage ultimates need damage
    if ((UltimateType == EUltimateType::Damage || UltimateType == EUltimateType::DamageAOE) && BaseDamage <= 0)
    {
        ValidationErrors.Add(FText::FromString(TEXT("Damage ultimate must have BaseDamage > 0")));
        Result = EDataValidationResult::Invalid;
    }

    // Heal ultimates need healing
    if (UltimateType == EUltimateType::Heal && BaseHealing <= 0)
    {
        ValidationErrors.Add(FText::FromString(TEXT("Heal ultimate must have BaseHealing > 0")));
        Result = EDataValidationResult::Invalid;
    }

    // Buff/Debuff need duration and percentage
    if ((UltimateType == EUltimateType::Buff || UltimateType == EUltimateType::Debuff))
    {
        if (EffectPercentage <= 0.0f)
        {
            ValidationErrors.Add(FText::FromString(TEXT("Buff/Debuff ultimate must have EffectPercentage > 0")));
            Result = EDataValidationResult::Invalid;
        }
        if (EffectDuration <= 0)
        {
            ValidationErrors.Add(FText::FromString(TEXT("Buff/Debuff ultimate must have EffectDuration > 0")));
            Result = EDataValidationResult::Invalid;
        }
    }

    // CC needs duration
    if (UltimateType == EUltimateType::CrowdControl && EffectDuration <= 0)
    {
        ValidationErrors.Add(FText::FromString(TEXT("Crowd Control ultimate must have EffectDuration > 0")));
        Result = EDataValidationResult::Invalid;
    }

    // Turn cooldown validation
    if (CooldownType == EUltimateCooldownType::TurnBased && TurnCooldown <= 0)
    {
        ValidationErrors.Add(FText::FromString(TEXT("Turn-based cooldown must be > 0")));
        Result = EDataValidationResult::Invalid;
    }

    // Secondary effect validation
    if (bHasSecondaryEffect && SecondaryEffectType == EAbilityEffectType::None)
    {
        ValidationErrors.Add(FText::FromString(TEXT("Secondary effect enabled but no type selected")));
        Result = EDataValidationResult::Invalid;
    }

    // World stat requirements validation (0-7 range)
    if (RequiredWorldMind > 7 || RequiredWorldBody > 7 || RequiredWorldSpirit > 7)
    {
        ValidationErrors.Add(FText::FromString(TEXT("World stat requirements cannot exceed 7")));
        Result = EDataValidationResult::Invalid;
    }

    return Result;
}
#endif