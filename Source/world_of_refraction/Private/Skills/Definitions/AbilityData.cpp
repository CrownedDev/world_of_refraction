// AbilityData.cpp
// Ability Data Asset implementation

#include "Skills/Definitions/AbilityData.h"
#include "Character/CharacterData.h"

// ==================== MIGRATION ====================

void UAbilityData::PostLoad()
{
    Super::PostLoad();

    // D2: mirror the legacy montage into the unified base field. Triggered only
    // while SkillMontage is unauthored; ExecutionMontage stays the runtime
    // source of truth until the Stage 12 reader switch. Transient until resaved.
    if (!SkillMontage && ExecutionMontage)
    {
        SkillMontage = ExecutionMontage;
    }
}

// ==================== DAMAGE CALCULATIONS ====================

int32 UAbilityData::CalculateDamage(UCharacterData *Character, bool bIsInfused) const
{
    if (!Character)
        return 0;

    // Attacker-side base only. RawDamage multiplier is applied once downstream
    // by DamageCalculator::CalculateDamage via GetAttackerDamageMultiplier;
    // applying it here as well caused RawDamage² scaling at high Body stats.
    // Infused branch removed — the element-infusion damage penalty was deleted
    // per the locked cost matrix, so bIsInfused no longer affects damage.
    float Damage = BaseDamage;
    const float RequirementPenalty = CalculateRequirementPenalty(Character);
    Damage *= (1.0f - RequirementPenalty);

    return FMath::RoundToInt(Damage);
}

// Deprecated Blueprint forwarders — return the same value as CalculateDamage.
// Retained so existing BP graphs continue to resolve. Remove once graphs migrate.
int32 UAbilityData::CalculateNormalDamage(UCharacterData *Character) const
{
    return CalculateDamage(Character, /*bIsInfused=*/false);
}

int32 UAbilityData::CalculateInfusedDamage(UCharacterData *Character) const
{
    return CalculateDamage(Character, /*bIsInfused=*/true);
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

    float RequirementPenalty = CalculateRequirementPenalty(Character);
    Cost *= (1.0f + RequirementPenalty);

    return FMath::RoundToInt(Cost);
}

int32 UAbilityData::CalculateInfusedEnergyCost(UCharacterData *Character) const
{
    if (!Character)
        return BaseEnergyCost;

    float Cost = BaseEnergyCost;

    float RequirementPenalty = CalculateRequirementPenalty(Character);
    Cost *= (1.0f + RequirementPenalty);

    Cost *= CombatConstants::INFUSION_ENERGY_MULTIPLIER;

    return FMath::RoundToInt(Cost);
}

// ==================== STATUS BUILDUP ====================

int32 UAbilityData::CalculateStatusBuildup(UCharacterData *Character) const
{
    if (!Character || bImmuneToInfusion)
        return 0;

    float BuildupPerHit = CombatConstants::BASE_STATUS_BUILDUP_PER_HIT;

    float Multiplier = Character->CalculateStatusMultiplier();
    BuildupPerHit *= Multiplier;

    float TotalBuildup = BuildupPerHit * HitCount;

    return FMath::RoundToInt(TotalBuildup);
}

// ==================== EDITOR ====================

#if WITH_EDITOR
EDataValidationResult UAbilityData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // If it's a damage ability, must have at least 1 hit (HitCount has ClampMin=1 already)
    if (BaseDamage > 0 && HitCount < 1)
    {
        Context.AddError(FText::FromString(TEXT("Damage abilities must have at least 1 hit")));
        Result = EDataValidationResult::Invalid;
    }

    // Validate each effect's semantics (count cap handled by USkillDataBase)
    for (int32 i = 0; i < Effects.Num(); ++i)
    {
        const FSkillEffect &Effect = Effects[i];

        if (Effect.EffectType == ESkillEffectType::None)
        {
            Context.AddWarning(FText::FromString(
                FString::Printf(TEXT("Effect %d has no type set"), i + 1)));
        }

        if (Effect.DrainPercent > 0.0f && Effect.Condition != ESkillTrigger::OnHit)
        {
            Context.AddWarning(FText::FromString(
                FString::Printf(TEXT("Effect %d has DrainPercent but condition is not OnHit"), i + 1)));
        }

        if ((Effect.IsBuff() || Effect.IsDebuff()) && Effect.Duration <= 0)
        {
            Context.AddWarning(FText::FromString(
                FString::Printf(TEXT("Effect %d is a buff/debuff but has no duration"), i + 1)));
        }
    }

    // Validate execution-type specific fields
    if (ExecutionType == EAbilityExecutionType::Melee)
    {
        if (ExecutionRange <= 0.0f)
        {
            Context.AddWarning(FText::FromString(TEXT("Melee ability has zero or negative execution range")));
        }
    }

    if (ExecutionType == EAbilityExecutionType::Ranged)
    {
        // D6: Cast entries are authoritative when authored; the loose field
        // (deprecated, load-only) is the legacy fallback check.
        if (CastArray.Num() > 0)
        {
            for (const FSkillCastEntry &Entry : CastArray)
            {
                if (Entry.ProjectileSpeed < 100.0f)
                {
                    Context.AddWarning(FText::FromString(TEXT("Ranged ability Cast entry has very slow projectile speed")));
                }
            }
        }
        else if (ProjectileSpeed < 100.0f)
        {
            Context.AddWarning(FText::FromString(TEXT("Ranged ability has very slow projectile speed")));
        }
    }

    // Support abilities should have effects
    if (BaseDamage == 0 && Effects.Num() == 0)
    {
        Context.AddWarning(FText::FromString(TEXT("Ability has no damage and no effects - is this intentional?")));
    }

    // SkillMontage is the unified field (D2 reader switch); PostLoad mirrors
    // the legacy ExecutionMontage into it, so a null here means neither was
    // authored.
    if (!SkillMontage)
    {
        Context.AddWarning(FText::FromString(TEXT("No montage assigned (SkillMontage)")));
    }

    return Result;
}
// CanEditChange override removed (Stage 12 SC7) — DeliveryType/ProjectileSpeed
// are DeprecatedProperty now, hidden from the panel everywhere.
#endif
