// CrystalIdentity.h
// Free-function helpers operating on FCrystalId. Returns the derived data a
// caller needs about a crystal — element, max durability, display name —
// without holding any UEvolutionItemData / UEvolutionItemData asset pointer.
//
// Sits one layer above CrystalTypeHelpers: where CrystalTypeHelpers operates
// on raw ECrystalType, CrystalIdentity operates on the (Type, Tier) pair via
// FCrystalId — the API surface that subsequent refactor commits consume.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Inventory/ItemTier.h"
#include "Inventory/ItemEffectType.h"
#include "Equipment/Durability/DurabilityConstants.h"

namespace CrystalIdentity
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
     *  (Damage / Healing / EnergyRestore / BuffSpeed / BuffDefense /
     *  BuffCrit / Silence / Gamble / Cleanse / StatusClear). Type-only —
     *  Id.Tier is ignored. */
    inline EItemEffectType GetPrimaryEffectType(const FCrystalId &Id)
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
            return EItemEffectType::BuffSpeed;
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
        case ECrystalType::Whetstone:
            return EItemEffectType::BuffRawDamage;
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
        case ECrystalType::Whetstone:
            return TEXT("Whetstone");
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
