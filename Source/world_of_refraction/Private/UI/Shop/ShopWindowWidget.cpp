// ShopWindowWidget.cpp

#include "UI/Shop/ShopWindowWidget.h"

#include "Components/Button.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Shop/MerchantShopSubsystem.h"

namespace ShopWindowConstants
{
    constexpr float TOAST_DURATION_SECONDS = 1.5f;
}

namespace
{
    FString EssenceName(EEssenceType Type)
    {
        return StaticEnum<EEssenceType>()->GetAuthoredNameStringByValue(static_cast<int64>(Type));
    }

    /** "Skill 30 | Fire 25 | Reality 12" over the currencies the cart needs — "—" if none. */
    FString BuildEssenceLine(const FPurchaseCost &Cost,
                             TFunctionRef<int32(ECurrencyType, uint8)> AmountOf)
    {
        TArray<FString> Parts;
        if (Cost.SkillEssence > 0)
        {
            Parts.Add(FString::Printf(TEXT("Skill %d"), AmountOf(ECurrencyType::SkillEssence, 0)));
        }
        for (const TPair<EEssenceType, int32> &Pair : Cost.Typed)
        {
            Parts.Add(FString::Printf(TEXT("%s %d"), *EssenceName(Pair.Key),
                                      AmountOf(ECurrencyType::EssenceTyped, static_cast<uint8>(Pair.Key))));
        }
        return Parts.Num() > 0 ? FString::Join(Parts, TEXT(" | ")) : TEXT("—");
    }

    bool CanAffordCost(const UCurrencyComponent &Currency, const FPurchaseCost &Cost)
    {
        if (!Currency.CanAfford(ECurrencyType::Prisms, Cost.Prisms))
        {
            return false;
        }
        if (Cost.SkillEssence > 0 && !Currency.CanAfford(ECurrencyType::SkillEssence, Cost.SkillEssence))
        {
            return false;
        }
        for (const TPair<EEssenceType, int32> &Pair : Cost.Typed)
        {
            if (!Currency.CanAfford(ECurrencyType::EssenceTyped, Pair.Value, static_cast<uint8>(Pair.Key)))
            {
                return false;
            }
        }
        return true;
    }

    /** Cart-line identity for RemoveFromCart: same Asset, or same CrystalId. */
    bool SameShopLine(const FMerchantStockEntry &A, const FMerchantStockEntry &B)
    {
        if (A.IsAssetEntry() != B.IsAssetEntry())
        {
            return false;
        }
        return A.IsAssetEntry() ? A.Asset == B.Asset : A.Crystal == B.Crystal;
    }
}

void UShopWindowWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetIsFocusable(true); // FInputModeUIOnly focuses this widget

    if (ConfirmButton)
    {
        ConfirmButton->OnClicked.AddUniqueDynamic(this, &UShopWindowWidget::HandleConfirmClicked);
    }
    if (CloseButton)
    {
        CloseButton->OnClicked.AddUniqueDynamic(this, &UShopWindowWidget::HandleCloseClicked);
    }
    if (ToastText)
    {
        ToastText->SetVisibility(ESlateVisibility::Collapsed);
    }

    // Self-resolve the data source: the subsystem stamps ActiveMerchant/ActivePawn BEFORE
    // AddToViewport, so Construct reads them without the subsystem knowing this type.
    if (UMerchantShopSubsystem *Shop = GetShopSubsystem())
    {
        if (!ActiveMerchant && Shop->GetActiveMerchant())
        {
            SetMerchant(Shop->GetActiveMerchant(), Shop->GetActivePawn());
        }
    }
}

void UShopWindowWidget::NativeDestruct()
{
    if (UCurrencyComponent *Currency = PurchaserCurrency.Get())
    {
        Currency->OnCurrencyChanged.RemoveDynamic(this, &UShopWindowWidget::HandleCurrencyChanged);
    }
    FTSTicker::GetCoreTicker().RemoveTicker(ToastTickerHandle);
    ToastTickerHandle.Reset();

    Super::NativeDestruct();
}

