// VendorScreenModel.h
// B-456a (D-193, D-181, UIScreens #15 to #18, mockups screen_mockups/hub/vendors/): what the vendor screen shows,
// with no widget in it, so the specs read it headless. UVendorScreenWidget draws it.
//
// One shop screen, each vendor's services as tabs (D-193): Blacksmith: Buy, Sell, Dismantle; Jeweler: Buy, Sell,
// Item slots; Spiritualist: Buy, Sell, Repair, Curses, Dismantle, Merge. Every other vendor: Buy (UIScreens #15, "every
// vendor"). Built so far: Sell and Dismantle, whose rules and moves are WoRVendorServices' (Shop/VendorServices.h), and
// Buy (B-456b): the vendor's stock, a cart with who each line is for, bought through UEconomyService::PurchaseFor;
// Item slots (B-456c): a row per member's pouch, bought through ItemSlotUpgrade.
// A tab that isn't built yet is listed and says so. A row that can't be taken is listed with the reason, never hidden.
// Debug: wor.UI.PrintVendor logs GetVendorScreenString (each vendor's tabs and the party's rows).

#pragma once

#include "CoreMinimal.h"
#include "Shop/VendorServices.h"
#include "Skills/Definitions/ESpellElement.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "UI/Menus/PauseMenuModel.h"

class AActor;
class UMerchantData;

enum class EVendorScreenTab : uint8
{
	Buy,
	Sell,
	Dismantle,
	ItemSlots,
	Repair,
	Curses,
	Merge
};

/** One line of a tab: what the service acts on (Row; on Buy, Stock) and how it reads. */
struct FVendorScreenRow
{
	/** Sell and Dismantle: the thing taken. Buy: only Count, how many of the line the cart holds. */
	WoRVendorServices::FVendorRow Row;
	/** Buy: the vendor's stock line. */
	FMerchantStockEntry Stock;
	FText Name;
	FText Tier;
	/** "Weapon · Sword", "Crystal · Fire", "Evolution". */
	FText KindLine;
	ESpellElement Element = ESpellElement::None;
	/** What one of it gives: "100 Gold", "30 Gear Essence". Buy: what it costs, "260 Gold, 10 Pillar Essence". */
	FText Pay;
	/** Buy: the money alone ("260 Gold") when the cost has an essence too, for the row; else empty. */
	FText PayMoney;
	/** Buy: the detail panel's lines ("Power" / "42") and what the thing does. */
	TArray<FLoadoutScreenDetail> Details;
	FText Note;
	/** Why the service can't take it (shown in the kind line's place, the row greyed); empty when it can. */
	FText Refusal;
	/** Item slots: whose pouch the row is, the slots it has and the most it can have (the row's pips). */
	FName Member;
	int32 Slots = 0;
	int32 SlotsMax = 0;
};

struct FVendorScreenPage
{
	TArray<FVendorScreenRow> Rows;
	/** Why the tab has no rows (nothing to take, or the tab isn't built yet); empty when it has rows. */
	FText EmptyNote;
	/** What the tab is for, said above its rows; may be empty. */
	FText Intro;
	/** A rule of the service worth saying under the details; may be empty. */
	FText Rule;
};

/** One line of the Buy tab's cart: a stock line (its Count the units wanted) and who it is bought for. */
struct FVendorCartLine
{
	FMerchantStockEntry Stock;
	FName For;
	/** The receiver as the cart says it: a member's name, or "yourself". */
	FText ForName;
	FText Name;
};

/** The cart's foot. */
struct FVendorCartSummary
{
	/** "1,020 Gold", and each essence the cart charges. */
	FText Total;
	/** The place's currency left after the purchase: "7,400 Gold". */
	FText After;
	/** Why the cart can't be bought as it stands (the wallet is short); empty when it can, or when it is empty. */
	FText Refusal;
};

namespace WoRVendorScreen
{
	/** D-193: Vendor's service tabs, in order; a vendor D-193 doesn't name has Buy alone. */
	WORLD_OF_REFRACTION_API TArray<EVendorScreenTab> GetTabs(EMerchantType Vendor);
	WORLD_OF_REFRACTION_API FText GetTabName(EVendorScreenTab Tab);

	/** Whether the tab works on this screen yet (B-456 is built a tab group at a time). */
	WORLD_OF_REFRACTION_API bool IsBuilt(EVendorScreenTab Tab);

