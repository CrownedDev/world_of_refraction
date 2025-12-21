// StatusDisplayNames.h
// Display name and color mapping for element-agnostic status types

#pragma once

#include "CoreMinimal.h"
#include "EStatusType.h"
#include "ESpellElement.h"

/**
 * Status display information for UI
 */
struct FStatusDisplayInfo
{
    FString Name;
    FString Description;
    FLinearColor Color;
};

/**
 * Maps generic status types + elements to player-facing display names
 * Example: DOT + Fire = "Burn", DOT + Lightning = "Shocked"
 */
namespace StatusDisplayNames
{
    /** Get display name for status type + element combination */
    inline FString GetDisplayName(EStatusType StatusType, ESpellElement Element)
    {
        switch (StatusType)
        {
        case EStatusType::DOT:
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

        case EStatusType::SpeedDebuff:
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

        case EStatusType::DefenseDebuff:
            switch (Element)
            {
            case ESpellElement::Fire:
                return TEXT("Scorched");
            case ESpellElement::Earth:
                return TEXT("Corroded");
            case ESpellElement::Darkness:
                return TEXT("Cursed");
            case ESpellElement::Lightning:
                return TEXT("Shattered");
            case ESpellElement::Generic:
                return TEXT("Armor Break"); // Physical Pierce
            default:
                return TEXT("Weakened");
            }

        case EStatusType::SkipTurn:
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

        case EStatusType::EnergyDebuff:
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

        case EStatusType::CritDebuff:
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

        case EStatusType::RandomDebuff:
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

        // All other status types use their enum display name
        default:
        {
            const UEnum *EnumPtr = StaticEnum<EStatusType>();
            FString EnumName = EnumPtr ? EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(StatusType)).ToString() : TEXT("Unknown");
            return EnumName;
        }
        }
    }

    /** Get full display info (name, description, color) */
    inline FStatusDisplayInfo GetDisplayInfo(EStatusType StatusType, ESpellElement Element)
    {
        FStatusDisplayInfo Info;
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
        case EStatusType::DOT:
            Info.Description = FString::Printf(TEXT("%s damage each turn"), *Info.Name);
            break;
        case EStatusType::SpeedDebuff:
            Info.Description = TEXT("Reduced movement speed");
            break;
        case EStatusType::DefenseDebuff:
            Info.Description = TEXT("Reduced defense");
            break;
        case EStatusType::CritDebuff:
            Info.Description = TEXT("Reduced crit chance");
            break;
        case EStatusType::EnergyDebuff:
            Info.Description = TEXT("Energy generation locked");
            break;
        case EStatusType::SkipTurn:
            Info.Description = TEXT("Cannot act next turn");
            break;
        case EStatusType::RandomDebuff:
            Info.Description = TEXT("Random stat reduction");
            break;
        case EStatusType::BurstDamage:
            Info.Description = TEXT("Burst damage when triggered");
            break;
        default:
            Info.Description = Info.Name;
            break;
        }

        return Info;
    }
}
