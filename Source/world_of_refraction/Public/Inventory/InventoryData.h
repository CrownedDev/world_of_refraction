// InventoryData.h
// Per-character inventory ownership asset, plus the saved loadouts that
// reference items from those ownership lists.
//
// REPLACES ULoadoutData over the 6-commit migration:
// - Commit 1 (here): the asset type is added, no callers wire it up.
// - Commit 2: UCharacterData gains a sibling Inventory field.
// - Commit 3: UInventoryComponent owns SavedLoadouts at runtime.
// - Commit 4: init paths read this asset; designer migrates per-character.
// - Commit 5: UCharacterData::DefaultLoadout removed.
// - Commit 6: ULoadoutData and ULoadoutData-shaped factories deleted.
//
// The ownership lists (Weapons / Rings / CrystalStock / EvolutionEquipment /
// Spells / Abilities) are the source of truth for what
// a character can equip. Each FSavedLoadout in SavedLoadouts[] references
// assets that MUST also appear in the matching ownership list —
// GetValidationErrors() flags any mismatch.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Loadout/FSavedLoadout.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "InventoryData.generated.h"

class UWeaponData;
class URingData;
class UEvolutionItemData;
class USpellData;
class UAbilityData;

/**
 * UInventoryData
 *
 * Per-character inventory + loadouts asset. Designer authors what a
 * character owns and the saved loadout configurations they can switch
 * between in combat.
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UInventoryData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== OWNED EQUIPMENT ====================

    /** Weapons the character owns. Every loadout's PrimaryWeapon /
     *  SecondaryWeapon must come from this list. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Inventory|Equipment")
    TArray<UWeaponData *> Weapons;

    /** Rings the character owns. Every loadout's PrimaryRing /
     *  Resonator EquippedRings entry must come from this list. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Inventory|Equipment")
    TArray<URingData *> Rings;

    // ==================== OWNED CRYSTALS ====================
    // Per-tier cap CRYSTAL_PER_TIER_CAP enforced via IsDataValid;
    // EvolutionEquipment hard cap MAX_EVOLUTION_ITEMS.

    /** Authored crystal stock — the character's starting crystals (gems AND stones, MIXED; the
     *  bulk-load splits them to the runtime Gems / Stones pools by IsGemType). One map since the
     *  gem-merge (§13.5) collapsed the item/refined split. Per-tier cap of 20 PER runtime pool —
     *  gem entries and stone entries are capped separately per tier (see IsDataValid).
     *  ⚠️ RENAME-PASS NOTE: named CrystalStock (NOT Gems) because it holds gems + stones mixed —
     *  the runtime gem pool is still called Gems; this authored map is the mixed input. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1.5. Inventory|Crystals (New)",
              meta = (DisplayName = "Crystal Stock (counts)"))
    TMap<FCrystalId, int32> CrystalStock;

    // TODO: remove the two deprecated crystal maps below after one save cycle. The gem-merge
    // PostLoad folds them into CrystalStock; they are kept (still serialized) ONLY so legacy
    // .uasset files migrate losslessly. Do not author into them.
    UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Folded into CrystalStock by the gem-merge — do not author."))
    TMap<FCrystalId, int32> ItemCrystals;

    UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Folded into CrystalStock by the gem-merge — do not author."))
    TMap<FCrystalId, int32> RefinedCrystals;

    /** Evolution items the character owns (unslotted). Each entry becomes a
     *  runtime FEvolutionInventoryEntry with its own FGuid in
     *  UEvolutionInventoryComponent. Hard cap MAX_EVOLUTION_ITEMS (5). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1.5. Inventory|Crystals (New)",
              meta = (DisplayName = "Evolution Equipment"))
    TArray<UEvolutionItemData *> EvolutionEquipment;

    // ==================== OWNED SPELLS / ABILITIES ====================

    /** Spells the character knows. Every loadout's InnateSpells /
     *  EvolutionSpells / per-ring assigned spells must come from this list. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2. Inventory|Knowledge")
    TArray<USpellData *> Spells;

    /** Abilities the character knows. Every loadout's
     *  PrimaryWeaponAbilities / SecondaryWeaponAbilities must come from
     *  this list. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2. Inventory|Knowledge")
    TArray<UAbilityData *> Abilities;

    // ==================== SAVED LOADOUTS ====================

    /** Saved loadout configurations the character can switch between at
     *  combat start. Index 0 is the default active loadout (preset by
     *  convention — no bIsPreset flag). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Loadouts",
              meta = (TitleProperty = "LoadoutName"))
    TArray<FSavedLoadout> SavedLoadouts;

    /** Which entry in SavedLoadouts[] is active at spawn. Out-of-range
     *  values are clamped at runtime to 0. Editor renders as a dropdown of
     *  loadout names; storage stays as int32 so renames don't break refs. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Loadouts",
              meta = (GetOptions = "GetActiveLoadoutOptions"))
    int32 DefaultActiveLoadoutIndex = 0;

    /** Supplies dropdown labels for DefaultActiveLoadoutIndex. */
    UFUNCTION()
    TArray<FString> GetActiveLoadoutOptions() const;

    // ==================== VALIDATION ====================

    /** Per-loadout struct validation plus cross-checks against the
     *  ownership lists above. Returns one error string per violation. */
    TArray<FString> GetValidationErrors() const;

    /** Quick check for any validation errors */
    bool HasValidationErrors() const;

    // ==================== EDITOR ====================

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif

    // ==================== DATA ASSET ====================

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    /** Gem-merge migration (§13.5): folds the deprecated ItemCrystals + RefinedCrystals into
     *  CrystalStock so legacy assets load without re-authoring. Idempotent — guards on CrystalStock
     *  being empty, so a migrated / freshly-authored asset no-ops. */
    virtual void PostLoad() override;
};
