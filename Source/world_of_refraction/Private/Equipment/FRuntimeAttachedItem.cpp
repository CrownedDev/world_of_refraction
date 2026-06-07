// FRuntimeAttachedItem.cpp
// Implementations for discriminated-union dispatch helpers. Kept out of the
// header so UEvolutionItemData full-type access doesn't leak into every
// transitive consumer.

#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/FAttachedItem.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"

bool FRuntimeAttachedItem::IsBroken() const
{
    if (IsEmpty())
    {
        return false;
    }
    // A weapon stone carries no durability — it can never break, so ability
    // gating (CanProvideSpells) always sees it as intact.
    if (IsWeaponStone())
    {
        return false;
    }
    if (IsCrystal())
    {
        return Crystal.IsBroken();
    }
    if (IsEvolution())
    {
        return Evolution.IsBroken();
    }
    return false;
}

bool FRuntimeAttachedItem::CanProvideSpells() const
{
    // A weapon stone is intact but grants abilities, not spells — never a spell source.
    if (IsWeaponStone())
    {
        return false;
    }
    return !IsEmpty() && !IsBroken();
}

ESpellElement FRuntimeAttachedItem::GetElement() const
{
    // No element — matches the old Refined→CrystalIdentity::GetElement path,
    // which resolved a weapon-stone FCrystalId to Generic via its default arm.
    if (IsWeaponStone())
    {
        return ESpellElement::Generic;
    }
    if (IsCrystal())
    {
        return CrystalIdentity::GetElement(Crystal.Id);
    }
    if (IsEvolution() && Evolution.Item)
    {
        return Evolution.Item->GetAssociatedElement();
    }
    return ESpellElement::Generic;
}

int32 FRuntimeAttachedItem::GetCurrentDurability() const
{
    // A weapon stone has no durability concept — reports 0 (it never wears).
    if (IsWeaponStone())
    {
        return 0;
    }
    if (IsCrystal())
    {
        return Crystal.CurrentDurability;
    }
    if (IsEvolution())
    {
        return Evolution.CurrentDurability;
    }
    return 0;
}

int32 FRuntimeAttachedItem::GetMaxDurability() const
{
    // A weapon stone has no durability concept — reports 0 (it never wears).
    if (IsWeaponStone())
    {
        return 0;
    }
    if (IsCrystal())
    {
        return CrystalIdentity::GetMaxDurability(Crystal.Id);
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
    // A weapon stone never wears — nothing to break.
    if (IsWeaponStone())
    {
        return false;
    }
    if (IsCrystal())
    {
        return Crystal.ApplyWear(Amount);
    }
    if (IsEvolution())
    {
        return Evolution.ApplyWear(Amount);
    }
    return false;
}

int32 FRuntimeAttachedItem::RepairBetweenCombats(int32 Amount)
{
    // A weapon stone never wears, so there is nothing to repair.
    if (IsWeaponStone())
    {
        return 0;
    }
    if (IsCrystal())
    {
        return Crystal.RepairBetweenCombats(Amount);
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
    case EAttachedItemKind::Crystal:
    case EAttachedItemKind::WeaponStone:
        // Durability is per-instance state, not identity — compare by Id
        // only (identity match, not instance state). A weapon stone stores its
        // FCrystalId{sub-type, Tier} in the same Crystal slot.
        return Crystal.Id == Other.Crystal.Id;
    case EAttachedItemKind::Evolution:
        return Evolution.Item == Other.Evolution.Item;
    default:
        return false;
    }
}

FRuntimeAttachedItem FRuntimeAttachedItem::FromAttachedItem(const FAttachedItem &Source)
{
    FRuntimeAttachedItem Result;

    switch (Source.Kind)
    {
    case EAttachedItemKind::Crystal:
        Result.Kind = EAttachedItemKind::Crystal;
        Result.Crystal.Id = FCrystalId(Source.CrystalType, Source.CrystalTier);
        // Refined crystals carry no asset, so there is no per-asset MaxDurability
        // to read as FromAsset does. Seed from the tier-based max the runtime
        // already treats as refined's source of truth (GetMaxDurability and
        // FRefinedAttachment::RepairBetweenCombats both resolve via CrystalIdentity).
        Result.Crystal.CurrentDurability = CrystalIdentity::GetMaxDurability(Result.Crystal.Id);
        break;

    case EAttachedItemKind::Evolution:
        Result.Kind = EAttachedItemKind::Evolution;
        Result.Evolution.Item = Source.Evolution;
        // Byte-for-byte parity with FromAsset's evolution branch
        // (Crystal->MaxDurability) — Source.Evolution is that same asset.
        Result.Evolution.CurrentDurability = Source.Evolution ? Source.Evolution->MaxDurability : 0;
        break;

    case EAttachedItemKind::WeaponStone:
        Result.Kind = EAttachedItemKind::WeaponStone;
        // Identity carrier is the Crystal slot; read the authored sub-type
        // (DamageStone or AbilityStone) + tier, mirroring the Crystal case. No
        // durability — a weapon stone never wears, so seed 0 (do NOT call
        // GetMaxDurability, which is tier-based and nonzero).
        Result.Crystal.Id = FCrystalId(Source.CrystalType, Source.CrystalTier);
        Result.Crystal.CurrentDurability = 0;
        break;

    case EAttachedItemKind::None:
    default:
        // Leave Result default-constructed (Empty).
        break;
    }

    return Result;
}
