// Fill out your copyright notice in the Description page of Project Settings.

#include "SpellData.h"
#include "CharacterData.h"

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

int32 USpellData::CalculateDamage(UCharacterData *Character, bool bUsingElementalMode) const
{
    if (bHasModeToggle)
    {
        return bUsingElementalMode ? CalculateElementalModeDamage(Character) : CalculateRawModeDamage(Character);
    }
    else
    {
        // No toggle - use base damage
        if (!Character)
            return 0;

        float Damage = BaseDamage;

        // Apply requirement penalty
        float RequirementPenalty = CalculateRequirementPenalty(Character);
        Damage *= (1.0f - RequirementPenalty);

        // Apply character's effect damage multiplier (Spirit-based)
        float EffectMultiplier = Character->CalculateEffectDamageMultiplier();
        Damage *= EffectMultiplier;

        return FMath::RoundToInt(Damage);
    }
}

int32 USpellData::CalculateElementalModeDamage(UCharacterData *Character) const
{
    if (!Character)
        return 0;

    float Damage = ElementalModeDamage;

    // Apply requirement penalty
    float RequirementPenalty = CalculateRequirementPenalty(Character);
    Damage *= (1.0f - RequirementPenalty);

    // Apply character's effect damage multiplier (Spirit-based)
    float EffectMultiplier = Character->CalculateEffectDamageMultiplier();
    Damage *= EffectMultiplier;

    return FMath::RoundToInt(Damage);
}

int32 USpellData::CalculateRawModeDamage(UCharacterData *Character) const
{
    if (!Character)
        return 0;

    float Damage = RawModeDamage;

    // Apply requirement penalty
    float RequirementPenalty = CalculateRequirementPenalty(Character);
    Damage *= (1.0f - RequirementPenalty);

    // Raw mode uses raw damage multiplier (Body-based) instead of effect multiplier
    float RawDamageMultiplier = Character->CalculateRawDamageMultiplier();
    Damage *= RawDamageMultiplier;

    return FMath::RoundToInt(Damage);
}

// ==================== ENERGY CALCULATIONS ====================

int32 USpellData::CalculateEnergyCost(UCharacterData *Character, bool bUsingElementalMode) const
{
    if (bHasModeToggle)
    {
        return bUsingElementalMode ? CalculateElementalModeEnergyCost(Character) : CalculateRawModeEnergyCost(Character);
    }
    else
    {
        // No toggle - use base cost
        if (!Character)
            return BaseEnergyCost;

        float Cost = BaseEnergyCost;

        // Requirement penalty increases cost
        float RequirementPenalty = CalculateRequirementPenalty(Character);
        Cost *= (1.0f + RequirementPenalty);

        return FMath::RoundToInt(Cost);
    }
}

int32 USpellData::CalculateElementalModeEnergyCost(UCharacterData *Character) const
{
    if (!Character)
        return ElementalModeEnergyCost;

    float Cost = ElementalModeEnergyCost;

    // Requirement penalty increases cost
    float RequirementPenalty = CalculateRequirementPenalty(Character);
    Cost *= (1.0f + RequirementPenalty);

    return FMath::RoundToInt(Cost);
}

int32 USpellData::CalculateRawModeEnergyCost(UCharacterData *Character) const
{
    if (!Character)
        return RawModeEnergyCost;

    float Cost = RawModeEnergyCost;

    // Requirement penalty increases cost
    float RequirementPenalty = CalculateRequirementPenalty(Character);
    Cost *= (1.0f + RequirementPenalty);

    return FMath::RoundToInt(Cost);
}

// ==================== STATUS BUILDUP ====================

int32 USpellData::CalculateStatusBuildup(UCharacterData *Character) const
{
    if (!Character)
        return 0;

    // Only elemental mode applies status (or non-toggle spells with school type)
    if (!bHasModeToggle && (School == ESpellSchool::Enhancement || School == ESpellSchool::Restoration))
    {
        // Buffs/heals don't build status
        return 0;
    }

    // Base buildup per hit
    float BuildupPerHit = bHasModeToggle ? ElementalModeStatusBuildup : CombatConstants::BASE_STATUS_BUILDUP_PER_HIT;

    // Scale with character's Effect Damage multiplier (Spirit stat)
    float EffectMultiplier = Character->CalculateEffectDamageMultiplier();
    BuildupPerHit *= EffectMultiplier;

    // Multiply by hit count
    float TotalBuildup = BuildupPerHit * HitCount;

    return FMath::RoundToInt(TotalBuildup);
}

// ==================== HELPER FUNCTIONS ====================

bool USpellData::CanCharacterCast(UCharacterData *Character) const
{
    if (!Character)
        return false;

    // Generic element cannot cast any spells
    if (Character->InnateElement == ESpellElement::Generic)
    {
        return false;
    }

    // Universal spells can be cast by any non-Generic element
    if (bIsUniversalSpell)
    {
        return true;
    }

    // Regular spells: check element match
    if (Character->InnateElement != Element)
    {
        return false; // Element locked!
    }

    return true; // Can attempt (might have penalties)
}

FString USpellData::GetDisplayName(UCharacterData *Caster) const
{
    if (bIsUniversalSpell && bPrependElementName && Caster)
    {
        FString ElementName = UEnum::GetValueAsString(Caster->InnateElement);
        ElementName.RemoveFromStart(TEXT("ESpellElement::"));
        return FString::Printf(TEXT("%s %s"), *ElementName, *SpellName);
    }
    return SpellName;
}

// ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
EDataValidationResult USpellData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Validate mode toggle configuration
    if (bHasModeToggle)
    {
        if (ElementalModeDamage < 0 || RawModeDamage < 0)
        {
            Context.AddError(FText::FromString(TEXT("Mode damage values cannot be negative")));
            Result = EDataValidationResult::Invalid;
        }

        if (ElementalModeEnergyCost < 0 || RawModeEnergyCost < 0)
        {
            Context.AddError(FText::FromString(TEXT("Mode energy costs cannot be negative")));
            Result = EDataValidationResult::Invalid;
        }
    }
    else
    {
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