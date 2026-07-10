// ShopWindowWidget.h
// C++ base for WBP_ShopWindow (Cluster 3b): two-column shop-with-cart. ALL logic lives
// here; the WBP supplies layout only, wiring by BindWidgetOptional name. Reads its data
// source (merchant + purchaser) from UMerchantShopSubsystem in NativeConstruct — the
// subsystem stamps ActiveMerchant/ActivePawn BEFORE AddToViewport and never needs to
// know this concrete type. Prices the cart via UEconomyService::PreviewCartCost (the
// same builder Purchase charges with) and buys via the atomic Purchase; Close routes
// through the subsystem so the modal bracket (input/cursor/pause) always unwinds.
//
// Buttons are bound in C++ (AddUniqueDynamic in NativeConstruct) — the WBP needs NO
// OnClicked graph wiring; Handle* stay BlueprintCallable for optional BP-side triggers.
// Rows (Cluster 3c): StockList/CartList carry UShopEntryObject items; leave
// EntryWidgetClass unset until WBP_StockRow / WBP_CartRow land — lists render blank.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"      // FTSTicker toast timer — world timers are paused under the shop
#include "Currency/CurrencyTypes.h" // ECurrencyType for the OnCurrencyChanged handler
#include "Merchant/MerchantData.h"  // FMerchantStockEntry — stock + cart element
#include "ShopWindowWidget.generated.h"

class APawn;
class UButton;
class UCurrencyComponent;
class UEconomyService;
class UListView;
class UMerchantShopSubsystem;
class UTextBlock;

/** UObject carrier for one shop line — UListView items must be UObjects and
 *  FMerchantStockEntry is a struct. Stock and cart lists both hold these; the
 *  3c row widgets read Entry off their list item. */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UShopEntryObject : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    FMerchantStockEntry Entry;

    /** The window whose list this item sits in — the row's route to AddToCart /
     *  RemoveFromCart without coupling row → window structurally. Stamped by the
     *  window before SetListItems. Weak: an item never keeps the window alive. */
    TWeakObjectPtr<class UShopWindowWidget> ParentWindow;
};

UCLASS()
class WORLD_OF_REFRACTION_API UShopWindowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ==================== DATA SOURCE ====================

    /** Point the window at a merchant + purchaser and rebuild every pane. NativeConstruct
     *  self-resolves both from the subsystem; call explicitly only to re-target. */
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void SetMerchant(UMerchantData *Merchant, APawn *PurchaserPawn);

    // ==================== CART ====================

    /** Append Entry to the cart and refresh cart list / totals / Confirm state.
     *  The 3c stock rows call this. */
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void AddToCart(const FMerchantStockEntry &Entry);

    /** Remove the FIRST cart line matching Entry (same Asset, or same CrystalId).
     *  The 3c cart rows call this. */
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void RemoveFromCart(const FMerchantStockEntry &Entry);

    // ==================== DETAIL PANEL (3e) ====================

    /** Point the middle detail panel at Entry (rows call this on hover; null clears).
     *  Stock population defaults it to the first stock entry. */
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void SetHoveredEntry(UShopEntryObject *Entry);

    // ==================== BUTTON HANDLERS (C++-bound; callable from BP too) ============

    /** Confirm: UEconomyService::Purchase(Purchaser, Cart). Success → clear cart + toast;
     *  failure → toast (should not fire — Confirm disables while unaffordable). */
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void HandleConfirmClicked();

    /** Close: UMerchantShopSubsystem::Close() — restores input / cursor / unpause. */
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void HandleCloseClicked();

    /** Detail panel [+ Add to Cart]: AddToCart(hovered entry). Disabled while nothing
     *  is hovered. Replaces the per-stock-row Add button (3e). */
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void HandleDetailAddClicked();

    // ==================== DEBUG ====================

    /** Formatted snapshot: merchant, purchaser, cart lines, per-currency totals,
     *  affordability — inspectable without clicking through the UI. */
    UFUNCTION(BlueprintPure, Category = "Debug")
    FString GetShopString() const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ==================== BINDWIDGET (names must match the WBP tree) ====================

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UTextBlock> MerchantName;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UTextBlock> MerchantType;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UListView> StockList;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UListView> CartList;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UTextBlock> CartTotalPrisms;

    /** Compact multi-currency line, e.g. "Skill 30 | Fire 25 | Reality 12" — or "—". */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UTextBlock> CartTotalEssence;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UTextBlock> WalletPrisms;

    /** Balances mirroring the currencies the CART needs (same order as CartTotalEssence). */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UTextBlock> WalletEssence;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UButton> ConfirmButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UButton> CloseButton;

    /** Hidden by default; shown for TOAST_DURATION_SECONDS after Confirm. */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UTextBlock> ToastText;

    // ---- Detail panel (middle column, 3e) ----

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UTextBlock> DetailName;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UTextBlock> DetailTier;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UTextBlock> DetailDescription;

    /** Per-type stat lines (attack/spell lists, damage numbers, scaling grades,
     *  effect NAMES — never effect numbers/durations). */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UTextBlock> DetailStats;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UButton> DetailAddButton;

    /** Cart lines — the exact array handed to Purchase. */
    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    TArray<FMerchantStockEntry> Cart;

private:
    // ==================== STATE ====================

    UPROPERTY()
    TObjectPtr<UMerchantData> ActiveMerchant;

    TWeakObjectPtr<APawn> Purchaser;
    TWeakObjectPtr<UCurrencyComponent> PurchaserCurrency;

    /** ListView item objects — owned here so GC keeps them alive for the lists. */
    UPROPERTY()
    TArray<TObjectPtr<UShopEntryObject>> StockObjects;

    UPROPERTY()
    TArray<TObjectPtr<UShopEntryObject>> CartObjects;

    /** Core-ticker handle for the toast auto-hide. NOT a world timer: the hub is PAUSED
     *  while the shop is open, so FTimerManager never fires under it. */
    FTSTicker::FDelegateHandle ToastTickerHandle;

    /** The entry the detail panel is showing (last hovered row; defaults to the
     *  first stock entry on population). */
    TWeakObjectPtr<UShopEntryObject> HoveredEntry;

    // ==================== REFRESH ====================

    void RefreshHeader();
    void RefreshStockList();
    void RefreshCartList();
    /** Cart totals + wallet mirror + ConfirmButton enable, all from one PreviewCartCost. */
    void RefreshCostAndWallet();
    /** Middle-column detail panel from HoveredEntry (clears when null). */
    void RefreshDetail();

    void ShowToast(const FText &Message);
    void HideToast();

    UEconomyService *GetEconomyService() const;
    UMerchantShopSubsystem *GetShopSubsystem() const;

    /** CurrencyComponent::OnCurrencyChanged — any wallet movement refreshes totals/enable. */
    UFUNCTION()
    void HandleCurrencyChanged(ECurrencyType Currency, uint8 SubKey, int32 NewBalance);
};
