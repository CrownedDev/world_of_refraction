// ItemIdentity.h
// Free-function helpers operating on FCrystalId. Returns the derived data a
// caller needs about a crystal — element, max durability, display name —
// without holding any UEvolutionItemData / UEvolutionItemData asset pointer.
//
// Sits one layer above CrystalTypeHelpers: where CrystalTypeHelpers operates
// on raw ECrystalType, ItemIdentity operates on the (Type, Tier) pair via
// FCrystalId — the API surface that subsequent refactor commits consume.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Inventory/ItemTier.h"
#include "Inventory/ItemEffectType.h"
#include "Equipment/Durability/DurabilityConstants.h"
#include "Equipment/Crystals/AugmentStoneConstants.h"

namespace ItemIdentity
{
    /** Returns the spell element this crystal grants. Pure delegation to
     *  CrystalTypeHelpers::GetElement — accepts FCrystalId for caller
     *  convenience and so future evolution of the lookup (e.g. tier-dependent
     *  elements) has a single migration point. */
    inline ESpellElement GetElement(const FCrystalId &Id)
    {
        if (Id.Type == ECrystalType::None)
        {
            return ESpellElement::Generic;
        }
        return CrystalTypeHelpers::GetElement(Id.Type);
    }

    /** Returns the primary effect category for this crystal type
     *  (Damage / Healing / EnergyRestore / GrantBonusTurn / BuffDefense /
     *  BuffCrit / Silence / Gamble / Cleanse / StatusClear). Type-only —
     *  Id.Tier is ignored. */
    inline EItemEffectType GetItemEffectType(const FCrystalId &Id)
    {
        if (Id.Type == ECrystalType::None)
        {
            return EItemEffectType::Damage;
        }
        switch (Id.Type)
        {
        case ECrystalType::Garnet:
            return EItemEffectType::Damage;
        case ECrystalType::Sapphire:
            return EItemEffectType::Healing;
        case ECrystalType::Citrine:
            return EItemEffectType::EnergyRestore;
        case ECrystalType::Emerald:
            return EItemEffectType::GrantBonusTurn;
        case ECrystalType::Amber:
            return EItemEffectType::BuffDefense;
        case ECrystalType::Opal:
            return EItemEffectType::BuffCrit;
        case ECrystalType::Onyx:
            return EItemEffectType::Silence;
        case ECrystalType::Amethyst:
            return EItemEffectType::Gamble;
        case ECrystalType::Iolite:
            return EItemEffectType::Cleanse;
        case ECrystalType::Quartz:
            return EItemEffectType::StatusClear;
        case ECrystalType::DamageStone:
            return EItemEffectType::BuffRawDamage;
        case ECrystalType::AbilityStone:
            // Attach-only — no consumable effect, so UseItem dispatch finds no
            // case and falls to its "not a usable consumable" default arm.
            return EItemEffectType::None;
        case ECrystalType::DefenseStone:
            // Directional defense consumable — routes to ExecuteDefenseBuffEffect
            // (the same handler Amber uses). Magnitude/hook land in later clusters.
            return EItemEffectType::BuffDefense;
        case ECrystalType::CritStone:
            // Crit-DAMAGE consumable (5f-C) — routes to ExecuteCritDamageBuffEffect (DIRECTIONAL:
            // ally ModifyCritDamage buff / enemy CritDamageDebuff), matching the attached CritStone's
            // crit-DAMAGE form. Opal stays on BuffCrit (crit CHANCE) below.
            return EItemEffectType::BuffCritDamage;
        case ECrystalType::TurnSpeedStone:
            // Directional turn-speed consumable — routes to ExecuteTurnSpeedStoneEffect
            // (pure-stat buff/debuff, stone magnitude, NO turn mechanic; separate from
            // Emerald's GrantBonusTurn handler). Buff an ally's turn speed / debuff an enemy's.
            return EItemEffectType::BuffTurnSpeed;
        case ECrystalType::StatusStone:
            // Directional status consumable — routes to ExecuteStatusBuffEffect (transient
            // StatusMultiplierBuff/Debuff, stone magnitude; the Step-5b layer, separate
            // from the attached form's base-stat getter). Buff an ally / debuff an enemy.
            return EItemEffectType::BuffStatusMultiplier;
        case ECrystalType::EfficiencyStone:
            // Directional efficiency consumable — routes to ExecuteEfficiencyBuffEffect
            // (transient EfficiencyBuff/Debuff, stone magnitude; folded into the unified
            // getter so it reaches BD/EP/durability). Buff an ally / debuff an enemy.
            return EItemEffectType::BuffEfficiency;
        case ECrystalType::MaxHPStone:
            // Directional MaxHP consumable — routes to ExecuteMaxHPBuffEffect (transient
            // MaxHPBuff/Debuff, stone magnitude; folded into RecomputeMaxPools via the P2b
            // trigger). Raise an ally's MaxHP / lower an enemy's.
            return EItemEffectType::BuffMaxHP;
        case ECrystalType::MaxEPStone:
            // Directional MaxEP consumable — routes to ExecuteMaxEPBuffEffect (transient
            // MaxEnergyBuff/Debuff). Raise an ally's MaxEP / lower an enemy's.
            return EItemEffectType::BuffMaxEP;
        case ECrystalType::SpellDamageStone:
            // Directional spell-damage consumable — routes to ExecuteSpellDamageBuffEffect
            // (transient SpellDamageBuff/Debuff, stone magnitude; read SPELL-only in
            // GetStatusEffectDamageModifier). The magical mirror of DamageStone's BuffRawDamage.
            return EItemEffectType::BuffSpellDamage;
        case ECrystalType::ResistanceStone:
            // Directional status-resistance consumable — routes to ExecuteResistanceBuffEffect
            // (blanket ModifyStatusResist; ally +mag raises resist, enemy -mag goes negative ->
            // amplified status buildup). Read element-agnostic in StatusBuildupManager's aggregate.
            return EItemEffectType::BuffResistance;
        case ECrystalType::SpellSpeedStone:
            // Directional spell-speed consumable — routes to ExecuteSpellSpeedBuffEffect
            // (SpellSpeedBuff/Debuff; read on the cast-montage PlayRate path). Visual now;
            // combat teeth via the Real-Time Defense Rework.
            return EItemEffectType::BuffSpellSpeed;
        case ECrystalType::ActionSpeedStone:
            // Directional action-speed consumable — routes to ExecuteActionSpeedBuffEffect
            // (ActionSpeedBuff/Debuff; read on the ability/attack montage PlayRate path).
            return EItemEffectType::BuffActionSpeed;
        case ECrystalType::DurabilityStone:
            // Attach-only — durability is a fusion-attachment property, not a transient
            // combat buff, so there is no meaningful consumable form. Like AbilityStone,
            // returns None: UseItem dispatch finds no case and falls to its "not a usable
            // consumable" default arm. No Execute…Effect handler, no new EItemEffectType.
            return EItemEffectType::None;
        case ECrystalType::LuckStone:
            // Directional luck consumable (5f-C) — routes to ExecuteLuckBuffEffect (transient
            // LuckBuff/LuckDebuff, stone magnitude). Buff an ally's luck / debuff an enemy's; lifts
            // all luck consumers (crit chance, break-skip) via GetEquipmentModifiedLuck.
            return EItemEffectType::BuffLuck;
        default:
            return EItemEffectType::Damage;
        }
    }

