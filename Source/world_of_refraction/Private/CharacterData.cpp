// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterData.h"
#include "RingData.h"

#include "StanceData.h"
#include "WeaponData.h"
#include "EWeaponSlotType.h"
#include "WeaponAttackData.h"
#include "ItemData.h"
#include "StatConstants.h"
#include "LoadoutConstants.h"

// Implementation is mostly in header (inline functions)
// Add any non-inline implementations here if needed

ESpellElement UCharacterData::GetSecondaryElement() const
{
    return (PrimarySlotType == EPrimarySlotType::Evolution && PrimaryEvolution && PrimaryEvolution->GrantsEvolution())
               ? PrimaryEvolution->GetAssociatedElement()
               : ESpellElement::Generic;
}
bool UCharacterData::HasSecondaryElement() const
{
    return IsEvolved() && GetSecondaryElement() != ESpellElement::Generic;
}

bool UCharacterData::IsDualElementCaster() const
{
    return IsCaster() && IsEvolved() && PrimaryEvolution &&
           PrimaryEvolution->GrantsEvolution() &&
           PrimaryEvolution->GetAssociatedElement() != InnateElement;
}

TArray<USpellData *> UCharacterData::GetCombatSpells() const
{
    TArray<USpellData *> CombatSpells;

    // Casters always have access to innate spells
    if (IsCaster())
    {
        CombatSpells.Append(InnateSpells);
    }

    // If evolved, also add evolution spells
    if (IsEvolved() && PrimaryEvolution)
    {
        CombatSpells.Append(PrimaryEvolution->GetSpells());
    }

    return CombatSpells;
}

UItemData *UCharacterData::GetPrimaryEvolution() const
{
    if (PrimarySlotType == EPrimarySlotType::Evolution)
    {
        return PrimaryEvolution;
    }
    return nullptr;
}

bool UCharacterData::HasWeaponAccess() const
{
    // Evolved Casters lose all weapon access
    if (IsEvolved() && CharacterClass == ECharacterClass::Caster)
    {
        return false;
    }

    // Evolved Generic loses secondary only (still has primary)
    // Resonators keep rings
    // Non-evolved characters have normal weapon access
    return true;
}

// ==================== STANCE HELPERS ====================

bool UCharacterData::IsArmed() const
{
    if (CharacterClass == ECharacterClass::Generic)
    {
        // Generic is always armed (Primary or Secondary)
        return true;
    }

    // Caster/Resonator: bUsePrimary = true means armed
    return bUsePrimary;
}

UStanceData *UCharacterData::GetCurrentStance() const
{
    if (IsArmed())
    {
        UWeaponData *ActiveWeapon = GetActiveCharacterWeapon();
        if (ActiveWeapon && ActiveWeapon->WeaponStance)
        {
            return ActiveWeapon->WeaponStance;
        }
    }

    return UnarmedStance;
}

UAnimMontage *UCharacterData::GetCurrentIdleMontage() const
{
    UStanceData *Stance = GetCurrentStance();
    return Stance ? Stance->IdleAnimMontage : nullptr;
}

UWeaponData *UCharacterData::GetActiveWeapon() const
{
    // Evolved Casters have no weapon access
    if (IsCaster() && IsEvolved())
    {
        return nullptr;
    }

    // Evolved Resonators have no weapon access
    if (IsResonator() && IsEvolved())
    {
        return nullptr;
    }

    // Casters with ring primary have no weapon
    if (IsCaster() && PrimarySlotType == EPrimarySlotType::Ring)
    {
        return nullptr;
    }

    // Generic with Ring/Evolution primary - weapon from secondary only
    if (IsGeneric() && PrimarySlotType != EPrimarySlotType::Weapon)
    {
        if (SecondarySlotType == ESecondarySlotType::Weapon)
        {
            return SecondaryWeapon;
        }
        return nullptr;
    }

    // Generic with Weapon primary - use bUsePrimary toggle
    if (IsGeneric() && !IsEvolved())
    {
        if (bUsePrimary)
        {
            return PrimaryWeapon;
        }
        else if (SecondarySlotType == ESecondarySlotType::Weapon)
        {
            return SecondaryWeapon;
        }
    }

    // Default to primary weapon (Caster/Resonator with weapon primary)
    return PrimaryWeapon;
}

UWeaponAttackData *UCharacterData::GetCurrentAttack() const
{
    if (!IsArmed())
    {
        return nullptr;
    }

    UWeaponData *ActiveWeapon = GetActiveCharacterWeapon();
    if (ActiveWeapon && ActiveWeapon->WeaponAttack)
    {
        return ActiveWeapon->WeaponAttack;
    }

    return nullptr;
}

