// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilityData.h"
#include "CharacterData.h"

// ==================== REQUIREMENT CHECKS ====================

bool UAbilityData::MeetsRequirements(UCharacterData *Character) const
{
    if (!Character)
        return false;
    return GetTotalDeficit(Character) == 0;
}

int32 UAbilityData::GetTotalDeficit(UCharacterData *Character) const
{
    if (!Character)
        return 0;

    int32 MindDeficit = FMath::Max(0, RequiredMind - Character->GetBaseMind());
    int32 BodyDeficit = FMath::Max(0, RequiredBody - Character->GetBaseBody());
    int32 SpiritDeficit = FMath::Max(0, RequiredSpirit - Character->GetBaseSpirit());

    return MindDeficit + BodyDeficit + SpiritDeficit;
}

float UAbilityData::CalculateRequirementPenalty(UCharacterData *Character) const
{
    int32 Deficit = GetTotalDeficit(Character);
    if (Deficit == 0)
        return 0.0f;

    // Square root scaling: sqrt(deficit) × scale factor
    float Penalty = FMath::Sqrt(static_cast<float>(Deficit)) * CombatConstants::REQUIREMENT_PENALTY_SCALE;
    return FMath::Clamp(Penalty, 0.0f, CombatConstants::REQUIREMENT_PENALTY_MAX);
}

// ==================== DAMAGE CALCULATIONS ====================

int32 UAbilityData::CalculateDamage(UCharacterData *Character, bool bIsInfused) const
{
    return bIsInfused ? CalculateInfusedDamage(Character) : CalculateNormalDamage(Character);
}

int32 UAbilityData::CalculateNormalDamage(UCharacterData *Character) const
{
    if (!Character)
        return 0;

    // Start with base damage
    float Damage = BaseDamage;

    // Apply requirement penalty
    float RequirementPenalty = CalculateRequirementPenalty(Character);
    Damage *= (1.0f - RequirementPenalty);

    // Apply character's raw damage multiplier (Body stat)
    float RawDamageMultiplier = Character->CalculateRawDamageMultiplier();
    Damage *= RawDamageMultiplier;

    return FMath::RoundToInt(Damage);
}

int32 UAbilityData::CalculateInfusedDamage(UCharacterData *Character) const
{
    if (!Character)
        return 0;

    // Start with base damage
    float Damage = BaseDamage;

    // Apply requirement penalty
    float RequirementPenalty = CalculateRequirementPenalty(Character);
    Damage *= (1.0f - RequirementPenalty);

    // Apply infusion damage penalty (global constant)
    Damage *= (1.0f - CombatConstants::INFUSION_DAMAGE_PENALTY);

    // Apply character's raw damage multiplier
    float RawDamageMultiplier = Character->CalculateRawDamageMultiplier();
    Damage *= RawDamageMultiplier;

    return FMath::RoundToInt(Damage);
}

// ==================== ENERGY CALCULATIONS ====================

int32 UAbilityData::CalculateEnergyCost(UCharacterData *Character, bool bIsInfused) const
{
    return bIsInfused ? CalculateInfusedEnergyCost(Character) : CalculateNormalEnergyCost(Character);
}

int32 UAbilityData::CalculateNormalEnergyCost(UCharacterData *Character) const
{
    if (!Character)
        return BaseEnergyCost;

    float Cost = BaseEnergyCost;

    // Requirement penalty increases cost
    float RequirementPenalty = CalculateRequirementPenalty(Character);
    Cost *= (1.0f + RequirementPenalty);

    return FMath::RoundToInt(Cost);
}

int32 UAbilityData::CalculateInfusedEnergyCost(UCharacterData *Character) const
{
    if (!Character)
        return BaseEnergyCost;

    float Cost = BaseEnergyCost;

    // Requirement penalty increases cost
    float RequirementPenalty = CalculateRequirementPenalty(Character);
    Cost *= (1.0f + RequirementPenalty);

    // Infusion multiplier
    Cost *= CombatConstants::INFUSION_ENERGY_MULTIPLIER;

    return FMath::RoundToInt(Cost);
}

// ==================== STATUS BUILDUP ====================

int32 UAbilityData::CalculateStatusBuildup(UCharacterData *Character) const
{
    if (!Character || !bCanBeInfused)
        return 0;

    // Base buildup per hit
    float BuildupPerHit = CombatConstants::BASE_STATUS_BUILDUP_PER_HIT;

    // Scale with character's Effect Damage multiplier (Spirit stat)
    float EffectMultiplier = Character->CalculateEffectDamageMultiplier();
    BuildupPerHit *= EffectMultiplier;

    // Multiply by hit count
    float TotalBuildup = BuildupPerHit * HitCount;

    return FMath::RoundToInt(TotalBuildup);
}

// ==================== HELPER FUNCTIONS ====================

FString UAbilityData::GetRequirementSummary(UCharacterData *Character) const
{
    if (!Character)
        return TEXT("Invalid Character");

    FString Summary = TEXT("");

    // Check each requirement
    if (RequiredMind > 0)
    {
        int32 CharMind = Character->GetBaseMind();
        FString Status = (CharMind >= RequiredMind) ? TEXT("✓") : FString::Printf(TEXT("✗ -%d"), RequiredMind - CharMind);
        Summary += FString::Printf(TEXT("Mind: %d/%d %s\n"), CharMind, RequiredMind, *Status);
    }

    if (RequiredBody > 0)
    {
        int32 CharBody = Character->GetBaseBody();
        FString Status = (CharBody >= RequiredBody) ? TEXT("✓") : FString::Printf(TEXT("✗ -%d"), RequiredBody - CharBody);
        Summary += FString::Printf(TEXT("Body: %d/%d %s\n"), CharBody, RequiredBody, *Status);
    }

    if (RequiredSpirit > 0)
    {
        int32 CharSpirit = Character->GetBaseSpirit();
        FString Status = (CharSpirit >= RequiredSpirit) ? TEXT("✓") : FString::Printf(TEXT("✗ -%d"), RequiredSpirit - CharSpirit);
        Summary += FString::Printf(TEXT("Spirit: %d/%d %s\n"), CharSpirit, RequiredSpirit, *Status);
    }

    if (Summary.IsEmpty())
    {
        Summary = TEXT("No requirements");
    }

    return Summary;
}

// ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
EDataValidationResult UAbilityData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Validate base values
    if (BaseDamage < 0)
    {
        Context.AddError(FText::FromString(TEXT("Base Damage cannot be negative")));
        Result = EDataValidationResult::Invalid;
    }

    if (BaseEnergyCost < 0)
    {
        Context.AddError(FText::FromString(TEXT("Base Energy Cost cannot be negative")));
        Result = EDataValidationResult::Invalid;
    }

    if (HitCount < 0)
    {
        Context.AddError(FText::FromString(TEXT("Hit Count cannot be negative")));
        Result = EDataValidationResult::Invalid;
    }

    // If it's a damage ability, must have at least 1 hit
    if (BaseDamage > 0 && HitCount < 1)
    {
        Context.AddError(FText::FromString(TEXT("Damage abilities must have at least 1 hit")));
        Result = EDataValidationResult::Invalid;
    }

    // Validate requirements
    if (RequiredMind < 0 || RequiredBody < 0 || RequiredSpirit < 0)
    {
        Context.AddError(FText::FromString(TEXT("Requirements cannot be negative")));
        Result = EDataValidationResult::Invalid;
    }

    return Result;
}
#endif