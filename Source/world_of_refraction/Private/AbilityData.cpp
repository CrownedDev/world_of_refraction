// AbilityData.cpp
// Ability Data Asset implementation

#include "AbilityData.h"
#include "CharacterData.h"

// ==================== REQUIREMENT CHECKS ====================

bool UAbilityData::MeetsRequirements(UCharacterData* Character) const
{
    if (!Character)
        return false;
    return Requirements.MeetsRequirements(Character);
}

int32 UAbilityData::GetTotalDeficit(UCharacterData* Character) const
{
    if (!Character)
        return 0;

    return Requirements.GetTotalDeficit(Character);
}

float UAbilityData::CalculateRequirementPenalty(UCharacterData* Character) const
{
    if (!Character)
        return 0.0f;

    return Requirements.CalculatePenalty(Character);
}

// ==================== DAMAGE CALCULATIONS ====================

int32 UAbilityData::CalculateDamage(UCharacterData* Character, bool bIsInfused) const
{
    return bIsInfused ? CalculateInfusedDamage(Character) : CalculateNormalDamage(Character);
}

int32 UAbilityData::CalculateNormalDamage(UCharacterData* Character) const
{
    if (!Character)
        return 0;

    // Start with base damage
    float Damage = BaseDamage;

    // Apply requirement penalty
    float RequirementPenalty = CalculateRequirementPenalty(Character);
    Damage *= (1.0f - RequirementPenalty);

    // Apply character's raw damage multiplier (Body stat)
    float RawDamageMultiplier = Character->CalculateRawDamage();
    Damage *= RawDamageMultiplier;

    return FMath::RoundToInt(Damage);
}

int32 UAbilityData::CalculateInfusedDamage(UCharacterData* Character) const
{
    if (!Character)
        return 0;

    // Start with base damage
    float Damage = BaseDamage;

    // Apply requirement penalty
    float RequirementPenalty = CalculateRequirementPenalty(Character);
    Damage *= (1.0f - RequirementPenalty);

    // Element-infusion damage penalty removed per locked cost matrix.

    // Apply character's raw damage multiplier
    float RawDamageMultiplier = Character->CalculateRawDamage();
    Damage *= RawDamageMultiplier;

    return FMath::RoundToInt(Damage);
}

// ==================== ENERGY CALCULATIONS ====================

int32 UAbilityData::CalculateEnergyCost(UCharacterData* Character, bool bIsInfused) const
{
    return bIsInfused ? CalculateInfusedEnergyCost(Character) : CalculateNormalEnergyCost(Character);
}

int32 UAbilityData::CalculateNormalEnergyCost(UCharacterData* Character) const
{
    if (!Character)
        return BaseEnergyCost;

    float Cost = BaseEnergyCost;

    // Requirement penalty increases cost
    float RequirementPenalty = CalculateRequirementPenalty(Character);
    Cost *= (1.0f + RequirementPenalty);

    return FMath::RoundToInt(Cost);
}

int32 UAbilityData::CalculateInfusedEnergyCost(UCharacterData* Character) const
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

int32 UAbilityData::CalculateStatusBuildup(UCharacterData* Character) const
{
    if (!Character || bImmuneToInfusion)
        return 0;

    // Base buildup per hit
    float BuildupPerHit = CombatConstants::BASE_STATUS_BUILDUP_PER_HIT;

    // Scale with character's StatusMultiplier (Mind stat in Phase 1; moves to Spirit in Phase 2b).
    float Multiplier = Character->CalculateStatusMultiplier();
    BuildupPerHit *= Multiplier;

    // Multiply by hit count
    float TotalBuildup = BuildupPerHit * HitCount;

    return FMath::RoundToInt(TotalBuildup);
}

// ==================== EFFECT HELPERS ====================

TArray<FSkillEffect> UAbilityData::GetEffectsForCondition(EPassiveTrigger Condition) const
{
    TArray<FSkillEffect> Result;

    for (const FSkillEffect& Effect : Effects)
    {
        if (Effect.Condition == Condition && Effect.IsValid())
        {
            Result.Add(Effect);
        }
    }

    return Result;
}

bool UAbilityData::HasDrainEffect() const
{
    for (const FSkillEffect& Effect : Effects)
    {
        if (Effect.IsDrain())
        {
            return true;
        }
    }
    return false;
}

bool UAbilityData::HasBuffEffects() const
{
    for (const FSkillEffect& Effect : Effects)
    {
        if (Effect.IsBuff())
        {
            return true;
        }
    }
    return false;
}

bool UAbilityData::HasDebuffEffects() const
{
    for (const FSkillEffect& Effect : Effects)
    {
        if (Effect.IsDebuff())
        {
            return true;
        }
    }
    return false;
}

// ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
EDataValidationResult UAbilityData::IsDataValid(FDataValidationContext& Context) const
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

    // Validate effects count
    if (Effects.Num() > LoadoutConstants::MAX_SKILL_EFFECTS)
    {
        Context.AddError(FText::FromString(
            FString::Printf(TEXT("Too many effects (%d). Maximum is %d"),
                            Effects.Num(), LoadoutConstants::MAX_SKILL_EFFECTS)));
        Result = EDataValidationResult::Invalid;
    }

    // Validate each effect
    for (int32 i = 0; i < Effects.Num(); ++i)
    {
        const FSkillEffect& Effect = Effects[i];

        if (Effect.EffectType == EStatusType::None)
        {
            Context.AddWarning(FText::FromString(
                FString::Printf(TEXT("Effect %d has no type set"), i + 1)));
        }

        // Drain effects must have OnHit condition
        if (Effect.DrainPercent > 0.0f && Effect.Condition != EPassiveTrigger::OnHit)
        {
            Context.AddWarning(FText::FromString(
                FString::Printf(TEXT("Effect %d has DrainPercent but condition is not OnHit"), i + 1)));
        }

        // Duration should be > 0 for buffs/debuffs
        if ((Effect.IsBuff() || Effect.IsDebuff()) && Effect.Duration <= 0)
        {
            Context.AddWarning(FText::FromString(
                FString::Printf(TEXT("Effect %d is a buff/debuff but has no duration"), i + 1)));
        }
    }

    // Validate execution type specific fields
    if (ExecutionType == EAbilityExecutionType::Melee)
    {
        if (ExecutionRange <= 0.0f)
        {
            Context.AddWarning(FText::FromString(TEXT("Melee ability has zero or negative execution range")));
        }
    }

    if (ExecutionType == EAbilityExecutionType::Ranged)
    {
        if (ProjectileSpeed < 100.0f)
        {
            Context.AddWarning(FText::FromString(TEXT("Ranged ability has very slow projectile speed")));
        }

        if (!ProjectileVFX)
        {
            Context.AddWarning(FText::FromString(TEXT("Ranged ability has no ProjectileVFX assigned")));
        }
    }

    // Support abilities should have effects
    if (BaseDamage == 0 && Effects.Num() == 0)
    {
        Context.AddWarning(FText::FromString(TEXT("Ability has no damage and no effects - is this intentional?")));
    }

    // Validate animation
    if (!ExecutionMontage)
    {
        Context.AddWarning(FText::FromString(TEXT("No ExecutionMontage assigned")));
    }

    return Result;
}
#endif
