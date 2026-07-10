// ShopRowWidget.h
// C++ base for the shop ListView rows (WBP_StockRow / WBP_CartRow). Exists because
// UListView refuses to compile without an EntryWidgetClass implementing
// IUserObjectListEntry — and that interface can't be added to a WBP via MCP.
// Minimal by design (Cluster 3b): caches the UShopEntryObject and forwards it to BP
// for visuals. Row BEHAVIOUR (AddToCart / RemoveFromCart clicks, name/price display)
// lands here in Cluster 3c.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "ShopRowWidget.generated.h"

class UShopEntryObject;

UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UShopRowWidget : public UUserWidget, public IUserObjectListEntry
{
    GENERATED_BODY()

public:
    /** The stock/cart line this row displays, or null before the list assigns one. */
    UFUNCTION(BlueprintPure, Category = "Shop")
    UShopEntryObject *GetEntryObject() const { return EntryObject.Get(); }

protected:
    /** IUserObjectListEntry: the ListView hands us our item. Cache + notify BP. */
    virtual void NativeOnListItemObjectSet(UObject *ListItemObject) override;

    /** BP hook for row visuals — fires every time the (pooled) row is re-bound. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Shop")
    void OnEntrySet(UShopEntryObject *Entry);

private:
    TWeakObjectPtr<UShopEntryObject> EntryObject;
};
