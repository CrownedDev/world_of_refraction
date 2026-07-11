// MerchantShopSubsystem.cpp

#include "Shop/MerchantShopSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Merchant/MerchantData.h"

void UMerchantShopSubsystem::OpenForMerchant(UMerchantData *Merchant, APawn *Instigator)
{
    if (!Merchant || !Instigator)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MerchantShop] OpenForMerchant: null %s — ignored"),
               !Merchant ? TEXT("Merchant") : TEXT("Instigator"));
        return;
    }

    APlayerController *PC = Cast<APlayerController>(Instigator->GetController());
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MerchantShop] OpenForMerchant: %s has no PlayerController — ignored"),
               *Instigator->GetName());
        return;
    }

    if (IsOpen())
    {
        Close();
    }

    UE_LOG(LogTemp, Log, TEXT("[MerchantShop] Open: %s [%s] for %s"),
           *Merchant->GetName(),
           *UEnum::GetDisplayValueAsText(Merchant->MerchantType).ToString(),
           *Instigator->GetName());

    // NO_UI_YET (3a): the WBP lands in 3b. Skip the widget AND the modal bracket —
    // pausing with no window (and no Close button) would soft-lock the hub.
    UClass *WidgetClass = ShopWindowClass.LoadSynchronous();
    if (!WidgetClass)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[MerchantShop] ShopWindowClass not configured (NO_UI_YET) — shop window skipped."));
        return;
    }

    UUserWidget *Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
    if (!Widget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MerchantShop] CreateWidget failed for %s — shop window skipped."),
               *WidgetClass->GetName());
        return;
    }

    // State first: the widget reads GetActiveMerchant/GetActivePawn during Construct.
    ActiveWidget = Widget;
    ActiveMerchant = Merchant;
    ActivePawn = Instigator;
    Widget->AddToViewport();

    // Modal bracket — hub is real-time locomotion, so freeze it under the window.
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(Widget->TakeWidget());
    PC->SetInputMode(InputMode);
    PC->SetShowMouseCursor(true);
    UGameplayStatics::SetGamePaused(GetGameInstance(), true);
}

void UMerchantShopSubsystem::Close()
{
    if (!IsOpen())
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[MerchantShop] Close: %s"),
           ActiveMerchant.IsValid() ? *ActiveMerchant->GetName() : TEXT("<unknown merchant>"));

    if (UUserWidget *Widget = ActiveWidget.Get())
    {
        Widget->RemoveFromParent();
    }

    UGameplayStatics::SetGamePaused(GetGameInstance(), false);
    if (APawn *Pawn = ActivePawn.Get())
    {
        if (APlayerController *PC = Cast<APlayerController>(Pawn->GetController()))
        {
            PC->SetInputMode(FInputModeGameOnly());
            PC->SetShowMouseCursor(false);
        }
    }

    ActiveWidget = nullptr;
    ActiveMerchant = nullptr;
    ActivePawn = nullptr;
}