    /** Returns max durability for the crystal's tier (F=30 … S=100, per
     *  DurabilityConstants). Tier-only — does not vary by Type. */
    inline int32 GetMaxDurability(const FCrystalId &Id)
    {
        if (Id.Type == ECrystalType::None)
        {
            return 0;
        }
        return DurabilityConstants::GetMaxDurabilityForTier(Id.Tier);
    }

    /** Bonus durability a fusion's whole-unit pool gains on top of the gem half's
     *  tier max — keyed on BOTH halves' tiers (symmetric, like the bonus-stat
     *  formula). ((TVA + TVB) * 2) + 1 with TV = F=0..S=6: F+F = 1, S+S = 25.
     *  Added in GetMaxDurability() for an elemental fusion only (augmented fusions
     *  have no gem half and never reach this). */
    inline int32 GetFusionBonusDurability(EItemTier TierA, EItemTier TierB)
    {
        const int32 TVA = TierHelpers::GetTierValue(TierA);
        const int32 TVB = TierHelpers::GetTierValue(TierB);
        return ((TVA + TVB) * 2) + 1;
    }

    /** Flat durability a DurabilityStone half contributes to an elemental fusion's
     *  whole-unit max, ADDED ON TOP of GetFusionBonusDurability (the matrix) — it
     *  does not replace it. Tier-keyed (F=8 .. S=50, even +7 steps) via the flat
     *  DURABILITY_STONE_BONUS curve. 0 for an out-of-range tier. Caller gates on the
     *  half actually being a DurabilityStone; this getter is type-agnostic. */
    inline int32 GetDurabilityStoneBonus(EItemTier Tier)
    {
        const int32 i = TierHelpers::GetTierValue(Tier);
        return (i >= 0 && i < 7) ? AugmentStoneConstants::DURABILITY_STONE_BONUS[i] : 0;
    }

