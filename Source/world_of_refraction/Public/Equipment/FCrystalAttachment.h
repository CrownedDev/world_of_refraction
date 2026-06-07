// FCrystalAttachment.h
// Runtime refined-crystal attachment. Identified by FCrystalId (Type + Tier)
// with per-instance CurrentDurability. No asset pointer — refined crystals
// post-refactor are enum pairs, not asset references. No GUID — refined
// crystals are destroyed on removal from their holder, so no instance
// retrieval pathway exists.
//
// Used as the Refined branch of FRuntimeAttachedItem.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Crystals/CrystalIdentity.h"
#include "FCrystalAttachment.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FCrystalAttachment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Crystal")
    FCrystalId Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Crystal")
    int32 CurrentDurability = 0;

    FCrystalAttachment() = default;

    FCrystalAttachment(const FCrystalId &InId, int32 InDurability)
        : Id(InId), CurrentDurability(InDurability) {}

    /** Refined crystals are never immune to breaking. Caller must gate this
     *  with !IsEmpty()-equivalent — a default-constructed FRefinedAttachment
     *  reports IsBroken()==true since CurrentDurability defaults to 0.
     *  FRuntimeAttachedItem::IsBroken() applies that gate. */
    bool IsBroken() const
    {
        return CurrentDurability <= 0;
    }

    /** Apply wear. Returns true iff this wear broke the crystal
     *  (durability transitioned from >0 to 0). No-op when Amount <= 0 or
     *  CurrentDurability <= 0. */
    bool ApplyWear(int32 Amount)
    {
        if (Amount <= 0 || CurrentDurability <= 0)
        {
            return false;
        }

        const int32 Before = CurrentDurability;
        CurrentDurability = FMath::Max(0, CurrentDurability - Amount);

        return Before > 0 && CurrentDurability == 0;
    }

    /** Repair between combats. Clamp to ItemIdentity::GetMaxDurability(Id).
     *  Returns actual amount repaired. No-op when Amount <= 0. */
    int32 RepairBetweenCombats(int32 Amount)
    {
        if (Amount <= 0)
        {
            return 0;
        }

        const int32 MaxDur = ItemIdentity::GetMaxDurability(Id);
        const int32 Before = CurrentDurability;
        CurrentDurability = FMath::Min(MaxDur, CurrentDurability + Amount);

        return CurrentDurability - Before;
    }
};