UAnimMontage *UCharacterData::GetCurrentAttackMontage() const
{
    UWeaponAttackData *Attack = GetCurrentAttack();
    return Attack ? Attack->AttackMontage : nullptr;
}

UWeaponData *UCharacterData::GetActiveCharacterWeapon() const
{
    // Generic uses bUsePrimary to determine active weapon
    if (IsGeneric())
    {
        if (bUsePrimary)
        {
            return PrimaryWeapon;
        }
        else if (SecondarySlotType == ESecondarySlotType::Weapon)
        {
            return SecondaryWeapon;
        }
        return nullptr;
    }

    // Caster/Resonator: only primary when armed
    return bUsePrimary ? PrimaryWeapon : nullptr;
}

// ==================== EVOLUTION COST FUNCTIONS ====================

bool UCharacterData::CanApplyEvolution(UItemData *EvolutionCrystal) const
{
    if (!EvolutionCrystal || !EvolutionCrystal->GrantsEvolution())
    {
        return false;
    }

    ESpellElement EvolutionElement = EvolutionCrystal->GetAssociatedElement();

    // Special elements may have restrictions
    if (EvolutionElement == ESpellElement::BrokenDarkness || EvolutionElement == ESpellElement::Reality)
    {
        return false;
    }

    switch (CharacterClass)
    {
    case ECharacterClass::Generic:
        return true;

    case ECharacterClass::Caster:
        return true;

    case ECharacterClass::Resonator:
        // Resonator must have at least one ring of the evolution element
        for (URingData *Ring : EquippedRings)
        {
            if (Ring && Ring->GetRingElement() == EvolutionElement)
            {
                return true;
            }
        }
        return false;
    }

    return false;
}

FEvolutionCostResult UCharacterData::CalculateEvolutionCost(UItemData *EvolutionCrystal) const
{
    FEvolutionCostResult Result;
    Result.bCanEvolve = CanApplyEvolution(EvolutionCrystal);

    if (!EvolutionCrystal || !EvolutionCrystal->GrantsEvolution())
    {
        Result.bCanEvolve = false;
        Result.CostDescription = TEXT("Invalid evolution crystal");
        return Result;
    }

    ESpellElement EvolutionElement = EvolutionCrystal->GetAssociatedElement();

    switch (CharacterClass)
    {
    case ECharacterClass::Generic:
        Result.CostDescription = TEXT("Lose secondary weapon slot");
        Result.GainDescription = TEXT("Gain evolution abilities and spells");
        if (SecondaryWeapon)
        {
            Result.Warnings.Add(FString::Printf(
                TEXT("Will lose access to %s"), *SecondaryWeapon->WeaponName));
        }
        break;

    case ECharacterClass::Caster:
        if (EvolutionElement == InnateElement)
        {
            Result.CostDescription = TEXT("No cost (same element)");
            Result.GainDescription = TEXT("Gain evolution abilities");
        }
        else
        {
            Result.CostDescription = TEXT("Lose weapon slot");
            Result.GainDescription = FString::Printf(
                TEXT("Gain %s as second element, can switch between elements"),
                *EvolutionCrystal->GetCrystalName());
            if (PrimaryWeapon)
            {
                Result.Warnings.Add(FString::Printf(
                    TEXT("Will lose access to %s"), *PrimaryWeapon->WeaponName));
            }
        }
        break;

    case ECharacterClass::Resonator:
        Result.CostDescription = FString::Printf(
            TEXT("Locked to %s element only"), *EvolutionCrystal->GetCrystalName());
        Result.GainDescription = TEXT("Gain innate spells (no ring required), can equip 2nd weapon OR 2nd ring");
        Result.Warnings.Add(TEXT("Will lose access to other elements"));
        Result.Warnings.Add(TEXT("Multi-element builds no longer possible"));
        break;
    }

    return Result;
}

FString UCharacterData::GetEvolutionCostDescription(UItemData *EvolutionCrystal) const
{
    if (!EvolutionCrystal || !EvolutionCrystal->GrantsEvolution())
    {
        return TEXT("Invalid evolution crystal");
    }

    ESpellElement EvolutionElement = EvolutionCrystal->GetAssociatedElement();

    switch (CharacterClass)
    {
    case ECharacterClass::Generic:
        return TEXT("Loses secondary weapon, gains abilities + spells");

    case ECharacterClass::Caster:
        return TEXT("Same element: No cost. Different element: Loses weapon, gains dual-element");

    case ECharacterClass::Resonator:
        return FString::Printf(TEXT("Locked to %s, gains innate spells"), *EvolutionCrystal->GetCrystalName());

    default:
        return TEXT("Unknown");
    }
}

// ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
EDataValidationResult UCharacterData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Validate stat budget
    if (!IsValidInitialDistribution())
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("Initial sub-stat distribution (%d) doesn't match budget (%d)"),
            GetInitialSubStatSum(), StatConstants::INITIAL_STAT_BUDGET)));
        Result = EDataValidationResult::Invalid;
    }

    if (!IsValidWorldDistribution())
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("World sub-stat distribution (%d) doesn't match expected (%d)"),
            GetWorldSubStatSum(), GetExpectedWorldPoints())));
        Result = EDataValidationResult::Invalid;
    }

    // Validate class-specific requirements
    switch (CharacterClass)
    {
    case ECharacterClass::Generic:
        if (!PrimaryWeapon)
        {
            Context.AddWarning(FText::FromString(TEXT("Generic character has no primary weapon")));
        }
        if (SecondarySlotType == ESecondarySlotType::Weapon && !SecondaryWeapon)
        {
            Context.AddWarning(FText::FromString(TEXT("Secondary slot set to Weapon but no weapon assigned")));
        }
        if (InnateSpells.Num() > 0)
        {
            Context.AddError(FText::FromString(TEXT("Generic characters cannot have innate spells")));
            Result = EDataValidationResult::Invalid;
        }
        if (EquippedRings.Num() > 0)
        {
            Context.AddError(FText::FromString(TEXT("Generic characters cannot use EquippedRings")));
            Result = EDataValidationResult::Invalid;
        }
        break;

    case ECharacterClass::Caster:
        if (InnateSpells.Num() == 0)
        {
            Context.AddWarning(FText::FromString(TEXT("Caster has no innate spells")));
        }
        if (EquippedRings.Num() > 0)
        {
            Context.AddError(FText::FromString(TEXT("Casters use PrimaryRing, not EquippedRings")));
            Result = EDataValidationResult::Invalid;
        }
        if (SecondarySlotType != ESecondarySlotType::None)
        {
            Context.AddError(FText::FromString(TEXT("Casters cannot have secondary slots")));
            Result = EDataValidationResult::Invalid;
        }
        break;

    case ECharacterClass::Resonator:
        if (EquippedRings.Num() == 0)
        {
            Context.AddWarning(FText::FromString(TEXT("Resonator has no rings equipped")));
        }
        // Ring limits depend on evolution state
        {
            const bool bIsCharEvolved = (PrimarySlotType == EPrimarySlotType::Evolution);
            const int32 MaxRings = bIsCharEvolved ? LoadoutConstants::RESONATOR_RING_SLOTS_EVOLVED
                                                  : LoadoutConstants::RESONATOR_RING_SLOTS_NORMAL;
            const int32 MaxEvolvedRings = bIsCharEvolved ? LoadoutConstants::RESONATOR_MAX_EVOLVED_RINGS_EVOLVED
                                                         : LoadoutConstants::RESONATOR_MAX_EVOLVED_RINGS_NORMAL;

            if (EquippedRings.Num() > MaxRings)
            {
                Context.AddError(FText::FromString(FString::Printf(
                    TEXT("Resonator can only equip %d rings (current: %d)"), MaxRings, EquippedRings.Num())));
                Result = EDataValidationResult::Invalid;
            }

            int32 EvolvedRingCount = 0;
            for (URingData *Ring : EquippedRings)
            {
                if (Ring && Ring->IsEvolved())
                {
                    EvolvedRingCount++;
                }
            }
            if (EvolvedRingCount > MaxEvolvedRings)
            {
                Context.AddError(FText::FromString(FString::Printf(
                    TEXT("Resonator can only equip %d evolved rings (current: %d)"), MaxEvolvedRings, EvolvedRingCount)));
                Result = EDataValidationResult::Invalid;
            }
        }
        if (InnateSpells.Num() > 0)
        {
            Context.AddError(FText::FromString(TEXT("Resonators get spells from rings, not innate")));
            Result = EDataValidationResult::Invalid;
        }
        if (SecondarySlotType != ESecondarySlotType::None)
        {
            Context.AddError(FText::FromString(TEXT("Resonators use EquippedRings, not SecondarySlot")));
            Result = EDataValidationResult::Invalid;
        }
        break;
    }

    return Result;
}
void UCharacterData::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // Update evolution element display
    if (PrimarySlotType == EPrimarySlotType::Evolution && PrimaryEvolution)
    {
        EvolutionElementDisplay = UEnum::GetValueAsString(PrimaryEvolution->GetAssociatedElement());
        EvolutionElementDisplay.RemoveFromStart(TEXT("ESpellElement::"));
    }
    else
    {
        EvolutionElementDisplay = TEXT("None");
    }
}
#endif