	/** The built tab's service; false for a tab that isn't a WoRVendorServices one. */
	WORLD_OF_REFRACTION_API bool GetService(EVendorScreenTab Tab, EVendorService &OutService);

	/** What the vendor deals in, under its name: "weapons, augment stones". */
	WORLD_OF_REFRACTION_API FText GetVendorLine(EMerchantType Vendor);

	/** A Sell or Dismantle tab's page (or a tab that isn't built yet). Buy is BuildBuyPage, Item slots
	 *  BuildItemSlotsPage (here: with no one to pay). */
	WORLD_OF_REFRACTION_API FVendorScreenPage BuildPage(UWorld *World, EMerchantType Vendor, EVendorScreenTab Tab, FName Member);

	/** B-456b: the Buy tab. Merchant's stock (this run's roll at a trial vendor), each line with its cost; a line
	 *  Payer's wallet can't buy for Member on top of Cart is greyed with the purchase's own reason. */
	WORLD_OF_REFRACTION_API FVendorScreenPage BuildBuyPage(UWorld *World, const UMerchantData *Merchant, AActor *Payer, FName Member,
														   const TArray<FVendorCartLine> &Cart);

	/** Puts Row's stock line in Cart for Member (gear and crystals for the same member stack). Empty when added; else
	 *  the row's reason. */
	WORLD_OF_REFRACTION_API FText AddToCart(AActor *Payer, const FVendorScreenRow &Row, const FLoadoutScreenMember &Member, TArray<FVendorCartLine> &Cart);

	/** Takes one press's worth of Row's line back out of Cart: Member's line first, else the last one put in for
	 *  anyone. False when the cart holds none of it. */
	WORLD_OF_REFRACTION_API bool RemoveFromCart(const FVendorScreenRow &Row, FName Member, TArray<FVendorCartLine> &Cart);

	WORLD_OF_REFRACTION_API FVendorCartSummary BuildCartSummary(UWorld *World, AActor *Payer, const TArray<FVendorCartLine> &Cart);

	/** "Ashen Blade x2, for Garrick: 1,240 Gold". */
	WORLD_OF_REFRACTION_API FText GetCartLineText(UWorld *World, const FVendorCartLine &Line);

	/** Buys Cart with Payer's wallet, one receiver's lines at a time (UEconomyService::PurchaseFor); what is bought
	 *  leaves Cart. Empty on success; else the reason, the lines not bought still in Cart. */
	WORLD_OF_REFRACTION_API FText PurchaseCart(UWorld *World, AActor *Payer, TArray<FVendorCartLine> &Cart);

	/** B-456c: the Jeweler's Item slots tab (mockup 2_jeweler_item_slots.png). A row per party member: the pouch's
	 *  slots and the next one's price; a row Payer's wallet can't buy is greyed with ItemSlotUpgrade's own reason. */
	WORLD_OF_REFRACTION_API FVendorScreenPage BuildItemSlotsPage(UWorld *World, AActor *Payer);

	/** "Buy an item slot for Sera for 300 Gold?" */
	WORLD_OF_REFRACTION_API FText GetItemSlotConfirmText(const FVendorScreenRow &Row);

	/** Buys the row's member one slot with Payer's wallet (ItemSlotUpgrade::TryBuy). Empty on success; else the reason. */
	WORLD_OF_REFRACTION_API FText BuyItemSlot(UWorld *World, AActor *Payer, const FVendorScreenRow &Row);

	/** The vendor of this kind standing in World (its stock is the Buy tab's), or null. */
	WORLD_OF_REFRACTION_API const UMerchantData *FindMerchant(const UWorld *World, EMerchantType Vendor);

	/** How many of Row the service takes on one press: the whole stack with the toggle on, else one. Gear: one. */
	WORLD_OF_REFRACTION_API int32 GetTakeCount(const FVendorScreenRow &Row, bool bWholeStack);

	/** The money shown top right: the place's currency and Gear Essence. */
	WORLD_OF_REFRACTION_API TArray<FPauseMenuChip> BuildChips(const UWorld *World);

	/** Payer: who would buy (the Buy rows' refusals); null lists the stock with no buyer. */
	WORLD_OF_REFRACTION_API FString GetVendorScreenString(UWorld *World, EMerchantType Vendor, AActor *Payer = nullptr);
}
