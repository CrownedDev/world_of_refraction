// EStatusType.h
// Unified status types for both physical and elemental attacks

#pragma once

#include "CoreMinimal.h"
#include "ESpellElement.h"
#include "EPhysicalDamageType.h"
#include "EStatusType.generated.h"

/**
 * Unified status effect types.
 * Shared between physical attacks (weapons/abilities) and elemental attacks (spells).
 * Last hit determines what triggers when status bar fills.
 */
UENUM(BlueprintType)
enum class EStatusType : uint8
{
    None UMETA(DisplayName = "None"),

    // ==================== PHYSICAL ====================

    /** Slash weapon damage over time */
    Bleed UMETA(DisplayName = "Bleed (Slash DOT)"),

    /** Pierce weapon defense reduction */
    ArmorBreak UMETA(DisplayName = "Armor Break (Pierce)"),

    /** Blunt weapon skip turn */
    Staggered UMETA(DisplayName = "Staggered (Blunt)"),

    // ==================== ELEMENTAL ====================

    /** Fire damage over time */
    Burn UMETA(DisplayName = "Burn (Fire DOT)"),

    /** Water speed reduction */
    Slow UMETA(DisplayName = "Slow (Water)"),

    /** Wind skip turn */
    Tripped UMETA(DisplayName = "Tripped (Wind)"),

    /** Lightning speed reduction + DOT */
    Shocked UMETA(DisplayName = "Shocked (Lightning)"),

    /** Earth defense reduction */
    Weakened UMETA(DisplayName = "Weakened (Earth)"),

    /** Light crit chance reduction */
    Dimmed UMETA(DisplayName = "Dimmed (Light)"),

    /** Darkness energy lock */
    Silenced UMETA(DisplayName = "Silenced (Darkness)"),

    /** Void random stat reduction */
    Destabilized UMETA(DisplayName = "Destabilized (Void)"),

    // ==================== SPECIAL ====================

    /** Reality / Generic / Raw mode - burst damage */
    RawDamage UMETA(DisplayName = "Raw Damage (Reality/Generic)")
};

/**
 * Helper functions for status type mapping
 */
namespace StatusTypeHelper
{
    /** Get default status type for an element */
    inline EStatusType GetDefaultForElement(ESpellElement Element)
    {
        switch (Element)
        {
        case ESpellElement::Fire:
            return EStatusType::Burn;
        case ESpellElement::Water:
            return EStatusType::Slow;
        case ESpellElement::Earth:
            return EStatusType::Weakened;
        case ESpellElement::Wind:
            return EStatusType::Tripped;
        case ESpellElement::Lightning:
            return EStatusType::Shocked;
        case ESpellElement::Light:
            return EStatusType::Dimmed;
        case ESpellElement::Darkness:
            return EStatusType::Silenced;
        case ESpellElement::Void:
            return EStatusType::Destabilized;
        case ESpellElement::Reality:
            return EStatusType::RawDamage;
        case ESpellElement::Generic:
            return EStatusType::RawDamage;
        default:
            return EStatusType::None;
        }
    }

    /** Get default status type for a physical damage type */
    inline EStatusType GetDefaultForPhysical(EPhysicalDamageType DamageType)
    {
        switch (DamageType)
        {
        case EPhysicalDamageType::Slash:
            return EStatusType::Bleed;
        case EPhysicalDamageType::Pierce:
            return EStatusType::ArmorBreak;
        case EPhysicalDamageType::Blunt:
            return EStatusType::Staggered;
        default:
            return EStatusType::None;
        }
    }

    /** Check if a status type causes skip turn */
    inline bool IsSkipTurnStatus(EStatusType Status)
    {
        return Status == EStatusType::Staggered || Status == EStatusType::Tripped;
    }

    /** Check if a status type is DOT */
    inline bool IsDOTStatus(EStatusType Status)
    {
        return Status == EStatusType::Bleed ||
               Status == EStatusType::Burn ||
               Status == EStatusType::Shocked; // Shocked has DOT component
    }

    /** Get status type from AbilityEffectType (for PrimaryEffect mapping) */
    inline EStatusType GetFromAbilityEffect(EAbilityEffectType EffectType)
    {
        switch (EffectType)
        {
        case EAbilityEffectType::BurnDOT:
            return EStatusType::Burn;
        case EAbilityEffectType::BleedDOT:
            return EStatusType::Bleed;
        case EAbilityEffectType::PoisonDOT:
            return EStatusType::Bleed; // Map poison to bleed for now
        case EAbilityEffectType::ElectrifiedDOT:
            return EStatusType::Shocked;
        case EAbilityEffectType::SpeedDebuff:
            return EStatusType::Slow;
        case EAbilityEffectType::DefenseDebuff:
            return EStatusType::Weakened;
        case EAbilityEffectType::CritChanceDebuff:
            return EStatusType::Dimmed;
        case EAbilityEffectType::EnergyDrain:
            return EStatusType::Silenced;
        case EAbilityEffectType::Stun:
            return EStatusType::Staggered;
        case EAbilityEffectType::DamageDebuff:
            return EStatusType::Destabilized;
        default:
            return EStatusType::None;
        }
    }
}