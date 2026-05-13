// SpellData.cpp
// Spell Data Asset implementation.

#include "SpellData.h"
#include "CharacterData.h"

// ==================== DAMAGE CALCULATIONS ====================

int32 USpellData::CalculateDamage(UCharacterData *Character, const FActionStatModifiers &ActionMods) const
{
    if (!Character)
        return 0;

    float FinalDamage = BaseDamage;

    // Raw mode: +10% damage bonus
    if (bIsRawMode)
    {
        FinalDamage *= CombatConstants::RAW_MODE_DAMAGE_MULTIPLIER;
    }

    // Apply requirement penalty
    float RequirementPenalty = CalculateRequirementPenalty(Character);
    FinalDamage *= (1.0f - RequirementPenalty);

    // Apply character's StatusMultiplier (Mind-based in Phase 1; Phase 2b switches spells to SpellDamage).
    // ActionMods.StatusMultiplier stacks Reality + Evolution + future per-action buffs.
    float Multiplier = Character->CalculateStatusMultiplier();
    Multiplier = ActionMods.ApplyTo(Multiplier, ESubStat::StatusMultiplier);
    FinalDamage *= Multiplier;

    return FMath::RoundToInt(FinalDamage);
}

int32 USpellData::CalculateStatusBuildup(UCharacterData *Character, const FActionStatModifiers &ActionMods) const
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

    // Scale with character's StatusMultiplier (Mind-based in Phase 1; pillar moves in Phase 2b).
    // ActionMods.StatusMultiplier stacks per-action stat buffs onto the multiplier.
    float Multiplier = Character->CalculateStatusMultiplier();
    Multiplier = ActionMods.ApplyTo(Multiplier, ESubStat::StatusMultiplier);
    BuildupPerHit *= Multiplier;

    // Multiply by hit count
    float TotalBuildup = BuildupPerHit * HitCount;

    return FMath::RoundToInt(TotalBuildup);
}

// ==================== ENERGY CALCULATIONS ====================

int32 USpellData::CalculateEnergyCost(UCharacterData *Character) const
{
    if (!Character)
        return BaseEnergyCost;

    float Cost = BaseEnergyCost;

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
    return Name;
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

    // Validate status buildup (only relevant for elemental mode; sign handled by ClampMin on base)
    if (!bIsRawMode && StatusBuildup < 0)
    {
        Context.AddError(FText::FromString(TEXT("Status Buildup cannot be negative")));
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
