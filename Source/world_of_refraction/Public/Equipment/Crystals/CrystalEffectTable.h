// CrystalEffectTable.h
// Static effect-data lookups keyed by FCrystalId — the consumable item-crystal
// effects (DOT, heal, EP, buffs, status, etc.) and the per-tier BD energy
// bonus. Switch bodies transferred verbatim from UEvolutionItemData in commit 0.5 of
// the crystal/evolution refactor sequence.
//
// All values hardcoded. Code change required to rebalance. Future option to
// migrate to a UCurveTable without changing call sites.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Inventory/ItemTier.h"
#include "Inventory/ItemConstants.h"
#include "Equipment/Crystals/AugmentStoneConstants.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Combat/Actions/ActionStatModifiers.h"

namespace CrystalEffectTable
{
    // ==================== GARNET DOT ====================

    /** Garnet: percent of target MaxHP per turn. 0 for non-Garnet. */
    inline float GetDOTDamagePercent(const FCrystalId &Id)
    {
        if (Id.Type != ECrystalType::Garnet)
        {
            return 0.0f;
        }
        switch (Id.Tier)
        {
        case EItemTier::F_Tier:
            return 5.0f;
        case EItemTier::E_Tier:
            return 7.0f;
        case EItemTier::D_Tier:
            return 9.0f;
        case EItemTier::C_Tier:
            return 12.0f;
        case EItemTier::B_Tier:
            return 16.0f;
        case EItemTier::A_Tier:
            return 20.0f;
        case EItemTier::S_Tier:
            return 30.0f;
        default:
            return 0.0f;
        }
    }

    /** Garnet: DOT duration in turns. 0 for non-Garnet. */
    inline int32 GetDOTDuration(const FCrystalId &Id)
    {
        if (Id.Type != ECrystalType::Garnet)
        {
            return 0;
        }
        switch (Id.Tier)
        {
        case EItemTier::F_Tier:
            return 3;
        case EItemTier::E_Tier:
            return 3;
        case EItemTier::D_Tier:
            return 3;
        case EItemTier::C_Tier:
            return 2;
        case EItemTier::B_Tier:
            return 2;
        case EItemTier::A_Tier:
            return 2;
        case EItemTier::S_Tier:
            return 1;
        default:
            return 0;
        }
    }

    // ==================== SAPPHIRE HEAL ====================

    /** Sapphire: percent of target MaxHP healed. 0 for non-Sapphire. */
    inline float GetHealPercent(const FCrystalId &Id)
    {
        if (Id.Type != ECrystalType::Sapphire)
        {
            return 0.0f;
        }
        switch (Id.Tier)
        {
        case EItemTier::F_Tier:
            return 15.0f;
        case EItemTier::E_Tier:
            return 20.0f;
        case EItemTier::D_Tier:
            return 25.0f;
        case EItemTier::C_Tier:
            return 30.0f;
        case EItemTier::B_Tier:
            return 35.0f;
        case EItemTier::A_Tier:
            return 45.0f;
        case EItemTier::S_Tier:
            return 60.0f;
        default:
            return 0.0f;
        }
    }

    // ==================== CITRINE EP RESTORE ====================

    /** Citrine: percent of target MaxEP restored. 0 for non-Citrine. */
    inline float GetEPRestorePercent(const FCrystalId &Id)
    {
        if (Id.Type != ECrystalType::Citrine)
        {
            return 0.0f;
        }
        switch (Id.Tier)
        {
        case EItemTier::F_Tier:
            return 30.0f;
        case EItemTier::E_Tier:
            return 40.0f;
        case EItemTier::D_Tier:
            return 50.0f;
        case EItemTier::C_Tier:
            return 60.0f;
        case EItemTier::B_Tier:
            return 70.0f;
        case EItemTier::A_Tier:
            return 85.0f;
        case EItemTier::S_Tier:
            return 100.0f;
        default:
            return 0.0f;
        }
    }

    // ==================== EMERALD SPEED ====================