    /** Returns the bare crystal name, e.g. "Garnet", "Sapphire", "Quartz".
     *  Independent of the UMETA display-name strings (which are verbose
     *  "Garnet (Fire - Damage)" forms intended for the editor dropdown). */
    inline FString GetTypeName(ECrystalType Type)
    {
        switch (Type)
        {
        case ECrystalType::Garnet:
            return TEXT("Garnet");
        case ECrystalType::Sapphire:
            return TEXT("Sapphire");
        case ECrystalType::Citrine:
            return TEXT("Citrine");
        case ECrystalType::Emerald:
            return TEXT("Emerald");
        case ECrystalType::Amber:
            return TEXT("Amber");
        case ECrystalType::Opal:
            return TEXT("Opal");
        case ECrystalType::Onyx:
            return TEXT("Onyx");
        case ECrystalType::Amethyst:
            return TEXT("Amethyst");
        case ECrystalType::Iolite:
            return TEXT("Iolite");
        case ECrystalType::Quartz:
            return TEXT("Quartz");
        case ECrystalType::DamageStone:
            return TEXT("DamageStone");
        case ECrystalType::AbilityStone:
            return TEXT("AbilityStone");
        case ECrystalType::DefenseStone:
            return TEXT("DefenseStone");
        case ECrystalType::CritStone:
            return TEXT("CritStone");
        case ECrystalType::TurnSpeedStone:
            return TEXT("TurnSpeedStone");
        case ECrystalType::StatusStone:
            return TEXT("StatusStone");
        case ECrystalType::EfficiencyStone:
            return TEXT("EfficiencyStone");
        case ECrystalType::MaxHPStone:
            return TEXT("MaxHPStone");
        case ECrystalType::MaxEPStone:
            return TEXT("MaxEPStone");
        case ECrystalType::SpellDamageStone:
            return TEXT("SpellDamageStone");
        case ECrystalType::ResistanceStone:
            return TEXT("ResistanceStone");
        case ECrystalType::SpellSpeedStone:
            return TEXT("SpellSpeedStone");
        case ECrystalType::ActionSpeedStone:
            return TEXT("ActionSpeedStone");
        case ECrystalType::DurabilityStone:
            return TEXT("DurabilityStone");
        default:
            return TEXT("Unknown");
        }
    }

    /** Returns the single-letter tier name, e.g. "F", "E", … "S".
     *  Delegates to TierHelpers::GetTierName (declared in ItemTier.h) so the
     *  tier-letter table has a single source of truth. */
    inline FString GetTierLetter(EItemTier Tier)
    {
        return TierHelpers::GetTierName(Tier);
    }

    /** Returns formatted display name "Type (Tier)", e.g. "Garnet (F)",
     *  "Sapphire (S)". Built from GetTypeName + GetTierLetter. */
    inline FString GetDisplayName(const FCrystalId &Id)
    {
        if (Id.Type == ECrystalType::None)
        {
            return TEXT("(empty)");
        }
        return FString::Printf(TEXT("%s (%s)"),
                               *GetTypeName(Id.Type),
                               *GetTierLetter(Id.Tier));
    }

    /** Returns a stable int32 source identifier for use with
     *  SkillEffectManager / FActiveSkillEffect dedup keys. Replaces
     *  `Item->GetUniqueID()` calls in UItemExecutor with a FCrystalId-keyed
     *  equivalent. Two slots holding the same (Type, Tier) intentionally
     *  share the same SourceID — preserves the singleton-DA dedup outcome
     *  (UPrimaryDataAsset is loaded once per path per session, so
     *  Item->GetUniqueID() returned the same value for every use of e.g.
     *  Sapphire (F) within a session). */
    inline int32 GetEffectSourceID(const FCrystalId &Id)
    {
        if (Id.Type == ECrystalType::None)
        {
            return 0;
        }
        return static_cast<int32>(GetTypeHash(Id));
    }
}