void UShopWindowWidget::SetMerchant(UMerchantData *Merchant, APawn *PurchaserPawn)
{
    if (UCurrencyComponent *Old = PurchaserCurrency.Get())
    {
        Old->OnCurrencyChanged.RemoveDynamic(this, &UShopWindowWidget::HandleCurrencyChanged);
    }

    ActiveMerchant = Merchant;
    Purchaser = PurchaserPawn;
    PurchaserCurrency = PurchaserPawn ? PurchaserPawn->FindComponentByClass<UCurrencyComponent>() : nullptr;

    if (UCurrencyComponent *Currency = PurchaserCurrency.Get())
    {
        Currency->OnCurrencyChanged.AddUniqueDynamic(this, &UShopWindowWidget::HandleCurrencyChanged);
    }
    else
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[ShopWindow] SetMerchant: %s has no CurrencyComponent — wallet blank, purchases will fail."),
               PurchaserPawn ? *PurchaserPawn->GetName() : TEXT("<null pawn>"));
    }

    Cart.Reset();
    RefreshHeader();
    RefreshStockList();
    RefreshCartList();
    RefreshCostAndWallet();
}

void UShopWindowWidget::AddToCart(const FMerchantStockEntry &Entry)
{
    if (!Entry.IsAssetEntry() && !Entry.IsCrystalEntry())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ShopWindow] AddToCart: empty entry ignored."));
        return;
    }
    Cart.Add(Entry);
    RefreshCartList();
    RefreshCostAndWallet();
}

void UShopWindowWidget::RemoveFromCart(const FMerchantStockEntry &Entry)
{
    const int32 Index = Cart.IndexOfByPredicate([&Entry](const FMerchantStockEntry &Line)
                                                { return SameShopLine(Line, Entry); });
    if (Index == INDEX_NONE)
    {
        return;
    }
    Cart.RemoveAt(Index);
    RefreshCartList();
    RefreshCostAndWallet();
}

void UShopWindowWidget::HandleConfirmClicked()
{
    if (Cart.Num() == 0)
    {
        return;
    }

    APawn *Pawn = Purchaser.Get();
    UEconomyService *Economy = GetEconomyService();
    if (!Pawn || !Economy)
    {
        ShowToast(NSLOCTEXT("Shop", "ShopUnavailable", "Shop unavailable"));
        return;
    }

    if (Economy->Purchase(Pawn, Cart))
    {
        Cart.Reset();
        RefreshCartList();
        RefreshCostAndWallet();
        ShowToast(NSLOCTEXT("Shop", "Purchased", "Purchased!"));
    }
    else
    {
        // Shouldn't fire — Confirm disables while unaffordable; capacity/duplicate edge
        // cases (e.g. inventory filled since the last refresh) land here.
        ShowToast(NSLOCTEXT("Shop", "PurchaseFailed", "Purchase failed"));
        RefreshCostAndWallet();
    }
}

void UShopWindowWidget::HandleCloseClicked()
{
    if (UMerchantShopSubsystem *Shop = GetShopSubsystem())
    {
        Shop->Close();
    }
}

void UShopWindowWidget::RefreshHeader()
{
    if (MerchantName)
    {
        MerchantName->SetText(ActiveMerchant ? FText::FromString(ActiveMerchant->GetName())
                                             : NSLOCTEXT("Shop", "NoMerchant", "<no merchant>"));
    }
    if (MerchantType)
    {
        MerchantType->SetText(ActiveMerchant ? UEnum::GetDisplayValueAsText(ActiveMerchant->MerchantType)
                                             : FText::GetEmpty());
    }
}

void UShopWindowWidget::RefreshStockList()
{
    StockObjects.Reset();
    if (!StockList)
    {
        return;
    }

    TArray<UObject *> Items;
    if (ActiveMerchant)
    {
        for (const FMerchantStockEntry &E : ActiveMerchant->Stock)
        {
            UShopEntryObject *Obj = NewObject<UShopEntryObject>(this);
            Obj->Entry = E;
            StockObjects.Add(Obj);
            Items.Add(Obj);
        }
    }
    StockList->SetListItems(Items);
}

void UShopWindowWidget::RefreshCartList()
{
    CartObjects.Reset();
    if (!CartList)
    {
        return;
    }

    TArray<UObject *> Items;
    for (const FMerchantStockEntry &E : Cart)
    {
        UShopEntryObject *Obj = NewObject<UShopEntryObject>(this);
        Obj->Entry = E;
        CartObjects.Add(Obj);
        Items.Add(Obj);
    }
    CartList->SetListItems(Items);
}

