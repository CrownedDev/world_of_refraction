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
#include "Equipment/Crystals/ItemIdentity.h"
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
            *ItemIdentity::GetTypeName(Id.Type).ToLower());
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
            return FString::Printf(
                TEXT("Defy death. On a LIVING target: Last Stand — wards them for %d turns; if they "
                     "would die within that window they instead survive at 50%% max HP (one death). "
                     "On a DEAD target: revives them at 30%% max HP. (Plain healing moved to the "
                     "Healing Stone.)"),
                CrystalEffectTable::GetLastStandWindow(Id));

        case ECrystalType::Citrine:
            return FString::Printf(
                TEXT("Restores %.0f%% of the target's max energy; overloads the user with Lightning status buildup."),
                CrystalEffectTable::GetEPRestorePercent(Id));

        case ECrystalType::Emerald:
        {
            const int32 BonusTurnDelay = CrystalEffectTable::GetEmeraldBonusTurnDelay(Id);
            if (BonusTurnDelay == 0)
            {
                return TEXT("Sacrifice your turn to grant the target a bonus turn immediately.");
            }
            return FString::Printf(
                TEXT("Sacrifice your turn to grant the target a bonus turn after %d turns."),
                BonusTurnDelay);
        }

        case ECrystalType::Amber:
            return FString::Printf(
                TEXT("Buffs an ally's defense (or debuffs an enemy's) by %.0f%% for %s."),
                CrystalEffectTable::GetAmberDefensePercent(Id),
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

        case ECrystalType::DamageStone:
            return FString::Printf(
                TEXT("Sharpens the weapon to raise raw attack damage by %.0f%% for %s; when attached, also unlocks the weapon's abilities via Resonate."),
                CrystalEffectTable::GetDamageStoneBasePercent(Id),
                *FormatTurns(CombatConstants::AUGMENT_STONE_CONSUMABLE_DURATION));

        case ECrystalType::AbilityStone:
            // Attach-only augment stone; per-tier slot text refines in Cluster 4.
            return TEXT("A augment stone that grants additional ability slots when attached.");

        case ECrystalType::DefenseStone:
            // Magnitude curve lands in C2, the directional consumable in C4 —
            // placeholder text until then.
            return TEXT("A stone that bolsters Defense — passively when attached, or on a target when used.");

        case ECrystalType::CritStone:
            // Placeholder until the attached crit-damage hook lands (CritStone maps to ESubStat::CritDamage
            // post-5e-C3; the attached read is not yet wired into GetCritDamageMultiplier — see 5e flag).
            return TEXT("A stone that raises Crit Damage when attached.");
        case ECrystalType::TurnSpeedStone:
            return TEXT("A stone that raises Turn Speed when attached.");
        case ECrystalType::StatusStone:
            return TEXT("A stone that raises Status Multiplier when attached.");
        case ECrystalType::EfficiencyStone:
            return TEXT("A stone that improves Efficiency when attached.");
        case ECrystalType::MaxHPStone:
            return TEXT("A stone that raises Max HP when attached.");
        case ECrystalType::MaxEPStone:
            return TEXT("A stone that raises Max EP when attached.");

        case ECrystalType::SpellDamageStone:
            return FString::Printf(
                TEXT("Attunes the weapon to raise spell (magical) damage by %.0f%% for %s — passively when attached, or on a target when used. The magical mirror of the Damage Stone."),
                CrystalEffectTable::GetStoneBasePercent(Id.Type, Id.Tier),
                *FormatTurns(CombatConstants::AUGMENT_STONE_CONSUMABLE_DURATION));

        case ECrystalType::ResistanceStone:
            return FString::Printf(
                TEXT("Wards against status buildup. When attached, grants %.0f%% blanket status resistance; when used, raises an ally's resistance or curses an enemy into amplified vulnerability for %s."),
                CrystalEffectTable::GetStoneBasePercent(Id.Type, Id.Tier),
                *FormatTurns(CombatConstants::AUGMENT_STONE_CONSUMABLE_DURATION));

        case ECrystalType::SpellSpeedStone:
            return FString::Printf(
                TEXT("Quickens spellcasting — raises spell speed by %.0f%% for %s. Speeds the cast animation now; tighter defense windows arrive with the real-time defense rework."),
                CrystalEffectTable::GetStoneBasePercent(Id.Type, Id.Tier),
                *FormatTurns(CombatConstants::AUGMENT_STONE_CONSUMABLE_DURATION));

        case ECrystalType::ActionSpeedStone:
            return FString::Printf(
                TEXT("Quickens weapon strikes — raises action speed by %.0f%% for %s. Speeds the attack animation now; tighter defense windows arrive with the real-time defense rework."),
                CrystalEffectTable::GetStoneBasePercent(Id.Type, Id.Tier),
                *FormatTurns(CombatConstants::AUGMENT_STONE_CONSUMABLE_DURATION));

        case ECrystalType::LuckStone:
            return TEXT("A stone that raises Luck when attached — lifting crit chance, crystal-wear "
                        "break-skip, and other luck-driven odds together.");

        case ECrystalType::ReflexStone:
            return TEXT("A stone that raises Reflex when attached — widening the defense input window "
                        "so incoming attacks are easier to react to and block.");

        case ECrystalType::DurabilityStone:
            return FString::Printf(
                TEXT("Reinforces an elemental fusion — adds %d flat durability when fused with a crystal. Inert on its own or in a stone-only fusion."),
                ItemIdentity::GetDurabilityStoneBonus(Id.Tier));

        case ECrystalType::HealingStone:
            return FString::Printf(
                TEXT("Consume to instantly heal any target (ally or enemy) for %.0f%% of their max "
                     "HP. Consume-only — attaching it to a weapon does nothing."),
                CrystalEffectTable::GetHealPercent(Id));

        default:
            return TEXT("Unknown effect.");
        }
    }
}
