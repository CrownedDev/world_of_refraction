// SkillEffectDisplayNames.h
// Display name and color mapping for element-agnostic status types

#pragma once

#include "CoreMinimal.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Infusion/ElementColors.h"
#include "WoRGameNames.h"
#include "Skills/Effects/BarCapTriggerResolver.h"

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
 * Example: DOT + Fire = "Fire DoT"; a Fire break = "Fire Break" (GetBreakDisplayName). Q-064a2 (Crown, 2026-11-12).
 */
namespace SkillEffectDisplayNames
{
    /** Get display name for status type + element combination */
    inline FString GetDisplayName(ESkillEffectType StatusType, ESpellElement Element)
    {
        switch (StatusType)
        {
        case ESkillEffectType::DOT:
        {
            // Q-064a2 (Crown, 2026-11-12): every damage over time is "<Element> DoT"; no special names.
            const FText DoTName = WoRGameNames::GetElementDoTName(Element);
            if (!DoTName.IsEmpty())
            {
                return DoTName.ToString();
            }
            return Element == ESpellElement::Generic ? TEXT("Bleed") // Physical Slash: no element (TODO: Crown, Q-064a3)
                                                     : WoRGameNames::GetSkillEffectTypeName(ESkillEffectType::DOT).ToString();
        }

        case ESkillEffectType::SpeedDebuff:
            switch (Element)
            {
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
        case ESkillEffectType::CritChanceDebuff: // the Light break (B-112)
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
            return TEXT("Heal Blocked"); // B-248: Water's break has its own name (D-090; "Water Break", Q-064a2)
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
        case ESkillEffectType::ReflexBuff:
            return TEXT("Sharp Reflexes");
        case ESkillEffectType::ReflexDebuff:
            return TEXT("Dulled Reflexes");

        // ==================== PASSIVE LAYER (Phase 2) ====================
        // Stat modifiers
        case ESkillEffectType::PushBack:
            return TEXT("Push Back");
        case ESkillEffectType::PullForward:
            return TEXT("Pull Forward");
        case ESkillEffectType::CritDamageDebuff:
            return TEXT("Crit Damage Down");

        // Shown as "Reality Immunity" (the shared table has none for it)
        case ESkillEffectType::GrantRealityImmunity:
            return TEXT("Reality Immunity");

        // All other status types use the shared name table (cooked-safe, B-126); B-248 dropped the cases
        // that only repeated it word for word.
        default:
            return WoRGameNames::GetSkillEffectTypeName(StatusType).ToString();
        }
    }

    /** Q-064a2 (Crown, 2026-11-12): what a capped bar applies (USkillEffectManager::ApplyTriggeredSkillEffect). When
     *  StatusType is the element's own bar-cap trigger it is that element's "<Element> Break" (B-248: read off the
     *  trigger it fires, so a moved trigger, D-090, takes its name along); otherwise (Ice's IceBreak on a Water hit,
     *  a physical break) the type's own name. */
    inline FString GetBreakDisplayName(ESkillEffectType StatusType, ESpellElement Element)
    {
        if (Element != ESpellElement::None && BarCapTriggerResolver::ResolveTrigger(Element, EPhysicalDamageType::None, false) == StatusType)
        {
            const FText BreakName = WoRGameNames::GetElementBreakName(Element);
            if (!BreakName.IsEmpty())
            {
                return BreakName.ToString();
            }
        }
        return GetDisplayName(StatusType, Element);
    }

    /** Get full display info (name, description, color).
     *  OBSOLETE (B-248, DRY audit A8): nothing calls it; remove after Crown's PIE pass of B-248 (a Blueprint check
     *  first). Its FString descriptions are against D-061. */
    inline FSkillEffectDisplayInfo GetDisplayInfo(ESkillEffectType StatusType, ESpellElement Element)
    {
        FSkillEffectDisplayInfo Info;
        Info.Name = GetDisplayName(StatusType, Element);

        // B-294 (D-106): the one element table's colour, not a second table.
        Info.Color = ElementColors::GetColorForElement(Element);

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
