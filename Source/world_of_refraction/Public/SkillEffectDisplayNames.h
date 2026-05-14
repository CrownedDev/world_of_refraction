// SkillEffectDisplayNames.h
// Display name and color mapping for element-agnostic status types

#pragma once

#include "CoreMinimal.h"
#include "ESkillEffectType.h"
#include "ESpellElement.h"

/**
 * Status display information for UI
 */
struct FSkillEffectDisplayInfo
{
    FString Name;
    FString Description;
    FLinearColor Color;
};

/**
 * Maps generic status types + elements to player-facing display names
 * Example: DOT + Fire = "Burn", DOT + Lightning = "Shocked"
 */
namespace SkillEffectDisplayNames
{
    /** Get display name for status type + element combination */
    inline FString GetDisplayName(ESkillEffectType StatusType, ESpellElement Element)
    {
        switch (StatusType)
        {
        case ESkillEffectType::DOT:
            switch (Element)
            {
            case ESpellElement::Fire:
                return TEXT("Burn");
            case ESpellElement::Water:
                return TEXT("Frost Bite");
            case ESpellElement::Earth:
                return TEXT("Poison");
            case ESpellElement::Lightning:
                return TEXT("Shocked");
            case ESpellElement::Darkness:
                return TEXT("Bleed");
            case ESpellElement::Void:
                return TEXT("Corrupt");
            case ESpellElement::Light:
                return TEXT("Sear");
            case ESpellElement::Wind:
                return TEXT("Lacerate");
            case ESpellElement::Generic:
                return TEXT("Bleed"); // Physical Slash
            default:
                return TEXT("Damage Over Time");
            }

        case ESkillEffectType::SpeedDebuff:
            switch (Element)
            {
            case ESpellElement::Water:
                return TEXT("Frozen");
            case ESpellElement::Earth:
                return TEXT("Rooted");
            case ESpellElement::Lightning:
                return TEXT("Paralyzed");
            case ESpellElement::Darkness:
                return TEXT("Withered");
            case ESpellElement::Void:
                return TEXT("Time Crawl");
            default:
                return TEXT("Slowed");
            }

        case ESkillEffectType::DefenseDebuff:
            switch (Element)
            {
            case ESpellElement::Fire:
                return TEXT("Scorched");
            case ESpellElement::Earth:
                return TEXT("Softened");
            case ESpellElement::Darkness:
                return TEXT("Cursed");
            case ESpellElement::Lightning:
                return TEXT("Shattered");
            case ESpellElement::Generic:
                return TEXT("Armor Break"); // Physical Pierce
            default:
                return TEXT("Weakened");
            }

        case ESkillEffectType::SkipTurn:
            switch (Element)
            {
            case ESpellElement::Wind:
                return TEXT("Tripped");
            case ESpellElement::Lightning:
                return TEXT("Stunned");
            case ESpellElement::Water:
                return TEXT("Frozen Solid");
            case ESpellElement::Generic:
                return TEXT("Staggered"); // Physical Impact
            default:
                return TEXT("Stunned");
            }

        case ESkillEffectType::EnergyDebuff:
            switch (Element)
            {
            case ESpellElement::Darkness:
                return TEXT("Silenced");
            case ESpellElement::Void:
                return TEXT("Drained");
            case ESpellElement::Light:
                return TEXT("Blinded");
            default:
                return TEXT("Energy Locked");
            }

        case ESkillEffectType::CritDebuff:
            switch (Element)
            {
            case ESpellElement::Light:
                return TEXT("Dimmed");
            case ESpellElement::Darkness:
                return TEXT("Blinded");
            case ESpellElement::Void:
                return TEXT("Unfocused");
            default:
                return TEXT("Unfocused");
            }

        case ESkillEffectType::RandomDebuff:
            switch (Element)
            {
            case ESpellElement::Void:
                return TEXT("Destabilized");
            case ESpellElement::Reality:
                return TEXT("Warped");
            case ESpellElement::Darkness:
                return TEXT("Confused");
            default:
                return TEXT("Confused");
            }

        // ==================== BAR-CAP GATE EFFECTS (Session X) ====================
        // Element-specific flavour names can be added later; defaults for now.
        case ESkillEffectType::Stun:
            return TEXT("Stunned");
        case ESkillEffectType::HealBlock:
            return TEXT("Frozen");
        case ESkillEffectType::Silenced:
            return TEXT("Silenced");
        case ESkillEffectType::RandomSkill:
            return TEXT("Voided");

        // ==================== NEW SUB-STAT BUFFS / DEBUFFS ====================
        // Plain defaults; element-flavour overrides can be added later if desired.
        case ESkillEffectType::SpellDamageBuff:
            return TEXT("Empowered");
        case ESkillEffectType::SpellDamageDebuff:
            return TEXT("Weakened Magic");
        case ESkillEffectType::SpellSpeedBuff:
            return TEXT("Quickened Casting");
        case ESkillEffectType::SpellSpeedDebuff:
            return TEXT("Slowed Casting");
        case ESkillEffectType::TurnSpeedBuff:
            return TEXT("Hastened");
        case ESkillEffectType::TurnSpeedDebuff:
            return TEXT("Lagging");
        case ESkillEffectType::LuckBuff:
            return TEXT("Fortunate");
        case ESkillEffectType::LuckDebuff:
            return TEXT("Misfortune");

        // ==================== PASSIVE LAYER (Phase 2) ====================
        // Stat modifiers
        case ESkillEffectType::ModifyDamageDealt:
            return TEXT("Damage Up");
        case ESkillEffectType::ModifyDamageTaken:
            return TEXT("Damage Taken Up");
        case ESkillEffectType::ModifyHealing:
            return TEXT("Healing Up");
        case ESkillEffectType::ModifyCritChance:
            return TEXT("Crit Chance Up");
        case ESkillEffectType::ModifyCritDamage:
            return TEXT("Crit Damage Up");
        case ESkillEffectType::ModifyEnergyCost:
            return TEXT("Energy Cost Up");
        case ESkillEffectType::ModifyTurnSpeed:
            return TEXT("Turn Speed Up");
        case ESkillEffectType::ModifyStatusResist:
            return TEXT("Status Resist Up");

        // Resource changes
        case ESkillEffectType::RestoreHPPercent:
            return TEXT("Restore HP %");
        case ESkillEffectType::RestoreEnergyPercent:
            return TEXT("Restore Energy %");
        case ESkillEffectType::DrainHP:
            return TEXT("Drain HP");
        case ESkillEffectType::DrainEnergy:
            return TEXT("Drain Energy");

        // Defensive
        case ESkillEffectType::DamageReflect:
            return TEXT("Reflect");
        case ESkillEffectType::Lifesteal:
            return TEXT("Lifesteal");
        case ESkillEffectType::AbsorbDamage:
            return TEXT("Absorb");
        case ESkillEffectType::Shield:
            return TEXT("Shield");

        // Counter
        case ESkillEffectType::CounterAttack:
            return TEXT("Counter");

        // Immunities — trigger-type
        case ESkillEffectType::GrantDOTImmunity:
            return TEXT("DOT Immunity");
        case ESkillEffectType::GrantStunImmunity:
            return TEXT("Stun Immunity");
        case ESkillEffectType::GrantSilenceImmunity:
            return TEXT("Silence Immunity");
        case ESkillEffectType::GrantAllStatusImmunity:
            return TEXT("All-Status Immunity");

        // Immunities — per element
        case ESkillEffectType::GrantFireImmunity:
            return TEXT("Fire Immunity");
        case ESkillEffectType::GrantWaterImmunity:
            return TEXT("Water Immunity");
        case ESkillEffectType::GrantEarthImmunity:
            return TEXT("Earth Immunity");
        case ESkillEffectType::GrantWindImmunity:
            return TEXT("Wind Immunity");
        case ESkillEffectType::GrantLightImmunity:
            return TEXT("Light Immunity");
        case ESkillEffectType::GrantDarknessImmunity:
            return TEXT("Darkness Immunity");
        case ESkillEffectType::GrantLightningImmunity:
            return TEXT("Lightning Immunity");
        case ESkillEffectType::GrantVoidImmunity:
            return TEXT("Void Immunity");
        case ESkillEffectType::GrantRealityImmunity:
            return TEXT("Reality Immunity");

        // Status application on trigger
        case ESkillEffectType::ApplyBurnToTarget:
            return TEXT("Apply Burn");
        case ESkillEffectType::ApplyChillToTarget:
            return TEXT("Apply Chill");
        case ESkillEffectType::ApplyStunToTarget:
            return TEXT("Apply Stun");
        case ESkillEffectType::CleanseSelf:
            return TEXT("Cleanse Self");
        case ESkillEffectType::CleanseAllies:
            return TEXT("Cleanse Allies");

        // Special mechanics
        case ESkillEffectType::ExtraAction:
            return TEXT("Extra Action");
        case ESkillEffectType::GuaranteedCrit:
            return TEXT("Guaranteed Crit");
        case ESkillEffectType::IgnoreDefense:
            return TEXT("Ignore Defense");
        case ESkillEffectType::DoubleHit:
            return TEXT("Double Hit");
        case ESkillEffectType::Revive:
            return TEXT("Revive");

        // All other status types use their enum display name
        default:
        {
            const UEnum *EnumPtr = StaticEnum<ESkillEffectType>();
            FString EnumName = EnumPtr ? EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(StatusType)).ToString() : TEXT("Unknown");
            return EnumName;
        }
        }
    }

    /** Get full display info (name, description, color) */
    inline FSkillEffectDisplayInfo GetDisplayInfo(ESkillEffectType StatusType, ESpellElement Element)
    {
        FSkillEffectDisplayInfo Info;
        Info.Name = GetDisplayName(StatusType, Element);

        // Element-based coloring
        switch (Element)
        {
        case ESpellElement::Fire:
            Info.Color = FLinearColor(1.0f, 0.3f, 0.0f);
            break;
        case ESpellElement::Water:
            Info.Color = FLinearColor(0.0f, 0.5f, 1.0f);
            break;
        case ESpellElement::Earth:
            Info.Color = FLinearColor(0.4f, 0.6f, 0.2f);
            break;
        case ESpellElement::Lightning:
            Info.Color = FLinearColor(0.9f, 0.9f, 0.3f);
            break;
        case ESpellElement::Light:
            Info.Color = FLinearColor(1.0f, 1.0f, 0.8f);
            break;
        case ESpellElement::Darkness:
            Info.Color = FLinearColor(0.3f, 0.0f, 0.5f);
            break;
        case ESpellElement::Void:
            Info.Color = FLinearColor(0.5f, 0.0f, 0.5f);
            break;
        case ESpellElement::Wind:
            Info.Color = FLinearColor(0.7f, 1.0f, 0.9f);
            break;
        case ESpellElement::Reality:
            Info.Color = FLinearColor(0.8f, 0.8f, 0.8f);
            break;
        case ESpellElement::Generic:
            Info.Color = FLinearColor::White;
            break;
        default:
            Info.Color = FLinearColor::White;
            break;
        }

        // Generate description based on type
        switch (StatusType)
        {
        case ESkillEffectType::DOT:
            Info.Description = FString::Printf(TEXT("%s damage each turn"), *Info.Name);
            break;
        case ESkillEffectType::SpeedDebuff:
            Info.Description = TEXT("Reduced action speed");
            break;
        case ESkillEffectType::DefenseDebuff:
            Info.Description = TEXT("Reduced defense");
            break;
        case ESkillEffectType::CritDebuff:
            Info.Description = TEXT("Reduced crit chance");
            break;
        case ESkillEffectType::EnergyDebuff:
            Info.Description = TEXT("Energy generation locked");
            break;
        case ESkillEffectType::SkipTurn:
            Info.Description = TEXT("Cannot act next turn");
            break;
        case ESkillEffectType::RandomDebuff:
            Info.Description = TEXT("Random stat reduction");
            break;
        case ESkillEffectType::BurstDamage:
            Info.Description = TEXT("Burst damage when triggered");
            break;
        default:
            Info.Description = Info.Name;
            break;
        }

        return Info;
    }
}
