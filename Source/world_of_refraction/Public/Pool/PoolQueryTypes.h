// PoolQueryTypes.h
// Query shapes for the structured pool: the fixed-axis FILTER + the per-category RETURN
// views. Separated from the subsystem so both the pool AND (later) the run-inventory
// facade can share one filter/result vocabulary. Pure data — no logic (the translation
// lives in PoolAccessors).

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Definitions/SpellSchool.h"
#include "Equipment/Weapons/EWeaponType.h"
#include "Inventory/ItemTier.h"
#include "Inventory/ItemQuality.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Loadout/Entries/FSpellCollection.h"   // FSpellInstance
#include "Loadout/Entries/FAbilityCollection.h" // FAbilityInstance
#include "Equipment/Crystals/FEvolutionInventoryEntry.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "PoolQueryTypes.generated.h"

/**
 * EItemBucket — the locked 2-bucket BROWSE classification for the Items category. A
 * presentation grouping over the one crystal pile (storage stays the two TMaps — this is
 * NOT a storage split). Classified from ECrystalType via CrystalTypeHelpers: gems → Crystal;
 * everything non-gem (stat stones + AbilityStone) → AugmentStone (Ability folds in here).
 */
UENUM(BlueprintType)
enum class EItemBucket : uint8
{
    Crystal      UMETA(DisplayName = "Crystal"),
    AugmentStone UMETA(DisplayName = "Augment Stone")
};

/**
 * FPoolFilter — the fixed designed filter axes. Each axis is gated by a bUseX flag:
 * bUseX = false → that axis is a WILDCARD (not applied). This is the BlueprintType-friendly
 * stand-in for TOptional (which isn't a BP type). An axis a given item TYPE doesn't have
 * (e.g. school on a weapon) excludes that item when the axis is active — see PoolAccessors.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FPoolFilter
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
    bool bUseElement = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter", meta = (EditCondition = "bUseElement"))
    ESpellElement Element = ESpellElement::Fire;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
    bool bUseWeaponType = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter", meta = (EditCondition = "bUseWeaponType"))
    EWeaponType WeaponType = EWeaponType::Sword;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
    bool bUseSchool = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter", meta = (EditCondition = "bUseSchool"))
    ESpellSchool School = ESpellSchool::Destruction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
    bool bUseTier = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter", meta = (EditCondition = "bUseTier"))
    EItemTier Tier = EItemTier::F_Tier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
    bool bUseQuality = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter", meta = (EditCondition = "bUseQuality"))
    EItemQuality Quality = EItemQuality::C_Quality;

    /** ITEMS-only storage-pool dimension: when set, keep only refined (true) or only consumable
     *  (false) crystals. The gem-vs-augment-stone BROWSE bucket is a separate 1c-iii concern. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter|Items")
    bool bUseRefined = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter|Items", meta = (EditCondition = "bUseRefined"))
    bool bRefined = false;

    /** ITEMS-only browse bucket (Crystal vs Augment Stone). Unset = both buckets. A
     *  classification over the crystal pile (PoolAccessors::GetBucket), not a storage axis. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter|Items")
    bool bUseBucket = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter|Items", meta = (EditCondition = "bUseBucket"))
    EItemBucket Bucket = EItemBucket::Crystal;
};

/** One crystal stack in a query result: the (Type,Tier) key, its count, and which pool it
 *  came from (refined vs consumable). Crystals are fungible — no per-instance identity. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FPoolItemStack
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Pool")
    FCrystalId Id;

    UPROPERTY(BlueprintReadOnly, Category = "Pool")
    int32 Count = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Pool")
    bool bRefined = false;
};

/** KNOWLEDGE category result — spells + abilities (the two skill stores). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FPoolKnowledgeView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Pool")
    TArray<FSpellInstance> Spells;

    UPROPERTY(BlueprintReadOnly, Category = "Pool")
    TArray<FAbilityInstance> Abilities;
};

/** ARMOURY category result — weapons + rings + evolutions (the gear stores). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FPoolArmouryView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Pool")
    TArray<FWeaponInventoryEntry> Weapons;

    UPROPERTY(BlueprintReadOnly, Category = "Pool")
    TArray<FRingInventoryEntry> Rings;

    UPROPERTY(BlueprintReadOnly, Category = "Pool")
    TArray<FEvolutionInventoryEntry> Evolutions;
};

/** Cross-category result for QueryAll — all three browse categories, one filter pass. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FPoolQueryResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Pool")
    FPoolKnowledgeView Knowledge;

    UPROPERTY(BlueprintReadOnly, Category = "Pool")
    FPoolArmouryView Armoury;

    UPROPERTY(BlueprintReadOnly, Category = "Pool")
    TArray<FPoolItemStack> Items;
};
