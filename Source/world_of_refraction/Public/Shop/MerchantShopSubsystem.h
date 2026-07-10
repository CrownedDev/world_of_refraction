// MerchantShopSubsystem.h
// Owns the hub shop window lifecycle (Cluster 3a): AMerchantInteractable::Interact()
// calls OpenForMerchant, which creates the shop widget, adds it to the viewport, and
// brackets the modal state (UIOnly input + cursor + pause — the hub is real-time);
// Close() unwinds all of it. Mirrors UCombatCommandMenuSubsystem only in OUTER shape
// (GameInstanceSubsystem + Open/Close/IsOpen + weak state refs) — unlike that data-only
// subsystem, this one owns the widget directly (modal window, single owner).
//
// No purchase logic here: the widget (Cluster 3b) calls UEconomyService::Purchase.
//
// ShopWindowClass comes from config (DefaultGame.ini [/Script/world_of_refraction.MerchantShopSubsystem])
// once 3b authors the WBP. Null class = NO_UI_YET fallback: Open logs and skips the
// widget (and the input/pause bracket, so PIE can never soft-lock without a Close path).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MerchantShopSubsystem.generated.h"

class APawn;
class UMerchantData;
class UUserWidget;

UCLASS(Config = Game)
class WORLD_OF_REFRACTION_API UMerchantShopSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Open the shop window for Merchant, instigated by Instigator (the pawn standing in
     *  the merchant's trigger). Closes any already-open shop first. Ignored (with a log)
     *  when Merchant/Instigator is null or Instigator has no PlayerController. */
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void OpenForMerchant(UMerchantData *Merchant, APawn *Instigator);

    /** Tear down the shop window and restore game input / cursor / unpause. Safe to call
     *  when nothing is open (no-op). The widget's Close button routes here. */
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void Close();

    UFUNCTION(BlueprintPure, Category = "Shop")
    bool IsOpen() const { return ActiveWidget.IsValid(); }

    /** The merchant whose shop is open, or null. The 3b widget reads its Stock from here. */
    UFUNCTION(BlueprintPure, Category = "Shop")
    UMerchantData *GetActiveMerchant() const { return ActiveMerchant.Get(); }

    /** The pawn shopping right now, or null. The 3b widget resolves wallet/inventory off it. */
    UFUNCTION(BlueprintPure, Category = "Shop")
    APawn *GetActivePawn() const { return ActivePawn.Get(); }

private:
    /** Shop window widget class — authored in Cluster 3b, pointed at via DefaultGame.ini.
     *  Null → NO_UI_YET: OpenForMerchant logs and skips widget + modal bracket. */
    UPROPERTY(Config)
    TSoftClassPtr<UUserWidget> ShopWindowClass;

    /** Weak: the viewport owns the widget's lifetime; never keep it alive past Close. */
    TWeakObjectPtr<UUserWidget> ActiveWidget;
    TWeakObjectPtr<UMerchantData> ActiveMerchant;
    TWeakObjectPtr<APawn> ActivePawn;
};
