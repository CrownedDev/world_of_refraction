// Fill out your copyright notice in the Description page of Project Settings.

#include "SpellData.h"
#include "CharacterData.h"
#include "RealityBoost.h"

// ==================== REQUIREMENT CHECKS ====================

int32 USpellData::GetTotalDeficit(const UCharacterData *Character) const
{
    if (!Character)
        return 0;

    // Calculate world stat deficits
    int32 MindDeficit = FMath::Max(0, Requirements.RequiredWorldMind - Character->WorldMindLevel);
    int32 BodyDeficit = FMath::Max(0, Requirements.RequiredWorldBody - Character->WorldBodyLevel);
    int32 SpiritDeficit = FMath::Max(0, Requirements.RequiredWorldSpirit - Character->WorldSpiritLevel);

    return MindDeficit + BodyDeficit + SpiritDeficit;
}

float USpellData::CalculateRequirementPenalty(const UCharacterData *Character) const
{
    if (!Character)
        return 0.0f;

    int32 TotalDeficit = GetTotalDeficit(Character);
    if (TotalDeficit == 0)
        return 0.0f;

    // Same penalty formula, applied to world stat deficit
    float Penalty = FMath::Sqrt(static_cast<float>(TotalDeficit)) * 0.10f;
    return FMath::Min(Penalty, 0.60f); // Cap at 60%
}

// ==================== DAMAGE CALCULATIONS ====================

int32 USpellData::CalculateDamage(UCharacterData *Character, bool bRealityL2Boost) const
{
    if (!Character)
        return 0;

    float FinalDamage = Damage;

    // Raw mode: +10% damage bonus
    if (bIsRawMode)
    {
        FinalDamage *= CombatConstants::RAW_MODE_DAMAGE_MULTIPLIER;
    }

    // Apply requirement penalty
    float RequirementPenalty = CalculateRequirementPenalty(Character);
    FinalDamage *= (1.0f - RequirementPenalty);

    // Apply character's effect damage multiplier (Spirit-based)
    float EffectMultiplier = Character->CalculateEffectDamageMultiplier();
    EffectMultiplier = RealityBoost::ApplyTo(EffectMultiplier, bRealityL2Boost);
    FinalDamage *= EffectMultiplier;

    return FMath::RoundToInt(FinalDamage);
}

int32 USpellData::CalculateStatusBuildup(UCharacterData *Character, bool bRealityL2Boost) const
{
    if (!Character)
        return 0;

    // Raw mode: no status buildup
    if (bIsRawMode)
        return 0;

    // Buffs/heals don't build status
    if (School == ESpellSchool::Enhancement || School == ESpellSchool::Restoration)
        return 0;

    // Base buildup per hit
    float BuildupPerHit = StatusBuildup;

    // Scale with character's Effect Damage multiplier (Spirit stat)
    float EffectMultiplier = Character->CalculateEffectDamageMultiplier();
    EffectMultiplier = RealityBoost::ApplyTo(EffectMultiplier, bRealityL2Boost);
    BuildupPerHit *= EffectMultiplier;

    // Multiply by hit count
    float TotalBuildup = BuildupPerHit * HitCount;

    return FMath::RoundToInt(TotalBuildup);
}

// ==================== ENERGY CALCULATIONS ====================

int32 USpellData::CalculateEnergyCost(UCharacterData *Character) const
{
    if (!Character)
        return EnergyCost;

    float Cost = EnergyCost;

    // Requirement penalty increases cost
    float RequirementPenalty = CalculateRequirementPenalty(Character);
    Cost *= (1.0f + RequirementPenalty);

    return FMath::RoundToInt(Cost);
}

// ==================== HELPER FUNCTIONS ====================

bool USpellData::CanCharacterCast(UCharacterData *Character) const
{
    if (!Character)
        return false;

    // The element-match gate (formerly here, Caster-only) now lives in
    // UActionExecutor::ValidateAction where the active infusion source is
    // available — Reality source bypasses the gate per locked design.
    // This function now answers "is this character of a class that can cast
    // spells at all" — true for all current classes (Generic / Caster / Resonator).
    return true;
}

FString USpellData::GetDisplayName(UCharacterData *Caster) const
{
    return SpellName;
}
// ==================== DEFENSE HELPERS ====================

bool USpellData::CanBeBlocked() const
{
    return DeliveryType != ESpellDeliveryType::Instant;
}

bool USpellData::CanBeParried() const
{
    return DeliveryType == ESpellDeliveryType::Projectile ||
           DeliveryType == ESpellDeliveryType::Homing;
}

bool USpellData::CanBeDodgedByMoving() const
{
    return DeliveryType == ESpellDeliveryType::Projectile;
}

bool USpellData::CanBeDodgedByTiming() const
{
    return DeliveryType == ESpellDeliveryType::Projectile ||
           DeliveryType == ESpellDeliveryType::Homing ||
           DeliveryType == ESpellDeliveryType::Beam;
}

TArray<EDefenseType> USpellData::GetAvailableDefenses() const
{
    TArray<EDefenseType> Options;

    if (DeliveryType == ESpellDeliveryType::Instant)
    {
        return Options; // Empty - unavoidable
    }

    Options.Add(EDefenseType::Block);

    if (CanBeParried())
    {
        Options.Add(EDefenseType::Parry);
    }

    if (CanBeDodgedByMoving() || CanBeDodgedByTiming())
    {
        Options.Add(EDefenseType::Dodge);
    }

    return Options;
}

// ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
EDataValidationResult USpellData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Validate damage
    if (Damage < 0)
    {
        Context.AddError(FText::FromString(TEXT("Damage cannot be negative")));
        Result = EDataValidationResult::Invalid;
    }

    // Validate energy cost
    if (EnergyCost < 0)
    {
        Context.AddError(FText::FromString(TEXT("Energy Cost cannot be negative")));
        Result = EDataValidationResult::Invalid;
    }

    // Validate status buildup (only relevant for elemental mode)
    if (!bIsRawMode && StatusBuildup < 0)
    {
        Context.AddError(FText::FromString(TEXT("Status Buildup cannot be negative")));
        Result = EDataValidationResult::Invalid;
    }

    // Validate hit count
    if (HitCount < 0)
    {
        Context.AddError(FText::FromString(TEXT("Hit Count cannot be negative")));
        Result = EDataValidationResult::Invalid;
    }

    // Validate construct
    if (bIsConstruct)
    {
        if (ConstructedWeapon == nullptr)
        {
            Context.AddError(FText::FromString(TEXT("Construct spell must have a weapon assigned")));
            Result = EDataValidationResult::Invalid;
        }
    }

    return Result;
}
#endif