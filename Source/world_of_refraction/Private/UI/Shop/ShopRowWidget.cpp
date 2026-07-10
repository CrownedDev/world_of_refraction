// ShopRowWidget.cpp

#include "UI/Shop/ShopRowWidget.h"

#include "UI/Shop/ShopWindowWidget.h" // UShopEntryObject

void UShopRowWidget::NativeOnListItemObjectSet(UObject *ListItemObject)
{
    IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

    UShopEntryObject *Entry = Cast<UShopEntryObject>(ListItemObject);
    EntryObject = Entry;
    if (!Entry)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ShopRow] %s bound to a non-UShopEntryObject item — row blank."), *GetName());
    }
    OnEntrySet(Entry);
}
