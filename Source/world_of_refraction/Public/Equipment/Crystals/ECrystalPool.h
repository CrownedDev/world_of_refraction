// ECrystalPool.h
// Names which crystal pool a batch operation targets — Item (unrefined gems / augment
// stones) or Refined (slottable gems). Replaces the IsGemType pool-GUESS in the crystal
// batch primitives: the caller (which knows the pool — the slotting rule for attach/fusion,
// or merge's bRefined axis) STATES it; the primitive honors the tag. This is the only way
// to express GemItem (unrefined gems), which IsGemType routing could never reach.
//
// Small header so EconomyService can include it without pulling InventoryComponent.h.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "ECrystalPool.generated.h"

UENUM(BlueprintType)
enum class ECrystalPool : uint8
{
    /** Unrefined gems (GemItem) and augment stones (StoneItem) — the consumable / attach-direct form. */
    Item,
    /** Slottable refined gems (GemRefined). Invalid for stones (they have no refined pool). */
    Refined
};

/** One crystal identity + the pool it lives in, for the pool-explicit batch primitives
 *  (RemoveCrystals / AddCrystals). A single batch can span pools (e.g. an elemental fusion:
 *  gem half → Refined, stat-stone half → Item) in ONE atomic op — the reason the batch
 *  carries a per-entry tag rather than a single pool flag. */
USTRUCT(BlueprintType)
struct FCrystalPoolEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal")
    FCrystalId Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal")
    ECrystalPool Pool = ECrystalPool::Item;

    FCrystalPoolEntry() = default;
    FCrystalPoolEntry(const FCrystalId &InId, ECrystalPool InPool) : Id(InId), Pool(InPool) {}

    /** Identity = same crystal AND same pool, so duplicate (Id, Pool) entries tally
     *  (a fusion with HalfA == HalfB needs 2 from the one pool). */
    bool operator==(const FCrystalPoolEntry &Other) const
    {
        return Id == Other.Id && Pool == Other.Pool;
    }
};

/** Hashable so the primitives can tally Required per (Id, Pool) in a TMap. Composes
 *  FCrystalId's hash with the pool byte. */
FORCEINLINE uint32 GetTypeHash(const FCrystalPoolEntry &Entry)
{
    return HashCombine(GetTypeHash(Entry.Id), ::GetTypeHash(static_cast<uint8>(Entry.Pool)));
}
