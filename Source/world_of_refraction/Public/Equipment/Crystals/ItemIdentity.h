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
#include "Equipment/Crystals/StoneRowTable.h"

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
            return ESpellElement::None;
        }
        return CrystalTypeHelpers::GetElement(Id.Type);
    }

    /** Returns the primary effect category for this crystal type
     *  (Damage / Healing / EnergyRestore / GrantBonusTurn / BuffDefense /
     *  BuffCrit / Silence / Gamble / Cleanse / StatusClear); a stat stone's comes from
     *  its StoneRowTable row (BuffLuck, BuffReflex, ...). Type-only — Id.Tier is ignored. */
    inline EItemEffectType GetItemEffectType(const FCrystalId &Id)
    {
        if (Id.Type == ECrystalType::None)
        {
            return EItemEffectType::Damage;
        }
        // Stat stones: their consumable's effect is in the stone row table (B-021).
        if (const FStoneRow *StoneRow = StoneRowTable::Find(Id.Type))
        {
            return StoneRow->ItemEffect;
        }
        switch (Id.Type)
        {
        case ECrystalType::Garnet:
            return EItemEffectType::Damage;
        case ECrystalType::Sapphire:
            // Defy-death (Last Stand on a living target / revive on a dead one) — routes to
            // ExecuteDefyDeathEffect. Repointed off Healing (AI-1) so detection can tell it apart
            // from the Healing Stone's real heal (RestoreHealth).
            return EItemEffectType::DefyDeath;
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
        case ECrystalType::AbilityStone:
            // Attach-only — no consumable effect, so UseItem dispatch finds no
            // case and falls to its "not a usable consumable" default arm.
            return EItemEffectType::None;
        case ECrystalType::DurabilityStone:
            // Attach-only — durability is a fusion-attachment property, not a transient
            // combat buff, so there is no meaningful consumable form. Like AbilityStone,
            // returns None: UseItem dispatch finds no case and falls to its "not a usable
            // consumable" default arm. No Execute…Effect handler, no new EItemEffectType.
            return EItemEffectType::None;
        case ECrystalType::HealingStone:
            // Consume-only instant heal (any target) — routes to ExecuteHealingStoneEffect.
            // Distinct from Sapphire (EItemEffectType::DefyDeath → defy-death handler).
            return EItemEffectType::RestoreHealth;
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
        // Stones: spaced display form ("DamageStone" -> "Damage Stone"). Gems are
        // single words already and fall through to the literal switch.
        if (CrystalTypeHelpers::IsAugmentStoneType(Type))
        {
            return FName::NameToDisplayString(
                StaticEnum<ECrystalType>()->GetAuthoredNameStringByValue(static_cast<int64>(Type)), false);
        }
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
        case ECrystalType::HealingStone:
            return TEXT("HealingStone");
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
