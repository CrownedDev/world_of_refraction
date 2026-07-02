// MerchantData.h
// Authored merchant definition for the hub/trial merchant system
// (Resources_Design.md "Hub/Trial Merchants — LOCKED DESIGN", 2026-06-26).
//
// v1 = FIXED authored stock lists. The §12 rolled/reroll stock and the C0
// registry tag-QUERY are v2 layers — tags are authored on items from day one
// so the v2 migration is "swap list for query".
//
// The SAME asset serves both contexts: currency is CONTEXT-driven at purchase
// time (hub = Prisms, trial = Gold), so currency is deliberately NOT a field
// here — otherwise every merchant would need two assets.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "GameplayTagContainer.h"
#include "MerchantData.generated.h"

/** The five merchant archetypes. Item-type -> merchant is AUTOMATIC from the
 *  item's class (merchant map, Resources_Design.md); vendor tags only override. */
UENUM(BlueprintType)
enum class EMerchantType : uint8
{
    Blacksmith UMETA(DisplayName = "Blacksmith (weapons, augment stones)"),
    Jeweler UMETA(DisplayName = "Jeweler (rings)"),
    CombatMaster UMETA(DisplayName = "Combat Master (abilities)"),
    SpellShop UMETA(DisplayName = "Spell Shop (spells)"),
    Spiritualist UMETA(DisplayName = "Spiritualist (evolutions, crystals, repairs)")
};

/**
 * One shop-shelf line. EXACTLY ONE of Asset / Crystal identifies the goods:
 *  - Asset: asset-backed items (UWeaponData, URingData, USpellData,
 *    UAbilityData, UEvolutionItemData).
 *  - Crystal: count-based items with no data asset — gem crystals and augment
 *    stones, identified by FCrystalId (Type + Tier).
 * Prices are NOT authored — pricing is tier-keyed via EconomyYield at purchase.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FMerchantStockEntry
{
    GENERATED_BODY()

    /** Asset-backed goods. Leave null for crystal/stone stock. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stock")
    TObjectPtr<UPrimaryDataAsset> Asset;

    /** Count-based goods (crystals / augment stones). Ignored when Asset is
     *  set; Type == None means unused. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stock")
    FCrystalId Crystal;

    /** Units on the shelf. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stock", meta = (ClampMin = "1"))
    int32 Count = 1;

    bool IsAssetEntry() const { return Asset != nullptr; }
    bool IsCrystalEntry() const { return Asset == nullptr && Crystal.Type != ECrystalType::None; }
};

/**
 * Primary data asset for one merchant: type, fixed authored stock, and
 * availability tags. Currency comes from the purchase CONTEXT (hub vs trial),
 * never from this asset.
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UMerchantData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EMerchantType MerchantType = EMerchantType::Blacksmith;

    // ==================== AVAILABILITY ====================

    /** Where this merchant appears (locked availability rules, option a):
     *  - empty          -> HUB only (the default)
     *  - Trial.X        -> trial only. Hierarchical: Trial.Garnet = any
     *                      Garnet-trial floor; Trial.Garnet.Floor1 = that floor.
     *  - Hub + Trial.X  -> both contexts.
     *  Trials are element/class-based, so trial tags map onto the existing
     *  crystal/element taxonomy. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Availability")
    FGameplayTagContainer AvailabilityTags;

    // ==================== STOCK ====================

    /** v1 fixed authored stock. The §12 rolled-stock generator layers on later
     *  and will replace/augment this list, not the entry shape. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stock", meta = (TitleProperty = "Asset"))
    TArray<FMerchantStockEntry> Stock;

    // ==================== AVAILABILITY QUERIES ====================

    /** True when this merchant appears at the hub: no tags authored (the
     *  default) or an explicit Hub tag alongside trial tags. */
    UFUNCTION(BlueprintPure, Category = "Merchant")
    bool AppearsInHub() const;

    /** True when this merchant appears in the trial context identified by
     *  ContextTag (e.g. Trial.Garnet.Floor1). Hierarchical: an authored
     *  Trial.Garnet matches any Garnet floor tag. */
    UFUNCTION(BlueprintPure, Category = "Merchant")
    bool AppearsInTrial(FGameplayTag ContextTag) const;

    // ==================== DEBUG ====================

    /** Formatted snapshot — type, availability, every stock line. */
    UFUNCTION(BlueprintPure, Category = "Debug")
    FString GetMerchantString() const;

    UFUNCTION(CallInEditor, Category = "Debug")
    void PrintMerchant();

    /** Logs every stock issue (structural errors + type/merchant mismatches).
     *  Same core as IsDataValid, on a Details-panel button. */
    UFUNCTION(CallInEditor, Category = "Debug")
    void ValidateStock();

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif

private:
    /** Shared validation core. Structural problems (empty entry, both Asset
     *  and Crystal set, bad count) go to OutErrors; sells-the-wrong-type goes
     *  to OutWarnings (vendor tags may deliberately override the merchant map). */
    void CollectStockIssues(TArray<FString> &OutErrors, TArray<FString> &OutWarnings) const;
};
