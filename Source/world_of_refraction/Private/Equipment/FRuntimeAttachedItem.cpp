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
    // A augment stone carries no durability — it can never break, so ability
    // gating (CanProvideSpells) always sees it as intact.
    if (IsAugmentStone())
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
    if (IsFusion())
    {
        // Elemental fusion wears and can break; augmented fusion never wears
        // (FFusionAttachment::IsBroken returns false when there is no gem half).
        return Fusion.IsBroken();
    }
    return false;
}

bool FRuntimeAttachedItem::CanProvideSpells() const
{
    // A augment stone is intact but grants abilities, not spells — never a spell source.
    if (IsAugmentStone())
    {
        return false;
    }
    // An augmented fusion (two stones, no gem half) carries no element and grants no
    // spells — only an elemental fusion's gem half is a spell source. An elemental
    // fusion (HasGemHalf) passes through to the empty/broken check below.
    if (IsFusion() && !Fusion.HasGemHalf())
    {
        return false;
    }
    return !IsEmpty() && !IsBroken();
}

ESpellElement FRuntimeAttachedItem::GetElement() const
{
    // No element — matches the Refined→ItemIdentity::GetElement path, which
    // resolves an augment-stone FCrystalId to None via its default arm.
    if (IsAugmentStone())
    {
        return ESpellElement::None;
    }
    if (IsCrystal())
    {
        return ItemIdentity::GetElement(Crystal.Id);
    }
    if (IsEvolution() && Evolution.Item)
    {
        return Evolution.Item->GetAssociatedElement();
    }
    if (IsFusion())
    {
        // The gem half drives an elemental fusion's element. Don't assume HalfA is
        // the stone — check both; an augmented fusion (two stones, no gem) is non-elemental (None).
        if (CrystalTypeHelpers::IsGemType(Fusion.Id.HalfA.Type))
        {
            return ItemIdentity::GetElement(Fusion.Id.HalfA);
        }
        if (CrystalTypeHelpers::IsGemType(Fusion.Id.HalfB.Type))
        {
            return ItemIdentity::GetElement(Fusion.Id.HalfB);
        }
        return ESpellElement::None;
    }
    return ESpellElement::None;
}

int32 FRuntimeAttachedItem::GetCurrentDurability() const
{
    // A augment stone has no durability concept — reports 0 (it never wears).
    if (IsAugmentStone())
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
    if (IsFusion())
    {
        return Fusion.CurrentDurability;
    }
    return 0;
}

int32 FRuntimeAttachedItem::GetMaxDurability() const
{
    // A augment stone has no durability concept — reports 0 (it never wears).
    if (IsAugmentStone())
    {
        return 0;
    }
    if (IsCrystal())
    {
        return ItemIdentity::GetMaxDurability(Crystal.Id);
    }
    if (IsEvolution() && Evolution.Item)
    {
        return Evolution.Item->MaxDurability;
    }
    if (IsFusion())
    {
        // Whole-fusion max: gem half's tier max + two-tier bonus (stubbed). 0 for
        // an augmented fusion (no gem). Logic lives on FFusionAttachment (DRY with
        // its RepairBetweenCombats clamp).
        return Fusion.GetMaxDurability();
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
    // A augment stone never wears — nothing to break.
    if (IsAugmentStone())
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
    if (IsFusion())
    {
        // Elemental fusion wears as a unit; augmented fusion is a no-op
        // (FFusionAttachment::ApplyWear gates on having a gem half).
        return Fusion.ApplyWear(Amount);
    }
    return false;
}

int32 FRuntimeAttachedItem::RepairBetweenCombats(int32 Amount)
{
    // A augment stone never wears, so there is nothing to repair.
    if (IsAugmentStone())
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
    if (IsFusion())
    {
        // Augmented fusion is a no-op (FFusionAttachment::RepairBetweenCombats
        // gates on having a gem half).
        return Fusion.RepairBetweenCombats(Amount);
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
    case EAttachedItemKind::AugmentStone:
        // Durability is per-instance state, not identity — compare by Id
        // only (identity match, not instance state). A augment stone stores its
        // FCrystalId{sub-type, Tier} in the same Crystal slot.
        return Crystal.Id == Other.Crystal.Id;
    case EAttachedItemKind::Evolution:
        return Evolution.Item == Other.Evolution.Item;
    case EAttachedItemKind::Fusion:
        // Identity = the fusion's FFusionId (order-agnostic ==); per-instance
        // durability is state, not identity, so it is excluded.
        return Fusion.Id == Other.Fusion.Id;
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
        // FRefinedAttachment::RepairBetweenCombats both resolve via ItemIdentity).
        Result.Crystal.CurrentDurability = ItemIdentity::GetMaxDurability(Result.Crystal.Id);
        break;

    case EAttachedItemKind::Evolution:
        Result.Kind = EAttachedItemKind::Evolution;
        Result.Evolution.Item = Source.Evolution;
        // Byte-for-byte parity with FromAsset's evolution branch
        // (Crystal->MaxDurability) — Source.Evolution is that same asset.
        Result.Evolution.CurrentDurability = Source.Evolution ? Source.Evolution->MaxDurability : 0;
        // Weapon/ring-attached form: seed .Tier from the asset (base). There is no owned-instance
        // source here until the deferred gear-attach op exists, so this stays base tier for now.
        Result.Evolution.Tier = Source.Evolution ? Source.Evolution->Tier : EItemTier::F_Tier;
        break;

    case EAttachedItemKind::AugmentStone:
        Result.Kind = EAttachedItemKind::AugmentStone;
        // Identity carrier is the Crystal slot; read the authored sub-type
        // (DamageStone or AbilityStone) + tier, mirroring the Crystal case. No
        // durability — a augment stone never wears, so seed 0 (do NOT call
        // GetMaxDurability, which is tier-based and nonzero).
        Result.Crystal.Id = FCrystalId(Source.CrystalType, Source.CrystalTier);
        Result.Crystal.CurrentDurability = 0;
        break;

    case EAttachedItemKind::Fusion:
        Result.Kind = EAttachedItemKind::Fusion;
        // Map the six flat authored fields into the runtime fusion identity. The
        // authoring-only bFusionHalfBIsCrystal is NOT stored — gem/stone nature is
        // recovered at runtime via CrystalTypeHelpers::IsGemType on each half.
        Result.Fusion.Id.HalfA     = FCrystalId(Source.FusionHalfAType, Source.FusionHalfATier);
        Result.Fusion.Id.HalfB     = FCrystalId(Source.FusionHalfBType, Source.FusionHalfBTier);
        Result.Fusion.Id.BonusStat = Source.FusionBonusStat;
        // Seed whole-fusion durability: an elemental fusion (gem half present)
        // starts at its gem-seeded max (incl. the two-tier bonus); an augmented
        // fusion seeds 0 and never wears. GetMaxDurability() returns 0 when there
        // is no gem half, so this single call covers both cases.
        Result.Fusion.CurrentDurability = Result.Fusion.GetMaxDurability();
        break;

    case EAttachedItemKind::None:
    default:
        // Leave Result default-constructed (Empty).
        break;
    }

    return Result;
}
