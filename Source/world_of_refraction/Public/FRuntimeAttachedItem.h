// FRuntimeAttachedItem.h
// Runtime attachment slot for FWeaponInventoryEntry / FRingInventoryEntry.
// Replaces AttachedCrystal (FCrystalInventoryEntry) after the storage-split
// commits land.
//
// Discriminated by Kind: when Refined, the Refined branch holds the live
// state (FCrystalId + CurrentDurability); when Evolution, the Evolution
// branch holds the live state (UEvolutionItemData* + CurrentDurability). Branches
// other than Kind are hidden in the editor via EditConditionHides and not
// expected to carry meaningful data.

#pragma once

#include "CoreMinimal.h"
#include "EAttachedItemKind.h"
#include "ESpellElement.h"
#include "FRefinedAttachment.h"
#include "FEvolutionAttachment.h"
#include "FRuntimeAttachedItem.generated.h"

class UEvolutionItemData;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FRuntimeAttachedItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Attached Item")
    EAttachedItemKind Kind = EAttachedItemKind::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Refined", EditConditionHides))
    FRefinedAttachment Refined;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Attached Item",
              meta = (EditCondition = "Kind == EAttachedItemKind::Evolution", EditConditionHides))
    FEvolutionAttachment Evolution;

    bool IsEmpty() const { return Kind == EAttachedItemKind::None; }
    bool IsRefined() const { return Kind == EAttachedItemKind::Refined; }
    bool IsEvolution() const { return Kind == EAttachedItemKind::Evolution; }

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

    /** Transitional factory — given a UEvolutionItemData* (the asset-side
     *  crystal handle still used by FCrystalInventoryEntry-era code),
     *  produce a discriminated runtime attachment. Branches on
     *  Crystal->bIsEvolutionCrystal. Returns a default (Empty) attachment
     *  when Crystal is null.
     *
     *  Refined branch: builds FCrystalId from Crystal->CrystalType +
     *  Crystal->Tier, seeds CurrentDurability from Crystal->MaxDurability
     *  for byte-for-byte parity with FCrystalInventoryEntry::CreateFromCrystal.
     *
     *  Evolution branch: stores Item, seeds CurrentDurability from
     *  Crystal->MaxDurability. */
    static FRuntimeAttachedItem FromAsset(UEvolutionItemData *Crystal);
};
