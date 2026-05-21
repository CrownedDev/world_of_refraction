// FRuntimeAttachedItem.h
// Runtime attachment slot for FWeaponInventoryEntry / FRingInventoryEntry.
// Replaces AttachedCrystal (FCrystalInventoryEntry) after the storage-split
// commits land.
//
// Discriminated by Kind: when Refined, the Refined branch holds the live
// state (FCrystalId + CurrentDurability); when Evolution, the Evolution
// branch holds the live state (UItemData* + CurrentDurability). Branches
// other than Kind are hidden in the editor via EditConditionHides and not
// expected to carry meaningful data.

#pragma once

#include "CoreMinimal.h"
#include "EAttachedItemKind.h"
#include "FRefinedAttachment.h"
#include "FEvolutionAttachment.h"
#include "FRuntimeAttachedItem.generated.h"

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
};