    /** Emerald: turn-speed buff percent. 0 for non-Emerald. Stone×2 uniform curve
     *  (Balance framework); applied multiplicatively as ×(1 + pct/100) by
     *  TurnManager's effective-speed calc. */
    inline float GetSpeedBuffPercent(const FCrystalId &Id)
    {
        if (Id.Type != ECrystalType::Emerald)
        {
            return 0.0f;
        }
        const int32 i = TierHelpers::GetTierValue(Id.Tier);
        if (i < 0 || i >= 7)
        {
            return 0.0f;
        }
        return AugmentStoneConstants::STAT_CRYSTAL_BUFF_PERCENT[i];
    }

    // ==================== SHARED BUFF DURATION ====================

    /** Shared consumable buff/debuff duration curve (turns) by tier: F/E=4,
     *  D/C/B=3, A/S=2. 0 for any unmapped tier. The per-crystal-type gate lives
     *  in each caller (GetCrystal/Gamble/ResistanceDuration). */
    inline int32 GetStandardConsumableDurationForTier(EItemTier Tier)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier:
            return 4;
        case EItemTier::E_Tier:
            return 4;
        case EItemTier::D_Tier:
            return 3;
        case EItemTier::C_Tier:
            return 3;
        case EItemTier::B_Tier:
            return 3;
        case EItemTier::A_Tier:
            return 2;
        case EItemTier::S_Tier:
            return 2;
        default:
            return 0;
        }
    }

    /** Shared duration table for Emerald/Amber/Opal buffs. 0 otherwise. */
    inline int32 GetCrystalDuration(const FCrystalId &Id)
    {
        switch (Id.Type)
        {
        case ECrystalType::Emerald:
        case ECrystalType::Amber:
        case ECrystalType::Opal:
            break;
        default:
            return 0;
        }
        return GetStandardConsumableDurationForTier(Id.Tier);
    }

    // ==================== OPAL CRIT ====================

    /** Opal: crit-chance buff/debuff percent. 0 for non-Opal. Stone×2 uniform
     *  curve (Balance framework): applied multiplicatively as ×(1 + pct/100) by
     *  GetCriticalChance after the crit additive→multiplicative conversion. */
    inline float GetCritBuffPercent(const FCrystalId &Id)
    {
        if (Id.Type != ECrystalType::Opal)
        {
            return 0.0f;
        }
        const int32 i = TierHelpers::GetTierValue(Id.Tier);
        if (i < 0 || i >= 7)
        {
            return 0.0f;
        }
        return AugmentStoneConstants::STAT_CRYSTAL_BUFF_PERCENT[i];
    }

    // ==================== AMETHYST GAMBLE ====================

    /** Amethyst: chance to roll a buff (vs debuff). 0 for non-Amethyst. */
    inline float GetBuffChancePercent(const FCrystalId &Id)
    {
        if (Id.Type != ECrystalType::Amethyst)
        {
            return 0.0f;
        }
        switch (Id.Tier)
        {
        case EItemTier::F_Tier:
            return 10.0f;
        case EItemTier::E_Tier:
            return 20.0f;
        case EItemTier::D_Tier:
            return 30.0f;
        case EItemTier::C_Tier:
            return 40.0f;
        case EItemTier::B_Tier:
            return 50.0f;
        case EItemTier::A_Tier:
            return 60.0f;
        case EItemTier::S_Tier:
            return 70.0f;
        default:
            return 0.0f;
        }
    }

    /** Amethyst: gamble effect magnitude. 0 for non-Amethyst. */
    inline float GetGambleMagnitudePercent(const FCrystalId &Id)
    {
        if (Id.Type != ECrystalType::Amethyst)
        {
            return 0.0f;
        }
        switch (Id.Tier)
        {
        case EItemTier::F_Tier:
            return 10.0f;
        case EItemTier::E_Tier:
            return 15.0f;
        case EItemTier::D_Tier:
            return 20.0f;
        case EItemTier::C_Tier:
            return 25.0f;
        case EItemTier::B_Tier:
            return 30.0f;
        case EItemTier::A_Tier:
            return 35.0f;
        case EItemTier::S_Tier:
            return 40.0f;
        default:
            return 0.0f;
        }
    }

    /** Amethyst: gamble effect duration in turns. 0 for non-Amethyst. */
    inline int32 GetGambleDuration(const FCrystalId &Id)
    {
        if (Id.Type != ECrystalType::Amethyst)
        {
            return 0;
        }
        return GetStandardConsumableDurationForTier(Id.Tier);
    }

    // ==================== IOLITE CLEANSE ====================

    /** Iolite: number of effects to remove (99 = all). 0 for non-Iolite. */
    inline int32 GetEffectsToRemoveCount(const FCrystalId &Id)
    {
        if (Id.Type != ECrystalType::Iolite)
        {
            return 0;
        }
        switch (Id.Tier)
        {
        case EItemTier::F_Tier:
            return 1;
        case EItemTier::E_Tier:
            return 1;
        case EItemTier::D_Tier:
            return 2;
        case EItemTier::C_Tier:
            return 2;
        case EItemTier::B_Tier:
            return 3;
        case EItemTier::A_Tier:
            return 3;
        case EItemTier::S_Tier:
            return 99;
        default:
            return 0;
        }
    }

    // ==================== QUARTZ STATUS CLEAR ====================

    /** Quartz: fraction of status bar cleared. 0 for non-Quartz. */
    inline float GetStatusClearPercent(const FCrystalId &Id)
    {
        if (Id.Type != ECrystalType::Quartz)
        {
            return 0.0f;
        }
        switch (Id.Tier)
        {
        case EItemTier::F_Tier:
            return 25.0f;
        case EItemTier::E_Tier:
            return 35.0f;
        case EItemTier::D_Tier:
            return 45.0f;
        case EItemTier::C_Tier:
            return 55.0f;
        case EItemTier::B_Tier:
            return 65.0f;
        case EItemTier::A_Tier:
            return 80.0f;
        case EItemTier::S_Tier:
            return 100.0f;
        default:
            return 0.0f;
        }
    }

    /** Quartz: elemental resistance duration in turns. 0 for non-Quartz. */
    inline int32 GetResistanceDuration(const FCrystalId &Id)
    {
        if (Id.Type != ECrystalType::Quartz)
        {
            return 0;
        }
        return GetStandardConsumableDurationForTier(Id.Tier);
    }

    // ==================== ELEMENTAL BUILDUP ====================

    /** Shared status-bar buildup table for Garnet/Citrine/Onyx/Amethyst. 0 otherwise. */
    inline float GetElementalBuildupPercent(const FCrystalId &Id)
    {
        switch (Id.Type)
        {
        case ECrystalType::Garnet:
        case ECrystalType::Citrine:
        case ECrystalType::Onyx:
        case ECrystalType::Amethyst:
            break;
        default:
            return 0.0f;
        }
        switch (Id.Tier)
        {
        case EItemTier::F_Tier:
            return 10.0f;
        case EItemTier::E_Tier:
            return 15.0f;
        case EItemTier::D_Tier:
            return 20.0f;
        case EItemTier::C_Tier:
            return 30.0f;
        case EItemTier::B_Tier:
            return 40.0f;
        case EItemTier::A_Tier:
            return 50.0f;
        case EItemTier::S_Tier:
            return 60.0f;
        default:
            return 0.0f;
        }
    }

    // ==================== BUFF PERCENTAGE (nested type+tier table) ====================

    /** Amber (defense): buff magnitude. 0 for any other type. Opal crit and
     *  Emerald speed each have a single live source (GetCritBuffPercent /
     *  GetSpeedBuffPercent); their duplicate arms were removed from this table.
     *  Out-of-line — tier switch body in CrystalEffectTable.cpp. */
    WORLD_OF_REFRACTION_API float GetBuffPercentage(const FCrystalId &Id);

    // ==================== ONYX SILENCE ====================

    /** Onyx: silence magnitude. 0 for non-Onyx. */
    inline float GetSilencePercentage(const FCrystalId &Id)
    {
        if (Id.Type != ECrystalType::Onyx)
        {
            return 0.0f;
        }
        switch (Id.Tier)
        {
        case EItemTier::F_Tier:
            return 15.0f;
        case EItemTier::E_Tier:
            return 30.0f;
        case EItemTier::D_Tier:
            return 30.0f;
        case EItemTier::C_Tier:
            return 50.0f;
        case EItemTier::B_Tier:
            return 70.0f;
        case EItemTier::A_Tier:
            return 70.0f;
        case EItemTier::S_Tier:
            return 100.0f;
        default:
            return 0.0f;
        }
    }

    // ==================== OPAL REVEALS (S-tier only) ====================

    /** Opal+S only: reveals target HP. */
    inline bool GetRevealsHP(const FCrystalId &Id)
    {
        return Id.Type == ECrystalType::Opal && Id.Tier == EItemTier::S_Tier;
    }

    /** Opal+S only: reveals target stats. */
    inline bool GetRevealsStats(const FCrystalId &Id)
    {
        return Id.Type == ECrystalType::Opal && Id.Tier == EItemTier::S_Tier;
    }

    // ==================== BROKEN DARKNESS ENERGY BONUS (Tier-only) ====================

    /** Fraction of the target BD's MaxEP granted as absorption energy when
     *  this crystal is used on them. Tier-scaled (F=0.10 .. S=0.70).
     *  Tier-only — Id.Type is ignored (other than the None early-out). */
    inline float GetBrokenDarknessEnergyPercent(const FCrystalId &Id)
    {
        if (Id.Type == ECrystalType::None || Id.Type == ECrystalType::DamageStone ||
            Id.Type == ECrystalType::AbilityStone || Id.Type == ECrystalType::DefenseStone ||
            Id.Type == ECrystalType::CritStone || Id.Type == ECrystalType::TurnSpeedStone ||
            Id.Type == ECrystalType::StatusStone || Id.Type == ECrystalType::EfficiencyStone ||
            Id.Type == ECrystalType::MaxHPStone || Id.Type == ECrystalType::MaxEPStone)
        {
            return 0.0f;
        }
        switch (Id.Tier)
        {
        case EItemTier::F_Tier:
            return ItemConstants::BD_ENERGY_PERCENT_F;
        case EItemTier::E_Tier:
            return ItemConstants::BD_ENERGY_PERCENT_E;
        case EItemTier::D_Tier:
            return ItemConstants::BD_ENERGY_PERCENT_D;
        case EItemTier::C_Tier:
            return ItemConstants::BD_ENERGY_PERCENT_C;
        case EItemTier::B_Tier:
            return ItemConstants::BD_ENERGY_PERCENT_B;
        case EItemTier::A_Tier:
            return ItemConstants::BD_ENERGY_PERCENT_A;
        case EItemTier::S_Tier:
            return ItemConstants::BD_ENERGY_PERCENT_S;
        default:
            return 0.0f;
        }
    }

    /** General stone base-percent curve, keyed by stone type + tier. The whole
     *  stat-stone family shares ONE curve (Balance framework — uniform numbers):
     *    DamageStone  -> 3/5/7/9/11/13/15 (F..S)
     *    DefenseStone -> 3/5/7/9/11/13/15 (F..S)  (same shared curve)
     *  0 for any non-stat-stone type. */
    inline float GetStoneBasePercent(ECrystalType Type, EItemTier Tier)
    {
        switch (Type)
        {
        case ECrystalType::DamageStone:
        case ECrystalType::DefenseStone:
        case ECrystalType::CritStone:
        case ECrystalType::TurnSpeedStone:
        case ECrystalType::StatusStone:
        case ECrystalType::EfficiencyStone:
        case ECrystalType::MaxHPStone:
        case ECrystalType::MaxEPStone:
            break;
        default:
            return 0.0f;
        }
        const int32 i = TierHelpers::GetTierValue(Tier);
        if (i < 0 || i >= 7)
        {
            return 0.0f;
        }
        return AugmentStoneConstants::STONE_BASE_PERCENT[i];
    }

    /** The sub-stat a stone amplifies. ESubStat::None for non-stat-stones
     *  (AbilityStone and any non-stone). Consumed by GetAttachedStonePercent and
     *  the per-read-site stat-stone hooks. */
    inline ESubStat StoneTargetStat(ECrystalType Type)
    {
        switch (Type)
        {
        case ECrystalType::DamageStone:
            return ESubStat::RawDamage;
        case ECrystalType::DefenseStone:
            return ESubStat::Defense;
        case ECrystalType::CritStone:
            return ESubStat::CritChance;
        case ECrystalType::TurnSpeedStone:
            return ESubStat::TurnSpeed;
        case ECrystalType::StatusStone:
            return ESubStat::StatusMultiplier;
        case ECrystalType::EfficiencyStone:
            return ESubStat::Efficiency;
        default:
            return ESubStat::None;
        }
    }

    /** Percent a stone ATTACHMENT grants for a specific sub-stat — the hook each
     *  read-site calls (e.g. Defense in GetDefenderFlatDefense, RawDamage in the
     *  Step 1.25 damage path). 0 unless the attachment is a augment stone whose
     *  target stat matches Stat. */
    inline float GetAttachedStonePercent(const FRuntimeAttachedItem &Att, ESubStat Stat)
    {
        if (Att.IsFusion())
        {
            // Both halves contribute: each stat-stone half whose target == Stat adds
            // its tiered %; a gem half (StoneTargetStat -> None) contributes nothing.
            // Plus the fusion STAT bonus (TV(A)+TV(B))/2 when it targets Stat. (This
            // /2 STAT bonus is distinct from the Stage-A durability bonus formula.)
            float Total = 0.0f;
            const FCrystalId &A = Att.Fusion.Id.HalfA;
            const FCrystalId &B = Att.Fusion.Id.HalfB;
            if (StoneTargetStat(A.Type) == Stat)
            {
                Total += GetStoneBasePercent(A.Type, A.Tier);
            }
            if (StoneTargetStat(B.Type) == Stat)
            {
                Total += GetStoneBasePercent(B.Type, B.Tier);
            }
            if (Att.Fusion.Id.BonusStat == Stat)
            {
                Total += (TierHelpers::GetTierValue(A.Tier) + TierHelpers::GetTierValue(B.Tier)) / 2.0f;
            }
            return Total;
        }
        if (!Att.IsAugmentStone())
        {
            return 0.0f;
        }
        if (StoneTargetStat(Att.Crystal.Id.Type) != Stat)
        {
            return 0.0f;
        }
        return GetStoneBasePercent(Att.Crystal.Id.Type, Att.Crystal.Id.Tier);
    }

    /** Percent a stone ATTACHMENT grants, keyed by the stone's TYPE rather than an
     *  ESubStat — for pool stones (MaxHP/MaxEP) which deliberately have no ESubStat
     *  representation (Option B). RecomputeMaxPools calls this with MaxHPStone / MaxEPStone.
     *  Mirrors GetAttachedStonePercent's field path; 0 unless Att is exactly StoneType. */
    inline float GetAttachedStonePercentForType(const FRuntimeAttachedItem &Att, ECrystalType StoneType)
    {
        if (Att.IsFusion())
        {
            // Sum any half matching StoneType. No bonus term: the fusion bonus is an
            // ESubStat and pools have no ESubStat (Option B), so only a literal
            // MaxHP/MaxEP stone half ever feeds a pool, never the bonus.
            float Total = 0.0f;
            const FCrystalId &A = Att.Fusion.Id.HalfA;
            const FCrystalId &B = Att.Fusion.Id.HalfB;
            if (A.Type == StoneType)
            {
                Total += GetStoneBasePercent(StoneType, A.Tier);
            }
            if (B.Type == StoneType)
            {
                Total += GetStoneBasePercent(StoneType, B.Tier);
            }
            return Total;
        }
        return (Att.IsAugmentStone() && Att.Crystal.Id.Type == StoneType)
                   ? GetStoneBasePercent(Att.Crystal.Id.Type, Att.Crystal.Id.Tier)
                   : 0.0f;
    }

    /** Raw-attack-damage increase (%) a Damage Stone grants. Thin wrapper over the
     *  shared GetStoneBasePercent curve, preserving the DamageStone-only guard so
     *  callers see byte-identical values (a DefenseStone Id still returns 0 here —
     *  the general curve is reached via GetStoneBasePercent / GetAttachedStonePercent).
     *  This is the consumable value AND the attached base. */
    inline float GetDamageStoneBasePercent(const FCrystalId &Id)
    {
        if (Id.Type != ECrystalType::DamageStone)
        {
            return 0.0f;
        }
        return GetStoneBasePercent(ECrystalType::DamageStone, Id.Tier);
    }

    /** Skill-slots an attachment grants at its tier — one shared per-tier curve:
     *    AbilityStone -> ability slots,  gem crystal (IsGemType) -> spell slots.
     *    F=2, E=3, D=3, C=4, B=4, A=5, S=6  (non-sequential)
     *  DamageStone -> 0 (grants no slots). Evolution / none -> 0 here; an
     *  evolution attachment keeps the flat spell ceiling, resolved by the caller
     *  (ResolveSpellSlotCap), not this type-keyed table. */
    inline int32 GetAttachmentSlotsForTier(const FCrystalId &Id)
    {
        if (Id.Type == ECrystalType::AbilityStone || CrystalTypeHelpers::IsGemType(Id.Type))
        {
            const int32 i = TierHelpers::GetTierValue(Id.Tier);
            if (i < 0 || i >= 7)
            {
                return 0;
            }
            return AugmentStoneConstants::ATTACHMENT_SLOTS[i];
        }
        // DamageStone and every other type grant no slots.
        return 0;
    }

    /** Spell-slot cap for a spell-capable attachment. Gem crystals gate by tier
     *  (the shared GetAttachmentSlotsForTier curve); evolution — and any other
     *  spell-capable attachment — keep the caller's flat ceiling. The caller
     *  supplies its own flat constant (MAX_SPELL_SLOTS for weapons,
     *  MAX_RING_SPELLS for rings). */
    inline int32 ResolveSpellSlotCap(const FRuntimeAttachedItem &Attachment, int32 FlatCeiling)
    {
        if (Attachment.IsCrystal() && CrystalTypeHelpers::IsGemType(Attachment.Crystal.Id.Type))
        {
            return GetAttachmentSlotsForTier(Attachment.Crystal.Id);
        }
        // An elemental fusion's gem half supplies spell slots exactly like a gem
        // crystal of that half's tier — keyed on the gem half, not the (empty)
        // Crystal slot. An augmented fusion (no gem half) falls through to the flat
        // ceiling, same as any non-gem attachment.
        if (Attachment.IsFusion() && Attachment.Fusion.HasGemHalf())
        {
            return GetAttachmentSlotsForTier(Attachment.Fusion.GemHalf());
        }
        return FlatCeiling;
    }

    /** The FCrystalId whose tier keys an attachment's AUGMENT-STONE ability-slot cap:
     *  a direct augment stone's own Id, or a fusion's AbilityStone half (the only
     *  ability-granting fusion half — other pairs are banned by validation). Returns
     *  a default (None) Id otherwise, for which GetAttachmentSlotsForTier yields 0.
     *  The ability analog of ResolveSpellSlotCap's gem-half resolution. */
    inline FCrystalId AbilityStoneSlotIdentity(const FRuntimeAttachedItem &Att)
    {
        if (Att.IsAugmentStone())
        {
            return Att.Crystal.Id;
        }
        if (Att.IsFusion())
        {
            if (Att.Fusion.Id.HalfA.Type == ECrystalType::AbilityStone)
            {
                return Att.Fusion.Id.HalfA;
            }
            if (Att.Fusion.Id.HalfB.Type == ECrystalType::AbilityStone)
            {
                return Att.Fusion.Id.HalfB;
            }
        }
        return FCrystalId();
    }
}
