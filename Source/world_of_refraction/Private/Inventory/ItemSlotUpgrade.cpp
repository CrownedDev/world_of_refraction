// ItemSlotUpgrade.cpp

#include "Inventory/ItemSlotUpgrade.h"

#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Inventory/InventoryConstants.h"
#include "Pool/PoolSubsystem.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRAuthority.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "ItemSlotUpgrade"

ECurrencyType ItemSlotUpgrade::GetCurrency()
{
	return ECurrencyType::Prisms;
}

int32 ItemSlotUpgrade::GetPrice(int32 SlotCount)
{
	const int32 Bought = FMath::Max(0, SlotCount - InventoryConstants::ITEM_SLOTS_START);
	return BASE_PRICE + PRICE_STEP * Bought;
}

bool ItemSlotUpgrade::CanGrow(int32 SlotCount)
{
	return SlotCount < InventoryConstants::ITEM_SLOTS_MAX;
}

bool ItemSlotUpgrade::IsAvailableHere(const UObject *Context)
{
	const UWorld *World = Context ? Context->GetWorld() : nullptr;
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	return !(Run && Run->IsRunActive());
}

FText ItemSlotUpgrade::GetRefusal(const UObject *Context, const UPoolSubsystem &Pool, FName CharacterKey, const UCurrencyComponent *Wallet)
{
	if (CharacterKey.IsNone())
	{
		return LOCTEXT("NoCharacter", "Choose a character first.");
	}
	if (!IsAvailableHere(Context))
	{
		return LOCTEXT("InRun", "Item slots are sold in the hub, not during a run.");
	}
	const int32 Slots = Pool.GetItemSlotCount(CharacterKey);
	if (!CanGrow(Slots))
	{
		return LOCTEXT("Full", "This pouch has every slot it can hold.");
	}
	if (!Wallet)
	{
		return LOCTEXT("NoWallet", "No wallet to pay from.");
	}
	if (!WoRAuthority::HasAuthority(Wallet->GetOwner()))
	{
		// Q-239 (Crown): host-only in co-op; a guest's own purchase would need a server RPC.
		return LOCTEXT("HostOnly", "Only the host can buy item slots.");
	}
	if (!Wallet->CanAfford(GetCurrency(), GetPrice(Slots)))
	{
		return FText::Format(LOCTEXT("TooPoor", "You need {0} {1}."), FText::AsNumber(GetPrice(Slots)), WoRGameNames::GetCurrencyName(GetCurrency()));
	}
	return FText::GetEmpty();
}

bool ItemSlotUpgrade::TryBuy(const UObject *Context, UPoolSubsystem &Pool, FName CharacterKey, UCurrencyComponent *Wallet,
							 FText &OutRefusal)
{
	OutRefusal = GetRefusal(Context, Pool, CharacterKey, Wallet);
	if (!OutRefusal.IsEmpty())
	{
		return false;
	}
	const int32 Price = GetPrice(Pool.GetItemSlotCount(CharacterKey));
	if (!Wallet->Spend(GetCurrency(), Price))
	{
		OutRefusal = LOCTEXT("SpendFailed", "The payment failed.");
		return false;
	}
	if (!Pool.AddItemSlot(CharacterKey))
	{
		Wallet->Add(GetCurrency(), Price); // can't happen after CanGrow; never keep the money for nothing
		OutRefusal = LOCTEXT("AddFailed", "The slot could not be added.");
		return false;
	}
	UE_LOG(LogWoR, Log, TEXT("[ItemSlotUpgrade] Bought a slot for %s for %d: %s"), *CharacterKey.ToString(), Price,
		   *GetUpgradeString(Pool, CharacterKey));
	return true;
}

FString ItemSlotUpgrade::GetUpgradeString(const UPoolSubsystem &Pool, FName CharacterKey)
{
	const int32 Slots = Pool.GetItemSlotCount(CharacterKey);
	return CanGrow(Slots) ? FString::Printf(TEXT("%d/%d slots, next costs %d"), Slots, InventoryConstants::ITEM_SLOTS_MAX, GetPrice(Slots))
						  : FString::Printf(TEXT("%d/%d slots (full)"), Slots, InventoryConstants::ITEM_SLOTS_MAX);
}

#undef LOCTEXT_NAMESPACE
