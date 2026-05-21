// CrystalIdentity.h
// Free-function helpers operating on FCrystalId. Returns the derived data a
// caller needs about a crystal — element, max durability, display name —
// without holding any UItemData / UEvolutionItemData asset pointer.
//
// Sits one layer above CrystalTypeHelpers: where CrystalTypeHelpers operates
// on raw ECrystalType, CrystalIdentity operates on the (Type, Tier) pair via
// FCrystalId — the API surface that subsequent refactor commits consume.

#pragma once

#include "CoreMinimal.h"
#include "FCrystalId.h"
#include "CrystalTypeHelpers.h"
#include "ESpellElement.h"
#include "ItemTier.h"
#include "Durabilityconstants.h"

namespace CrystalIdentity
{
    /** Returns the spell element this crystal grants. Pure delegation to
     *  CrystalTypeHelpers::GetElement — accepts FCrystalId for caller
     *  convenience and so future evolution of the lookup (e.g. tier-dependent
     *  elements) has a single migration point. */
    inline ESpellElement GetElement(const FCrystalId &Id)
    {
        return CrystalTypeHelpers::GetElement(Id.Type);
    }

    /** Returns max durability for the crystal's tier (F=30 … S=100, per
     *  DurabilityConstants). Tier-only — does not vary by Type. */
    inline int32 GetMaxDurability(const FCrystalId &Id)
    {
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
        return FString::Printf(TEXT("%s (%s)"),
                               *GetTypeName(Id.Type),
                               *GetTierLetter(Id.Tier));
    }
}
