// ShopRowWidget.h
// C++ base for the shop ListView rows (WBP_StockRow / WBP_CartRow). Cluster 3c: rows
// display name / tier / count / cost and drive the cart — AddButton (stock rows) →
// ParentWindow->AddToCart, RemoveButton (cart rows) → RemoveFromCart, both reached
// through the UShopEntryObject::ParentWindow back-ref stamped at list population.
//
// Buttons bind in C++ (AddUniqueDynamic in NativeConstruct — fires once per pooled
// row); texts refresh in NativeOnListItemObjectSet (fires per re-bind). The WBPs
// supply layout only, wiring by BindWidgetOptional name: stock rows place
// RowName / RowTier / RowCost (3e: Add moved to the window's detail panel; hovering a
// row populates that panel via NativeOnMouseEnter → ParentWindow->SetHoveredEntry);
// cart rows RowName / RowCount / RowCost / RemoveButton. Line cost comes from
// UEconomyService::PreviewCartCost on a one-entry cart — the same builder Purchase
// charges with. The static *For builders are the one display ladder shared with the
// window's detail panel.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "ShopRowWidget.generated.h"

class UButton;
class UEconomyService;
class UShopEntryObject;
class UTextBlock;
struct FMerchantStockEntry;

UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UShopRowWidget : public UUserWidget, public IUserObjectListEntry
{
    GENERATED_BODY()

public:
    /** The stock/cart line this row displays, or null before the list assigns one. */
    UFUNCTION(BlueprintPure, Category = "Shop")
    UShopEntryObject *GetEntryObject() const { return EntryObject.Get(); }

    // ==================== DISPLAY HELPERS ====================
    // The standard-named TextBlocks below are filled automatically on every list
    // bind; these stay BlueprintPure for custom WBP layouts.

    /** Item display name per type: equipment / skill / evolution / crystal. */
    UFUNCTION(BlueprintPure, Category = "Shop")
    FText GetEntryName() const;

    /** Short tier letter ("F".."S"). */
    UFUNCTION(BlueprintPure, Category = "Shop")
    FText GetEntryTier() const;

    /** Units in this line ("×2" style raw number; stock shelves are 1). */
    UFUNCTION(BlueprintPure, Category = "Shop")
    FText GetEntryCount() const;

    /** Line cost (Count included) via PreviewCartCost — "120 P | Fire 25". */
    UFUNCTION(BlueprintPure, Category = "Shop")
    FText GetEntryCost() const;

    /** The item's long Description (crystals: tier-flavor sentence, no numbers).
     *  3e: consumed by the window's detail panel — rows no longer set tooltips. */
    UFUNCTION(BlueprintPure, Category = "Shop")
    FText GetEntryTooltip() const;

    // Static per-entry display builders — ONE ladder shared by rows and the window's
    // detail panel (the instance getters above delegate here).
    static FText NameFor(const FMerchantStockEntry &Entry);
    static FText TierFor(const FMerchantStockEntry &Entry);
    static FText DescriptionFor(const FMerchantStockEntry &Entry);

    // ==================== BUTTON HANDLERS (C++-bound; callable from BP too) ============

    /** Stock row [+ Add]: ParentWindow->AddToCart(Entry) — window coalesces lines. */
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void HandleAddClicked();

    /** Cart row [− Remove]: ParentWindow->RemoveFromCart(Entry) — decrements, 0 removes. */
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void HandleRemoveClicked();

protected:
    virtual void NativeConstruct() override;

    /** IUserObjectListEntry: the ListView hands us our item. Cache + refresh + notify BP. */
    virtual void NativeOnListItemObjectSet(UObject *ListItemObject) override;

    /** 3e hover-drives-detail: notify the window (via ParentWindow) that this row's
     *  entry should populate the middle detail panel. */
    virtual void NativeOnMouseEnter(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

    /** BP hook for extra row visuals — fires every time the (pooled) row is re-bound. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Shop")
    void OnEntrySet(UShopEntryObject *Entry);

    // ==================== BINDWIDGET (names must match the WBP row trees) ================

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UTextBlock> RowName;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UTextBlock> RowTier;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UTextBlock> RowCount;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UTextBlock> RowCost;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UButton> AddButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Shop|Widgets")
    TObjectPtr<UButton> RemoveButton;

private:
    TWeakObjectPtr<UShopEntryObject> EntryObject;

    /** Fill RowName/RowTier/RowCount/RowCost from the current entry (null-safe). */
    void RefreshRowTexts();

    UEconomyService *GetEconomyService() const;
};
