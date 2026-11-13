// VendorScreenModel.h
// B-456a (D-193, D-181, UIScreens #15 to #18, mockups screen_mockups/hub/vendors/): what the vendor screen shows,
// with no widget in it, so the specs read it headless. UVendorScreenWidget draws it.
//
// One shop screen, each vendor's services as tabs (D-193): Blacksmith: Buy, Sell, Dismantle; Jeweler: Buy, Sell,
// Item slots; Spiritualist: Buy, Sell, Repair, Curses, Dismantle, Merge. Built so far: Sell and Dismantle, whose
// rules and moves are WoRVendorServices' (Shop/VendorServices.h). A tab that isn't built yet is listed and says so.
// A row that can't be taken is listed with the reason, never hidden.
// Debug: wor.UI.PrintVendor logs GetVendorScreenString (each vendor's tabs and the party's rows).

#pragma once

#include "CoreMinimal.h"
#include "Shop/VendorServices.h"
#include "Skills/Definitions/ESpellElement.h"
#include "UI/Menus/PauseMenuModel.h"

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

/** One line of a Sell or Dismantle tab: what the service acts on (Row) and how it reads. */
struct FVendorScreenRow
{
	WoRVendorServices::FVendorRow Row;
	FText Name;
	FText Tier;
	/** "Weapon · Sword", "Crystal · Fire", "Evolution". */
	FText KindLine;
	ESpellElement Element = ESpellElement::None;
	/** What one of it gives: "100 Gold", "30 Gear Essence". */
	FText Pay;
	/** Why the service can't take it (shown in the kind line's place, the row greyed); empty when it can. */
	FText Refusal;
};

struct FVendorScreenPage
{
	TArray<FVendorScreenRow> Rows;
	/** Why the tab has no rows (nothing to take, or the tab isn't built yet); empty when it has rows. */
	FText EmptyNote;
	/** A rule of the service worth saying under the details; may be empty. */
	FText Rule;
};

namespace WoRVendorScreen
{
	/** D-193: Vendor's service tabs, in order. Empty for a vendor with no shop screen. */
	WORLD_OF_REFRACTION_API TArray<EVendorScreenTab> GetTabs(EMerchantType Vendor);
	WORLD_OF_REFRACTION_API FText GetTabName(EVendorScreenTab Tab);

	/** Whether the tab works on this screen yet (B-456 is built a tab group at a time). */
	WORLD_OF_REFRACTION_API bool IsBuilt(EVendorScreenTab Tab);

	/** The built tab's service; false for a tab that isn't a WoRVendorServices one. */
	WORLD_OF_REFRACTION_API bool GetService(EVendorScreenTab Tab, EVendorService &OutService);

	/** What the vendor deals in, under its name: "weapons, augment stones". */
	WORLD_OF_REFRACTION_API FText GetVendorLine(EMerchantType Vendor);

	WORLD_OF_REFRACTION_API FVendorScreenPage BuildPage(UWorld *World, EMerchantType Vendor, EVendorScreenTab Tab, FName Member);

	/** How many of Row the service takes on one press: the whole stack with the toggle on, else one. Gear: one. */
	WORLD_OF_REFRACTION_API int32 GetTakeCount(const FVendorScreenRow &Row, bool bWholeStack);

	/** The money shown top right: the place's currency and Gear Essence. */
	WORLD_OF_REFRACTION_API TArray<FPauseMenuChip> BuildChips(const UWorld *World);

	WORLD_OF_REFRACTION_API FString GetVendorScreenString(UWorld *World, EMerchantType Vendor);
}
