// ShopRowWidget.cpp

#include "UI/Shop/ShopRowWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Currency/EconomyService.h"
#include "Engine/GameInstance.h"
#include "Equipment/Crystals/CrystalDescription.h" // crystal tooltip: flavor + effect text
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/EquipmentDataBase.h"
#include "Inventory/ItemTier.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "UI/Shop/ShopWindowWidget.h" // UShopEntryObject + UShopWindowWidget

void UShopRowWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Once per pooled row instance; re-binds only refresh texts.
    if (AddButton)
    {
        AddButton->OnClicked.AddUniqueDynamic(this, &UShopRowWidget::HandleAddClicked);
    }
    if (RemoveButton)
    {
        RemoveButton->OnClicked.AddUniqueDynamic(this, &UShopRowWidget::HandleRemoveClicked);
    }
}

void UShopRowWidget::NativeOnListItemObjectSet(UObject *ListItemObject)
{
    IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

    UShopEntryObject *Entry = Cast<UShopEntryObject>(ListItemObject);
    EntryObject = Entry;
    if (!Entry)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ShopRow] %s bound to a non-UShopEntryObject item — row blank."), *GetName());
    }
    RefreshRowTexts();
    OnEntrySet(Entry);
}

FText UShopRowWidget::NameFor(const FMerchantStockEntry &E)
{
    // Names stay tier-free — tier has its own column / detail line.
    if (E.IsCrystalEntry())
    {
        return FText::FromString(ItemIdentity::GetTypeName(E.Crystal.Type));
    }
    if (const UEquipmentDataBase *Equipment = Cast<UEquipmentDataBase>(E.Asset)) // weapon + ring
    {
        return FText::FromString(Equipment->GetDisplayName());
    }
    if (const USkillDataBase *Skill = Cast<USkillDataBase>(E.Asset)) // spell + ability
    {
        return FText::FromString(Skill->Name);
    }
    if (const UEvolutionItemData *Evolution = Cast<UEvolutionItemData>(E.Asset))
    {
        return FText::FromString(Evolution->GetCrystalName()); // variant name, no "(F-Tier)"
    }
    return E.Asset ? FText::FromString(E.Asset->GetName()) : FText::GetEmpty();
}

FText UShopRowWidget::TierFor(const FMerchantStockEntry &E)
{
    EItemTier Tier = EItemTier::F_Tier;
    if (E.IsCrystalEntry())
    {
        Tier = E.Crystal.Tier;
    }
    else if (const UEquipmentDataBase *Equipment = Cast<UEquipmentDataBase>(E.Asset))
    {
        Tier = Equipment->Tier;
    }
    else if (const USkillDataBase *Skill = Cast<USkillDataBase>(E.Asset))
    {
        Tier = Skill->Tier;
    }
    else if (const UEvolutionItemData *Evolution = Cast<UEvolutionItemData>(E.Asset))
    {
        Tier = Evolution->Tier;
    }
    else
    {
        return FText::GetEmpty();
    }
    return FText::FromString(TierHelpers::GetTierName(Tier));
}

FText UShopRowWidget::DescriptionFor(const FMerchantStockEntry &E)
{
    if (E.IsCrystalEntry())
    {
        // Tier-flavor sentence only — the mechanical effect surfaces as a NAME in the
        // detail panel's stats block, never as numbers (3e rule).
        return FText::FromString(CrystalDescription::GetCrystalText(E.Crystal));
    }
    if (const UEquipmentDataBase *Equipment = Cast<UEquipmentDataBase>(E.Asset)) // weapon + ring
    {
        return FText::FromString(Equipment->Description);
    }
    if (const USkillDataBase *Skill = Cast<USkillDataBase>(E.Asset)) // spell + ability
    {
        return FText::FromString(Skill->Description);
    }
    if (const UEvolutionItemData *Evolution = Cast<UEvolutionItemData>(E.Asset))
    {
        return FText::FromString(Evolution->Description); // variant-flavored, auto-generated
    }
    return FText::GetEmpty();
}

FText UShopRowWidget::GetEntryName() const
{
    const UShopEntryObject *Obj = EntryObject.Get();
    return Obj ? NameFor(Obj->Entry) : FText::GetEmpty();
}

FText UShopRowWidget::GetEntryTooltip() const
{
    const UShopEntryObject *Obj = EntryObject.Get();
    return Obj ? DescriptionFor(Obj->Entry) : FText::GetEmpty();
}

FText UShopRowWidget::GetEntryTier() const
{
    const UShopEntryObject *Obj = EntryObject.Get();
    return Obj ? TierFor(Obj->Entry) : FText::GetEmpty();
}

FText UShopRowWidget::GetEntryCount() const
{
    const UShopEntryObject *Obj = EntryObject.Get();
    return Obj ? FText::AsNumber(FMath::Max(1, Obj->Entry.Count)) : FText::GetEmpty();
}

FText UShopRowWidget::GetEntryCost() const
{
    const UShopEntryObject *Obj = EntryObject.Get();
    UEconomyService *Economy = GetEconomyService();
    if (!Obj || !Economy)
    {
        return FText::GetEmpty();
    }

    // One-entry cart through the SAME builder Purchase charges with — Count included.
    const FPurchaseCost Cost = Economy->PreviewCartCost({Obj->Entry});

    FString Line = FString::Printf(TEXT("%d P"), Cost.Prisms);
    if (Cost.SkillEssence > 0)
    {
        Line += FString::Printf(TEXT(" | Skill %d"), Cost.SkillEssence);
    }
    for (const TPair<EEssenceType, int32> &Pair : Cost.Typed)
    {
        Line += FString::Printf(TEXT(" | %s %d"),
                                *StaticEnum<EEssenceType>()->GetAuthoredNameStringByValue(static_cast<int64>(Pair.Key)),
                                Pair.Value);
    }
    return FText::FromString(Line);
}

void UShopRowWidget::HandleAddClicked()
{
    UShopEntryObject *Obj = EntryObject.Get();
    if (!Obj)
    {
        return;
    }
    if (UShopWindowWidget *Window = Obj->ParentWindow.Get())
    {
        // Window coalesces same-item lines and refreshes totals/Confirm.
        Window->AddToCart(Obj->Entry);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ShopRow] AddClicked with no ParentWindow stamped — ignored."));
    }
}

void UShopRowWidget::HandleRemoveClicked()
{
    UShopEntryObject *Obj = EntryObject.Get();
    if (!Obj)
    {
        return;
    }
    if (UShopWindowWidget *Window = Obj->ParentWindow.Get())
    {
        Window->RemoveFromCart(Obj->Entry);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ShopRow] RemoveClicked with no ParentWindow stamped — ignored."));
    }
}

void UShopRowWidget::RefreshRowTexts()
{
    if (RowName)
    {
        RowName->SetText(GetEntryName());
    }
    if (RowTier)
    {
        RowTier->SetText(GetEntryTier());
    }
    if (RowCount)
    {
        RowCount->SetText(GetEntryCount());
    }
    if (RowCost)
    {
        RowCost->SetText(GetEntryCost());
    }
    // 3e: no row tooltip — hover populates the window's detail panel instead.
}

void UShopRowWidget::NativeOnMouseEnter(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

    UShopEntryObject *Obj = EntryObject.Get();
    if (UShopWindowWidget *Window = Obj ? Obj->ParentWindow.Get() : nullptr)
    {
        Window->SetHoveredEntry(Obj);
    }
}

UEconomyService *UShopRowWidget::GetEconomyService() const
{
    UGameInstance *GI = GetGameInstance();
    return GI ? GI->GetSubsystem<UEconomyService>() : nullptr;
}
