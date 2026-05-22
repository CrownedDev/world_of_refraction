// FRuntimeAttachedItem.cpp
// Implementations for discriminated-union dispatch helpers. Kept out of the
// header so UEvolutionItemData full-type access doesn't leak into every
// transitive consumer.

#include "FRuntimeAttachedItem.h"
#include "FAttachedItem.h"
#include "EvolutionItemData.h"
#include "CrystalTypeHelpers.h"

bool FRuntimeAttachedItem::IsBroken() const
{
    if (IsEmpty())
    {
        return false;
    }
    if (IsRefined())
    {
        return Refined.IsBroken();
    }
    if (IsEvolution())
    {
        return Evolution.IsBroken();
    }
    return false;
}

bool FRuntimeAttachedItem::CanProvideSpells() const
{
    return !IsEmpty() && !IsBroken();
}

ESpellElement FRuntimeAttachedItem::GetElement() const
{
    if (IsRefined())
    {
        return CrystalIdentity::GetElement(Refined.Id);
    }
    if (IsEvolution() && Evolution.Item)
    {
        return Evolution.Item->GetAssociatedElement();
    }
    return ESpellElement::Generic;
}

int32 FRuntimeAttachedItem::GetCurrentDurability() const
{
    if (IsRefined())
    {
        return Refined.CurrentDurability;
    }
    if (IsEvolution())
    {
        return Evolution.CurrentDurability;
    }
    return 0;
}

int32 FRuntimeAttachedItem::GetMaxDurability() const
{
    if (IsRefined())
    {
        return CrystalIdentity::GetMaxDurability(Refined.Id);
    }
    if (IsEvolution() && Evolution.Item)
    {
        return Evolution.Item->MaxDurability;
    }
    return 0;
}

bool FRuntimeAttachedItem::HasStatModifiers() const
{
    return IsEvolution() && Evolution.Item && Evolution.Item->HasStatModifiers();
}

FString FRuntimeAttachedItem::GetStatModifierSummary() const
{
    if (IsEvolution() && Evolution.Item)
    {
        return Evolution.Item->GetStatModifierSummary();
    }
    return FString();
}

bool FRuntimeAttachedItem::ApplyWear(int32 Amount)
{
    if (IsRefined())
    {
        return Refined.ApplyWear(Amount);
    }
    if (IsEvolution())
    {
        return Evolution.ApplyWear(Amount);
    }
    return false;
}

int32 FRuntimeAttachedItem::RepairBetweenCombats(int32 Amount)
{
    if (IsRefined())
    {
        return Refined.RepairBetweenCombats(Amount);
    }
    if (IsEvolution())
    {
        return Evolution.RepairBetweenCombats(Amount);
    }
    return 0;
}

bool FRuntimeAttachedItem::operator==(const FRuntimeAttachedItem &Other) const
{
    if (Kind != Other.Kind)
    {
        return false;
    }
    switch (Kind)
    {
    case EAttachedItemKind::None:
        return true;
    case EAttachedItemKind::Refined:
        // Durability is per-instance state, not identity — match
        // FCrystalInventoryEntry::operator== semantics (asset-only).
        return Refined.Id == Other.Refined.Id;
    case EAttachedItemKind::Evolution:
        return Evolution.Item == Other.Evolution.Item;
    default:
        return false;
    }
}

FRuntimeAttachedItem FRuntimeAttachedItem::FromAsset(UEvolutionItemData *Crystal)
{
    FRuntimeAttachedItem Result;
    if (!Crystal)
    {
        return Result;
    }

    if (Crystal->bIsEvolutionCrystal)
    {
        Result.Kind = EAttachedItemKind::Evolution;
        Result.Evolution.Item = Crystal;
        Result.Evolution.CurrentDurability = Crystal->MaxDurability;
    }
    else
    {
        Result.Kind = EAttachedItemKind::Refined;
        Result.Refined.Id = FCrystalId(Crystal->CrystalType, Crystal->Tier);
        // Seed from asset's MaxDurability for byte-for-byte parity with
        // FCrystalInventoryEntry::CreateFromCrystal. CrystalIdentity's
        // tier-only lookup would also work; the asset value is the
        // existing source of truth pre-7b.
        Result.Refined.CurrentDurability = Crystal->MaxDurability;
    }

    return Result;
}

FRuntimeAttachedItem FRuntimeAttachedItem::FromAttachedItem(const FAttachedItem &Source)
{
    FRuntimeAttachedItem Result;

    switch (Source.Kind)
    {
    case EAttachedItemKind::Refined:
        Result.Kind = EAttachedItemKind::Refined;
        Result.Refined.Id = FCrystalId(Source.RefinedType, Source.RefinedTier);
        // Refined crystals carry no asset, so there is no per-asset MaxDurability
        // to read as FromAsset does. Seed from the tier-based max the runtime
        // already treats as refined's source of truth (GetMaxDurability and
        // FRefinedAttachment::RepairBetweenCombats both resolve via CrystalIdentity).
        Result.Refined.CurrentDurability = CrystalIdentity::GetMaxDurability(Result.Refined.Id);
        break;

    case EAttachedItemKind::Evolution:
        Result.Kind = EAttachedItemKind::Evolution;
        Result.Evolution.Item = Source.Evolution;
        // Byte-for-byte parity with FromAsset's evolution branch
        // (Crystal->MaxDurability) — Source.Evolution is that same asset.
        Result.Evolution.CurrentDurability = Source.Evolution ? Source.Evolution->MaxDurability : 0;
        break;

    case EAttachedItemKind::None:
    default:
        // Leave Result default-constructed (Empty).
        break;
    }

    return Result;
}