void UShopWindowWidget::RefreshCostAndWallet()
{
    UEconomyService *Economy = GetEconomyService();
    const FPurchaseCost Cost = Economy ? Economy->PreviewCartCost(Cart) : FPurchaseCost();

    if (CartTotalPrisms)
    {
        CartTotalPrisms->SetText(FText::AsNumber(Cost.Prisms));
    }
    if (CartTotalEssence)
    {
        // Cost side: amounts are the cart's own totals.
        CartTotalEssence->SetText(FText::FromString(BuildEssenceLine(
            Cost, [&Cost](ECurrencyType Type, uint8 SubKey)
            { return Type == ECurrencyType::SkillEssence ? Cost.SkillEssence
                                                         : Cost.Typed[static_cast<EEssenceType>(SubKey)]; })));
    }

    UCurrencyComponent *Currency = PurchaserCurrency.Get();
    if (WalletPrisms)
    {
        WalletPrisms->SetText(Currency ? FText::AsNumber(Currency->GetBalance(ECurrencyType::Prisms))
                                       : NSLOCTEXT("Shop", "NoWallet", "—"));
    }
    if (WalletEssence)
    {
        // Wallet side mirrors the SAME currencies (cart-relevant), showing balances.
        WalletEssence->SetText(Currency
                                   ? FText::FromString(BuildEssenceLine(
                                         Cost, [Currency](ECurrencyType Type, uint8 SubKey)
                                         { return Currency->GetBalance(Type, SubKey); }))
                                   : NSLOCTEXT("Shop", "NoWallet", "—"));
    }

    if (ConfirmButton)
    {
        ConfirmButton->SetIsEnabled(Currency && Cart.Num() > 0 && CanAffordCost(*Currency, Cost));
    }
}

void UShopWindowWidget::ShowToast(const FText &Message)
{
    if (!ToastText)
    {
        return;
    }
    ToastText->SetText(Message);
    ToastText->SetVisibility(ESlateVisibility::HitTestInvisible);

    // Core ticker, NOT a world timer: the hub is paused under the shop, so FTimerManager
    // would never fire the auto-hide.
    FTSTicker::GetCoreTicker().RemoveTicker(ToastTickerHandle);
    ToastTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateWeakLambda(this, [this](float)
                                          { HideToast(); return false; }),
        ShopWindowConstants::TOAST_DURATION_SECONDS);
}

void UShopWindowWidget::HideToast()
{
    if (ToastText)
    {
        ToastText->SetVisibility(ESlateVisibility::Collapsed);
    }
    ToastTickerHandle.Reset();
}

UEconomyService *UShopWindowWidget::GetEconomyService() const
{
    UGameInstance *GI = GetGameInstance();
    return GI ? GI->GetSubsystem<UEconomyService>() : nullptr;
}

UMerchantShopSubsystem *UShopWindowWidget::GetShopSubsystem() const
{
    UGameInstance *GI = GetGameInstance();
    return GI ? GI->GetSubsystem<UMerchantShopSubsystem>() : nullptr;
}

void UShopWindowWidget::HandleCurrencyChanged(ECurrencyType /*Currency*/, uint8 /*SubKey*/, int32 /*NewBalance*/)
{
    RefreshCostAndWallet();
}

FString UShopWindowWidget::GetShopString() const
{
    FString Result = FString::Printf(TEXT("ShopWindow — merchant: %s, purchaser: %s"),
                                     ActiveMerchant ? *ActiveMerchant->GetName() : TEXT("<none>"),
                                     Purchaser.IsValid() ? *Purchaser->GetName() : TEXT("<none>"));

    UEconomyService *Economy = GetEconomyService();
    const FPurchaseCost Cost = Economy ? Economy->PreviewCartCost(Cart) : FPurchaseCost();
    Result += FString::Printf(TEXT("\n  Cart (%d lines): %d Prisms, %d Skill"), Cart.Num(), Cost.Prisms, Cost.SkillEssence);
    for (const TPair<EEssenceType, int32> &Pair : Cost.Typed)
    {
        Result += FString::Printf(TEXT(", %d %s"), Pair.Value, *EssenceName(Pair.Key));
    }

    const UCurrencyComponent *Currency = PurchaserCurrency.Get();
    Result += FString::Printf(TEXT("\n  Affordable: %s"),
                              Currency && CanAffordCost(*Currency, Cost) ? TEXT("yes") : TEXT("no"));
    return Result;
}
