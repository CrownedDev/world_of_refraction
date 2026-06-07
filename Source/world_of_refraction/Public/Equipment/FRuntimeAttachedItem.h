// FRuntimeAttachedItem.h
// Runtime attachment slot for FWeaponInventoryEntry / FRingInventoryEntry.
// Holds per-instance crystal state (identity + durability) for an equipped
// weapon or ring.
//
// Discriminated by Kind: when Refined, the Refined branch holds the live
// state (FCrystalId + CurrentDurability); when Evolution, the Evolution
// branch holds the live state (UEvolutionItemData* + CurrentDurability). Branches
// other than Kind are hidden in the editor via EditConditionHides and not
// expected to carry meaningful data.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/EAttachedItemKind.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Equipment/FCrystalAttachment.h"
#include "Equipment/Crystals/FEvolutionAttachment.h"
#include "FRuntimeAttachedItem.generated.h"

class UEvolutionItemData;
struct FAttachedItem;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FRuntimeAttachedItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Attached Item")
    EAttachedItemKind Kind = EAttachedItemKind::None;

    // Identity carrier for Refined AND Whetstone — a whetstone stores its
    // FCrystalId{Whetstone, Tier} here so existing consumers keep reading
    // Refined.Id unchanged. (Factory population lands in Cluster B.)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Crystal || Kind == EAttachedItemKind::WeaponStone", EditConditionHides))
    FCrystalAttachment Crystal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Evolution", EditConditionHides))
    FEvolutionAttachment Evolution;

    bool IsEmpty() const { return Kind == EAttachedItemKind::None; }
    bool IsCrystal() const { return Kind == EAttachedItemKind::Crystal; }
    bool IsEvolution() const { return Kind == EAttachedItemKind::Evolution; }
    bool IsWeaponStone() const { return Kind == EAttachedItemKind::WeaponStone; }

    // ==================== STATE QUERIES ====================

    /** True only when an attachment exists and its active branch reports
     *  broken. Default-constructed (Empty) reports false — "broken" only
     *  makes sense when something exists. */
    bool IsBroken() const;

    /** Active branch exists and is not broken — i.e. spells routed through
     *  this attachment can fire. */
    bool CanProvideSpells() const;

    /** Element of the active branch. Refined resolves via CrystalIdentity;
     *  evolution resolves via the item's GetAssociatedElement. Empty returns
     *  Generic. */
    ESpellElement GetElement() const;

    /** Current durability of the active branch. 0 when empty. */
    int32 GetCurrentDurability() const;

    /** Max durability of the active branch. Refined resolves via
     *  CrystalIdentity::GetMaxDurability; evolution reads Item->MaxDurability.
     *  0 when empty or item is null. */
    int32 GetMaxDurability() const;

    /** Stat-modifier query — evolution-only. Refined attachments and empty
     *  return false. Evolution branch delegates to the item asset. */
    bool HasStatModifiers() const;

    /** Stat-modifier summary string — evolution-only. Refined attachments and
     *  empty return an empty string. */
    FString GetStatModifierSummary() const;

    // ==================== WEAR / REPAIR ====================

    /** Dispatch wear to the active branch. Returns true iff this wear broke
     *  the attachment. No-op when empty. */
    bool ApplyWear(int32 Amount);

    /** Dispatch repair to the active branch. Returns actual amount repaired.
     *  0 when empty. */
    int32 RepairBetweenCombats(int32 Amount);

    // ==================== COMPARISON ====================

    /** Same Kind, and the active branch matches. Refined compares Id only
     *  (durability is per-instance state, not identity). Evolution compares
     *  Item pointer only. */
    bool operator==(const FRuntimeAttachedItem &Other) const;
    bool operator!=(const FRuntimeAttachedItem &Other) const { return !(*this == Other); }

    // ==================== FACTORY ====================

    /** Bridge factory — given the design-time FAttachedItem struct carried on
     *  UEquipmentDataBase, produce the discriminated runtime attachment.
     *  Source.Kind directly carries the refined-vs-evolution decision; this
     *  just copies identity and seeds initial durability. Returns a default
     *  (Empty) attachment when Source.Kind is None.
     *
     *  Refined branch: builds FCrystalId from Source.RefinedType +
     *  Source.RefinedTier, seeds CurrentDurability from
     *  CrystalIdentity::GetMaxDurability — the tier-based max the runtime
     *  already treats as refined's source of truth (refined crystals carry no
     *  asset, so there is no per-asset MaxDurability to read as FromAsset does).
     *
     *  Evolution branch: stores Source.Evolution, seeds CurrentDurability from
     *  Evolution->MaxDurability — byte-for-byte parity with FromAsset, since
     *  Source.Evolution is that same asset pointer. */
    static FRuntimeAttachedItem FromAttachedItem(const FAttachedItem &Source);
};
