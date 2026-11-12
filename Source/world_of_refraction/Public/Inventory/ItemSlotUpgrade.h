// ItemSlotUpgrade.h
// B-239 (D-097, Q-239): the hub vendor's item slot upgrades. Each character's pouch (D-117) starts with
// InventoryConstants::ITEM_SLOTS_START slots; each purchase adds one, for hub Gold (ECurrencyType::Prisms, shown as
// Gold since D-159), at a price that rises with every slot bought, up to InventoryConstants::ITEM_SLOTS_MAX.
// Hub only: refused while a trial run is on. Numbers are first guesses: TODO: tune.
// Placeholder screen: UItemSlotUpgradeWidget (wor.UI.ItemSlots). Debug: GetUpgradeString.

#pragma once

#include "CoreMinimal.h"

class UCurrencyComponent;
class UObject;
class UPoolSubsystem;
enum class ECurrencyType : uint8;

namespace ItemSlotUpgrade
{
	/** The first upgrade's price in hub Gold. TODO: tune. */
	constexpr int32 BASE_PRICE = 200;

	/** How much each upgrade already bought adds to the next one's price. TODO: tune. */
	constexpr int32 PRICE_STEP = 100;

	/** What upgrades are paid in: hub Gold (ECurrencyType::Prisms, D-159 naming). */
	WORLD_OF_REFRACTION_API ECurrencyType GetCurrency();

	/** The price of the next slot for a pouch that has SlotCount slots now (BASE_PRICE + PRICE_STEP per upgrade
	 *  bought). */
	WORLD_OF_REFRACTION_API int32 GetPrice(int32 SlotCount);

	/** True while a pouch of SlotCount slots can still grow (below ITEM_SLOTS_MAX). */
	WORLD_OF_REFRACTION_API bool CanGrow(int32 SlotCount);

	/** The hub service only: false while a trial run is on (a world with no run subsystem counts as the hub). */
	WORLD_OF_REFRACTION_API bool IsAvailableHere(const UObject *Context);

	/** Buys one slot for CharacterKey's pouch from Wallet: checks the hub, the cap and the price, spends, adds the
	 *  slot. False with OutRefusal (player-facing) when refused; nothing is spent then. */
	WORLD_OF_REFRACTION_API bool TryBuy(const UObject *Context, UPoolSubsystem &Pool, FName CharacterKey, UCurrencyComponent *Wallet,
										FText &OutRefusal);

	/** Debug: "10/20 slots, next costs 200 Gold" or "20/20 slots (full)". */
	WORLD_OF_REFRACTION_API FString GetUpgradeString(const UPoolSubsystem &Pool, FName CharacterKey);
}
