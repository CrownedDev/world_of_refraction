// CrystalDescription.h
// Free-function description generators for crystals, keyed by FCrystalId. Lifted
// from UEvolutionItemData::GenerateDescription so any consumer (item crystal,
// refined crystal, evolution crystal) can produce description text from just a
// FCrystalId — no UEvolutionItemData asset pointer required.
//
// Three layers:
//  - GetTierDescriptor: the tier adjective ("crude" … "legendary"), tier-only.
//  - GetCrystalText:    "A {adj} {name} crystal." — the shared flavor sentence
//                       any crystal kind can use.
//  - GetItemEffectText: the mechanical effect sentence with numbers, for item/
//                       refined consumable crystals. Self-contained sentence
//                       ending in ".".
//
// Both GetCrystalText and GetItemEffectText return self-contained sentences
// (trailing "."). Evolution crystals use GetCrystalText for their Description
// field and UEvolutionItemData::GetEvolutionEffectText for their effect text,
// which also returns a self-contained "." sentence — keeping period semantics
// consistent across item / evolution paths.
//
// Numerics come from CrystalEffectTable::* — this layer only formats text on
// top of the existing FCrystalId-keyed value tables.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Inventory/ItemTier.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/CrystalIdentity.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Combat/CombatConstants.h"

namespace CrystalDescription
{
    /** Tier-flavor adjective. F=crude, E=common, D=refined, C=quality,
     *  B=exceptional, A=masterwork, S=legendary. */
    inline FString GetTierDescriptor(EItemTier Tier)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier: return TEXT("crude");
        case EItemTier::E_Tier: return TEXT("common");
        case EItemTier::D_Tier: return TEXT("refined");
        case EItemTier::C_Tier: return TEXT("quality");
        case EItemTier::B_Tier: return TEXT("exceptional");
        case EItemTier::A_Tier: return TEXT("masterwork");
        case EItemTier::S_Tier: return TEXT("legendary");
        default:                return TEXT("unknown");
        }
    }

    /** Shared "what kind of crystal it is" flavor sentence — usable by any
     *  crystal kind. Format: "A {tier-descriptor} {name-lowercase} crystal." */
    inline FString GetCrystalText(const FCrystalId &Id)
    {
        return FString::Printf(
            TEXT("A %s %s crystal."),
            *GetTierDescriptor(Id.Tier),
            *CrystalIdentity::GetTypeName(Id.Type).ToLower());
    }

    /** Mechanical effect sentence for item/refined crystals. Per-CrystalType
     *  switch with S-tier conditional alternates (Sapphire / Emerald / Onyx)
     *  and effect-count branches (Iolite). Reads CrystalEffectTable for
     *  numerics. Pluralises turn counts ("1 turn" vs "N turns") and returns
     *  a self-contained sentence ending in ".". */
    inline FString GetItemEffectText(const FCrystalId &Id)
    {
        auto FormatTurns = [](int32 Count) -> FString
        {
            return FString::Printf(TEXT("%d %s"),
                                   Count,
                                   Count == 1 ? TEXT("turn") : TEXT("turns"));
        };

        switch (Id.Type)
        {
        case ECrystalType::Garnet:
            return FString::Printf(
                TEXT("Applies a fire burn dealing %.0f%% of target's max HP per turn for %s."),
                CrystalEffectTable::GetDOTDamagePercent(Id),
                *FormatTurns(CrystalEffectTable::GetDOTDuration(Id)));

        case ECrystalType::Sapphire:
            if (Id.Tier == EItemTier::S_Tier)
            {
                return TEXT("Revives fallen ally at 30% HP, or heals for 60% max HP.");
            }
            return FString::Printf(
                TEXT("Restores %.0f%% of target's max HP."),
                CrystalEffectTable::GetHealPercent(Id));

        case ECrystalType::Citrine:
            return FString::Printf(
                TEXT("Restores %.0f%% of the target's max energy; overloads the user with Lightning status buildup."),
                CrystalEffectTable::GetEPRestorePercent(Id));

        case ECrystalType::Emerald:
            if (Id.Tier == EItemTier::S_Tier)
            {
                return TEXT("Grants the target an extra turn.");
            }
            return FString::Printf(
                TEXT("Increases turn speed by %.0f%% for %s."),
                CrystalEffectTable::GetSpeedBuffPercent(Id),
                *FormatTurns(CrystalEffectTable::GetCrystalDuration(Id)));

        case ECrystalType::Amber:
            return FString::Printf(
                TEXT("Buffs an ally's defense (or debuffs an enemy's) by %.0f%% for %s."),
                CrystalEffectTable::GetBuffPercentage(Id),
                *FormatTurns(CrystalEffectTable::GetCrystalDuration(Id)));

        case ECrystalType::Opal:
            return FString::Printf(
                TEXT("Buffs an ally's crit chance (or debuffs an enemy's) by %.0f%% for %s."),
                CrystalEffectTable::GetCritBuffPercent(Id),
                *FormatTurns(CrystalEffectTable::GetCrystalDuration(Id)));

        case ECrystalType::Onyx:
            if (Id.Tier == EItemTier::S_Tier)
            {
                return TEXT("Completely silences target for 1 turn.");
            }
            return FString::Printf(
                TEXT("Drains %.0f%% of the target's energy on use."),
                CrystalEffectTable::GetSilencePercentage(Id));

        case ECrystalType::Amethyst:
            return FString::Printf(
                TEXT("%.0f%% chance of a random buff (else a random debuff) at %.0f%% magnitude for %s."),
                CrystalEffectTable::GetBuffChancePercent(Id),
                CrystalEffectTable::GetGambleMagnitudePercent(Id),
                *FormatTurns(CrystalEffectTable::GetGambleDuration(Id)));

        case ECrystalType::Iolite:
        {
            const int32 Count = CrystalEffectTable::GetEffectsToRemoveCount(Id);
            if (Count >= 99)
            {
                return TEXT("Removes all debuffs from an ally, or all buffs from an enemy.");
            }
            return FString::Printf(
                TEXT("Removes up to %d debuff(s) from an ally, or %d buff(s) from an enemy."),
                Count, Count);
        }

        case ECrystalType::Quartz:
            return FString::Printf(
                TEXT("Clears %.0f%% of the target's status bar and grants matching elemental resistance for %s."),
                CrystalEffectTable::GetStatusClearPercent(Id),
                *FormatTurns(CrystalEffectTable::GetResistanceDuration(Id)));

        case ECrystalType::Whetstone:
            return FString::Printf(
                TEXT("Sharpens the weapon to raise raw attack damage by %.0f%% for %s; when attached, also unlocks the weapon's abilities via Resonate."),
                CrystalEffectTable::GetWhetstoneBasePercent(Id),
                *FormatTurns(CombatConstants::WHETSTONE_CONSUMABLE_DURATION));

        case ECrystalType::AbilityStone:
            // Attach-only weapon stone; per-tier slot text refines in Cluster 4.
            return TEXT("A weapon stone that grants additional ability slots when attached.");

        default:
            return TEXT("Unknown effect.");
        }
    }
}
