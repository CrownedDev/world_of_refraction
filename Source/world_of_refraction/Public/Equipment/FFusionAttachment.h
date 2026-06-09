// FFusionAttachment.h
// Runtime fusion attachment. Identity is FFusionId (two FCrystalId halves + a
// bonus sub-stat); whole-fusion CurrentDurability tracks wear.
//
// Durability model (locked): WHOLE-FUSION, crystals only. An ELEMENTAL fusion
// (exactly one gem half) wears as a single unit, seeded from the gem half's max
// durability plus a two-tier bonus (GetFusionBonusDurability, stubbed). An
// AUGMENTED fusion (two stones, no gem) does not wear — CurrentDurability stays
// 0 and it never breaks, mirroring the augment-stone no-wear semantics.
//
// Used as the Fusion branch of FRuntimeAttachedItem.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FFusionId.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "FFusionAttachment.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FFusionAttachment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Fusion")
    FFusionId Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Fusion")
    int32 CurrentDurability = 0;

    FFusionAttachment() = default;

    FFusionAttachment(const FFusionId &InId, int32 InDurability)
        : Id(InId), CurrentDurability(InDurability) {}

    /** True iff the fusion carries a gem (crystal) half — only then does the
     *  fusion wear. Authoring forbids two gems, so at most one half is a gem. */
    bool HasGemHalf() const
    {
        return CrystalTypeHelpers::IsGemType(Id.HalfA.Type) ||
               CrystalTypeHelpers::IsGemType(Id.HalfB.Type);
    }

    /** The gem half, when present. HalfA takes priority (HalfB is the authored
     *  gem slot, but check both for robustness). Caller must gate with
     *  HasGemHalf() — returns a default FCrystalId (None) otherwise. */
    FCrystalId GemHalf() const
    {
        if (CrystalTypeHelpers::IsGemType(Id.HalfA.Type))
        {
            return Id.HalfA;
        }
        if (CrystalTypeHelpers::IsGemType(Id.HalfB.Type))
        {
            return Id.HalfB;
        }
        return FCrystalId();
    }

    /** Whole-fusion max durability: the gem half's tier-based max, plus the
     *  two-tier matrix bonus, plus a flat DurabilityStone bonus when a half is a
     *  DurabilityStone. 0 for an augmented fusion (no gem = never wears).
     *
     *  The DurabilityStone term STACKS on the matrix (additive, does not replace
     *  it) and only fires for an actual DurabilityStone half — both halves are
     *  summed because in a legal elemental fusion only one half can be the stone,
     *  so summing is harmless. Reached only past the HasGemHalf gate, so an
     *  augmented (stone+stone) fusion never reaches the term and stays 0/inert. */
    int32 GetMaxDurability() const
    {
        if (!HasGemHalf())
        {
            return 0;
        }
        return ItemIdentity::GetMaxDurability(GemHalf()) +
               ItemIdentity::GetFusionBonusDurability(Id.HalfA.Tier, Id.HalfB.Tier) +
               (Id.HalfA.Type == ECrystalType::DurabilityStone
                    ? ItemIdentity::GetDurabilityStoneBonus(Id.HalfA.Tier)
                    : 0) +
               (Id.HalfB.Type == ECrystalType::DurabilityStone
                    ? ItemIdentity::GetDurabilityStoneBonus(Id.HalfB.Tier)
                    : 0);
    }

    /** A wearing (elemental) fusion is broken at 0 durability. An augmented
     *  fusion (no gem half) never wears, so it is never broken — even though
     *  CurrentDurability defaults to 0. Mirrors FCrystalAttachment::IsBroken's
     *  <=0 test, gated on HasGemHalf so the augmented case is exempt. */
    bool IsBroken() const
    {
        return HasGemHalf() && CurrentDurability <= 0;
    }

    /** Apply wear. Returns true iff this wear broke the fusion (durability
     *  transitioned from >0 to 0). No-op when the fusion is augmented (no gem
     *  half), Amount <= 0, or CurrentDurability <= 0. Mirrors
     *  FCrystalAttachment::ApplyWear with the gem-half gate added. */
    bool ApplyWear(int32 Amount)
    {
        if (!HasGemHalf() || Amount <= 0 || CurrentDurability <= 0)
        {
            return false;
        }

        const int32 Before = CurrentDurability;
        CurrentDurability = FMath::Max(0, CurrentDurability - Amount);

        return Before > 0 && CurrentDurability == 0;
    }

    /** Repair between combats. Clamp to GetMaxDurability(). Returns actual amount
     *  repaired. No-op when the fusion is augmented (no gem half) or Amount <= 0.
     *  Mirrors FCrystalAttachment::RepairBetweenCombats with the gem-half gate. */
    int32 RepairBetweenCombats(int32 Amount)
    {
        if (!HasGemHalf() || Amount <= 0)
        {
            return 0;
        }

        const int32 MaxDur = GetMaxDurability();
        const int32 Before = CurrentDurability;
        CurrentDurability = FMath::Min(MaxDur, CurrentDurability + Amount);

        return CurrentDurability - Before;
    }
};
